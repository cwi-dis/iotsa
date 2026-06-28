import io
import os
import struct
import urllib.request
import zlib
from typing import Optional, List

# otadata struct layout and CRC formula sourced from:
#   ~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/
#   include/bootloader_support/include/esp_flash_partitions.h
#
# typedef struct {
#     uint32_t ota_seq;
#     uint8_t  seq_label[20];
#     uint32_t ota_state;
#     uint32_t crc;  /* CRC32 of ota_seq field only */
# } esp_ota_select_entry_t;   // 32 bytes total
#
# CRC = esp_rom_crc32_le(UINT32_MAX, &entry.ota_seq, 4)
# Python equivalent (verified empirically against real device otadata):
#   zlib.crc32(struct.pack('<I', ota_seq), 0xFFFFFFFF) & 0xFFFFFFFF

OTADATA_ENTRY_BYTES = 32
OTADATA_STATE_OFFSET = 24
OTADATA_CRC_OFFSET = 28

OTA_STATES = {
    0: 'new',
    1: 'pending_verify',
    2: 'valid',
    3: 'invalid',
    4: 'aborted',
    0xFFFFFFFF: 'undefined',
}

import esptool
import esptool.cmds as esptool_cmds

from .consts import IotsaError, VERBOSE

PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SIZE = 0xC00
PARTITION_ENTRY_SIZE = 32
PARTITION_MAGIC = 0x50AA
PARTITION_MD5_MAGIC = 0xEBEB

_APP_SUBTYPES: dict[int, str] = {0: 'factory', **{0x10 + i: f'ota_{i}' for i in range(16)}}
_DATA_SUBTYPES: dict[int, str] = {0: 'otadata', 1: 'phy', 2: 'nvs', 3: 'coredump', 0x80: 'spiffs', 0x81: 'fat', 0x82: 'littlefs', 0x83: 'nvs_keys'}
_TYPE_NAMES: dict[int, str] = {0: 'app', 1: 'data'}


class PartitionEntry:
    def __init__(self, name: str, type_name: str, subtype_name: str, offset: int, size: int, flags: int):
        self.name = name
        self.type = type_name
        self.subtype = subtype_name
        self.offset = offset
        self.size = size
        self.flags = flags

    def __repr__(self) -> str:
        return (f"PartitionEntry(name={self.name!r}, type={self.type!r}, "
                f"subtype={self.subtype!r}, offset=0x{self.offset:x}, size=0x{self.size:x})")


class DFU:
    """Handle iotsa board connected via USB or serial port.

    Wraps esptool v5 API for flash read/write/erase and partition-aware operations.
    Connect device via USB. For a bricked device, hold PRG while pressing RST before
    running any command.
    """

    def __init__(self, port: Optional[str] = None):
        self.port = port
        self._esp: Optional[esptool.ESPLoader] = None
        self._partition_table: Optional[List[PartitionEntry]] = None

    def _connect(self) -> esptool.ESPLoader:
        """Connect to device (lazy, reuses existing connection)."""
        if self._esp is not None:
            return self._esp
        if self.port:
            ser_list = [self.port]
            port = self.port
        else:
            ser_list = esptool.get_port_list()
            if not ser_list:
                raise IotsaError(
                    "No serial ports found. Connect device via USB or specify --serial."
                )
            if len(ser_list) > 1 and VERBOSE:
                print(f"Multiple serial ports: {ser_list}, using {ser_list[0]}")
            port = ser_list[0]
        if VERBOSE:
            print(f"+ Connecting to device on {port}")
        try:
            esp = esptool.get_default_connected_device(
                ser_list,
                port=port,
                connect_attempts=3,
                initial_baud=115200,
            )
        except esptool.FatalError as e:
            raise IotsaError(
                f"Could not connect to device on {port}: {e}\n"
                "For a bricked device: hold PRG while pressing RST, then release PRG."
            )
        if esp is None:
            raise IotsaError(f"Could not connect to device on {port}.")
        esp = esptool.run_stub(esp)
        self._esp = esp
        return esp

    def close(self) -> None:
        """Close the serial connection."""
        if self._esp is not None:
            try:
                self._esp._port.close()
            except Exception:
                pass
            self._esp = None
        self._partition_table = None

    def _download(self, filename: str) -> str:
        """Download URL to a temp file if needed; return local path."""
        if not os.path.exists(filename):
            if VERBOSE:
                print(f"+ Downloading {filename}")
            filename, _ = urllib.request.urlretrieve(filename)
        return filename

    # --- Chip info ---

    def getChipInfo(self) -> dict:
        """Return chip identification as a dict (name, description, features, revision, crystal, mac)."""
        esp = self._connect()
        mac_bytes = esp.read_mac("BASE_MAC")
        mac_str = ':'.join(f'{b:02x}' for b in mac_bytes)
        revision = esp.get_chip_revision()
        return {
            'name': esp.CHIP_NAME,
            'description': esp.get_chip_description(),
            'features': esp.get_chip_features(),
            'revision': revision,
            'crystal_mhz': esp.get_crystal_freq(),
            'mac': mac_str,
        }

    # --- Flash size ---

    def getFlashSize(self) -> int:
        """Return flash size in bytes."""
        esp = self._connect()
        size_str = esptool.detect_flash_size(esp)
        if size_str is None:
            raise IotsaError("Could not detect flash size")
        for suffix, mult in [('MB', 1024 * 1024), ('KB', 1024)]:
            if size_str.endswith(suffix):
                return int(size_str[:-len(suffix)]) * mult
        raise IotsaError(f"Unrecognised flash size string: {size_str!r}")

    # --- Partition table ---

    def getPartitionTable(self) -> List[PartitionEntry]:
        """Read and parse partition table from device. Cached after first read."""
        if self._partition_table is not None:
            return self._partition_table
        esp = self._connect()
        data = esptool_cmds.read_flash(esp, PARTITION_TABLE_OFFSET, PARTITION_TABLE_SIZE)
        assert data is not None
        self._partition_table = self._parsePartitionTable(data)
        return self._partition_table

    def _parsePartitionTable(self, data: bytes) -> List[PartitionEntry]:
        entries = []
        for i in range(0, len(data), PARTITION_ENTRY_SIZE):
            chunk = data[i:i + PARTITION_ENTRY_SIZE]
            if len(chunk) < PARTITION_ENTRY_SIZE:
                break
            magic = struct.unpack_from('<H', chunk, 0)[0]
            if magic == 0xFFFF:
                break
            if magic == PARTITION_MD5_MAGIC:
                continue
            if magic != PARTITION_MAGIC:
                break
            ptype = chunk[2]
            subtype = chunk[3]
            offset = struct.unpack_from('<I', chunk, 4)[0]
            size = struct.unpack_from('<I', chunk, 8)[0]
            name = chunk[12:28].rstrip(b'\x00').decode('ascii', errors='replace')
            flags = struct.unpack_from('<I', chunk, 28)[0]
            type_name = _TYPE_NAMES.get(ptype, f'type{ptype}')
            if ptype == 0:
                subtype_name = _APP_SUBTYPES.get(subtype, f'subtype{subtype:#x}')
            elif ptype == 1:
                subtype_name = _DATA_SUBTYPES.get(subtype, f'subtype{subtype:#x}')
            else:
                subtype_name = f'subtype{subtype:#x}'
            entries.append(PartitionEntry(name, type_name, subtype_name, offset, size, flags))
        return entries

    def findPartition(self, name: str) -> PartitionEntry:
        """Find partition by name or subtype. Raises IotsaError if not found."""
        table = self.getPartitionTable()
        for p in table:
            if p.name == name:
                return p
        for p in table:
            if p.subtype == name:
                return p
        available = [p.name for p in table]
        raise IotsaError(f"Partition {name!r} not found. Available: {available}")

    # --- Low-level read/write/erase ---

    def readFlash(self, offset: int, size: int, filename: Optional[str] = None) -> Optional[bytes]:
        """Read flash region. Returns bytes if filename is None, else writes to file."""
        esp = self._connect()
        return esptool_cmds.read_flash(esp, offset, size, output=filename)

    def writeFlash(self, offset: int, filename: str) -> None:
        """Write file to flash at given byte offset."""
        filename = self._download(filename)
        esp = self._connect()
        esptool_cmds.write_flash(esp, [(offset, filename)])

    def eraseFlash(self) -> None:
        """Erase entire flash."""
        esp = self._connect()
        esptool_cmds.erase_flash(esp)

    def eraseRegion(self, offset: int, size: int) -> None:
        """Erase a specific flash region."""
        esp = self._connect()
        esptool_cmds.erase_region(esp, offset, size)

    # --- Whole-flash backup/restore ---

    def backupFlash(self, filename: str) -> None:
        """Read entire flash to file."""
        size = self.getFlashSize()
        if VERBOSE:
            print(f"+ Reading {size // (1024 * 1024)}MB flash to {filename!r}")
        self.readFlash(0, size, filename=filename)
        print(f"Saved {size} bytes to {filename!r}")

    def restoreFlash(self, filename: str) -> None:
        """Write file to flash starting at offset 0 (paired with backupFlash)."""
        self.writeFlash(0, filename)

    # --- Partition-aware operations ---

    def readPartition(self, name: str, filename: Optional[str] = None) -> Optional[bytes]:
        """Read named partition. Returns bytes if filename is None, else writes to file."""
        p = self.findPartition(name)
        if VERBOSE:
            print(f"+ Reading partition {p.name!r} (offset=0x{p.offset:x} size=0x{p.size:x})")
        return self.readFlash(p.offset, p.size, filename=filename)

    def writePartition(self, name: str, filename: str) -> None:
        """Write file to named partition."""
        p = self.findPartition(name)
        if VERBOSE:
            print(f"+ Writing to partition {p.name!r} at offset 0x{p.offset:x}")
        self.writeFlash(p.offset, filename)

    def erasePartition(self, name: str) -> None:
        """Erase named partition."""
        p = self.findPartition(name)
        if VERBOSE:
            print(f"+ Erasing partition {p.name!r} (offset=0x{p.offset:x} size=0x{p.size:x})")
        self.eraseRegion(p.offset, p.size)

    def flashApp(self, filename: str) -> None:
        """Flash app binary to ota_0 partition."""
        self.writePartition('ota_0', filename)

    # --- OTA data ---

    def _otadata_crc(self, ota_seq: int) -> int:
        """CRC32 over the 4-byte ota_seq, as esp_rom_crc32_le(UINT32_MAX, &ota_seq, 4)."""
        return zlib.crc32(struct.pack('<I', ota_seq), 0xFFFFFFFF) & 0xFFFFFFFF

    def _parse_otadata_entry(self, data: bytes) -> dict:
        """Parse one 32-byte otadata entry from a 4096-byte sector."""
        ota_seq   = struct.unpack_from('<I', data, 0)[0]
        ota_state = struct.unpack_from('<I', data, OTADATA_STATE_OFFSET)[0]
        stored_crc = struct.unpack_from('<I', data, OTADATA_CRC_OFFSET)[0]
        if ota_seq == 0xFFFFFFFF or ota_seq == 0:
            return {'erased': True, 'valid': False}
        computed_crc = self._otadata_crc(ota_seq)
        return {
            'erased': False,
            'valid': stored_crc == computed_crc,
            'ota_seq': ota_seq,
            'ota_state': ota_state,
            'ota_state_name': OTA_STATES.get(ota_state, f'unknown({ota_state:#010x})'),
            'crc_ok': stored_crc == computed_crc,
        }

    def getOtadata(self) -> List[dict]:
        """Read and parse both otadata entries (one per 4096-byte sector)."""
        data = self.readPartition('otadata')
        assert data is not None
        return [
            self._parse_otadata_entry(data[0:]),
            self._parse_otadata_entry(data[4096:]),
        ]

    def getActiveOtaSlot(self) -> str:
        """Return subtype name of the partition that will boot next (e.g. 'ota_0', 'factory')."""
        entries = self.getOtadata()
        table = self.getPartitionTable()
        ota_parts = [p for p in table if p.type == 'app' and p.subtype.startswith('ota_')]
        n = len(ota_parts)
        best_seq = max(
            (e['ota_seq'] for e in entries if e.get('valid')),
            default=0
        )
        if best_seq == 0 or n == 0:
            factory = next((p for p in table if p.type == 'app' and p.subtype == 'factory'), None)
            return 'factory' if factory else 'ota_0'
        return f'ota_{(best_seq - 1) % n}'

    def setOtaSlot(self, slot_name: str) -> None:
        """Rewrite otadata to make the named OTA slot active on next boot."""
        table = self.getPartitionTable()
        ota_parts = [p for p in table if p.type == 'app' and p.subtype.startswith('ota_')]
        slot_idx = next(
            (i for i, p in enumerate(ota_parts) if p.subtype == slot_name or p.name == slot_name),
            None
        )
        if slot_idx is None:
            available = [p.subtype for p in ota_parts]
            raise IotsaError(f"OTA slot {slot_name!r} not found. Available: {available}")
        ota_seq = slot_idx + 1
        entry = bytearray(b'\xff' * 4096)
        struct.pack_into('<I', entry, 0, ota_seq)
        entry[4:24] = b'\x00' * 20         # seq_label: zeros (no SHA-256 hash)
        struct.pack_into('<I', entry, OTADATA_STATE_OFFSET, 2)  # ota_state: valid
        struct.pack_into('<I', entry, OTADATA_CRC_OFFSET, self._otadata_crc(ota_seq))
        self.erasePartition('otadata')
        p = self.findPartition('otadata')
        esp = self._connect()
        esptool_cmds.write_flash(esp, [(p.offset, io.BytesIO(bytes(entry)))])
        if VERBOSE:
            print(f"+ otadata set: {slot_name} (ota_seq={ota_seq})")

    def clearOtadata(self) -> None:
        """Erase otadata so bootloader defaults to factory (or ota_0 if no factory)."""
        self.erasePartition('otadata')

    # --- Raw passthrough ---

    def dfuTool(self, args: List[str]) -> None:
        """Pass arguments directly to esptool (v5 hyphen-style command names)."""
        command: List[str] = []
        if self.port:
            command += ['--port', self.port]
        command += args
        if VERBOSE:
            print(f"+ esptool {' '.join(command)}")
        try:
            esptool.main(command)
        except esptool.FatalError as e:
            raise IotsaError(str(e))
