#include <Esp.h>
#include "iotsaFilesBackup.h"
#include "iotsaFS.h"

#ifdef IOTSA_HAS_WEBSERVER
struct tarHeader {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char checksum[8];
	char islink;
	char link[100];
	char pad[255];
};

// The full path of every file, also in subdirectories (/config/wifi.cfg).
// The ESP32's File::name() is only the last part of the name; path() is the
// full one (cwi-dis/iotsa#306).
#ifdef ESP32
static void addFilenames(std::vector<String>& fileList, String dirName) {
	File root = IOTSA_FS.open(dirName);
	while(1) {
		File file = root.openNextFile();
		if (!file) break;
		String fileName(file.path());
		if (file.isDirectory()) {
			addFilenames(fileList, fileName);
		} else {
			fileList.push_back(fileName);
		}
	}
}
#else
static void addFilenames(std::vector<String>& fileList, String dirName) {
  String prefix = dirName.endsWith("/") ? dirName : dirName + "/";
  Dir d = IOTSA_FS.openDir(dirName);
  while (d.next()) {
	String fileName = prefix + d.fileName();
	if (d.isDirectory()) {
		addFilenames(fileList, fileName);
	} else {
		fileList.push_back(fileName);
	}
  }
}
#endif

static char buf[512];

static void octal(char *p, int size, unsigned int value)
{
	size--;
	p += size;
	*p-- = '\0';
	while (size--) {
		*p-- = '0' + (value & 7);
		value >>= 3;
	}
}

static void checksum(struct tarHeader *h) 
{
	int i;
	for(i=0; i<8; i++) h->checksum[i] = ' ';
	int checksum = 0;
	for(i=0; i<512; i++) {
		checksum += ((unsigned char *)h)[i];
	}
	octal(h->checksum, 7, checksum); // Note the 7 (not 8): last space remains
}

void IotsaFilesBackupMod::setup() {
}

// The tar holds every secret on the device (WiFi password, user passwords,
// the issuer key), which the API never returns. So, like changing those
// settings, it needs configuration mode (cwi-dis/iotsa#300).
void
IotsaFilesBackupMod::handler() {
  if (!iotsaController.inConfigurationMode()) {
    app.server->send(409, "text/plain", "The backup can only be downloaded in configuration mode.\n");
    return;
  }
  IFDEBUG IotsaSerial.println("Creating backup");
#ifdef CONTENT_LENGTH_UNKNOWN
  app.server->setContentLength(CONTENT_LENGTH_UNKNOWN);
#endif
  app.server->send(200, "application/x-tar");
  std::vector<String> fileNames;
  addFilenames(fileNames, "/");
  for(std::vector<String>::iterator it=fileNames.begin(); it != fileNames.end(); it++) {
  	const String& fileName = *it;
  	IFDEBUG IotsaSerial.print("File ");
  	IFDEBUG IotsaSerial.println(fileName);
  	File fp = IOTSA_FS.open(fileName, "r");
  	if (!fp) {
		  IotsaSerial.printf("Backup: cannot open %s\n", fileName.c_str());
		continue;
	}
  	//fileName = fileName.substring(1);
  	size_t fileSize = fp.size();
  	size_t filePadding = 512 - (fileSize & 511);
  	if (filePadding == 512) filePadding = 0;
  	IFDEBUG IotsaSerial.print("  size=");
  	IFDEBUG IotsaSerial.println(fileSize);
  	
  	// Write header
  	memset(buf, '\0', 512);
  	struct tarHeader *tarHeader = (struct tarHeader *)buf;
  	strncpy(tarHeader->name, fileName.c_str()+1, 99);  // Remove leading slash
  	octal(tarHeader->mode, 8, 0777);
  	octal(tarHeader->uid, 8, 0);
  	octal(tarHeader->gid, 8, 0);
  	octal(tarHeader->size, 12, fileSize);
  	octal(tarHeader->mtime, 12, 0);
  	tarHeader->islink = '0';
  	memset(tarHeader->link, '\0', 100);
  	memset(tarHeader->pad, '\0', 255);
  	checksum(tarHeader);
  	
  	app.server->sendContent_P(buf, 512);
  	// Write data
  	while (fileSize > 0) {
	  	int curLen = fp.read((uint8_t *)buf, 512);
	  	if (curLen <= 0) {
	  		// Short read: previously this looped forever (curLen -1 grew fileSize)
	  		// until the watchdog fired, see cwi-dis/iotsa#51. Zero-fill the rest
	  		// so the tar stream stays aligned with the size in the header.
	  		IotsaSerial.printf("Backup: short read on %s\n", fileName.c_str());
	  		memset(buf, '\0', 512);
	  		curLen = fileSize < 512 ? fileSize : 512;
	  	}
	  	app.server->sendContent_P(buf, curLen);
	  	fileSize -= curLen;
	  	iotsaController.feedWatchdog();   // a large file over a slow link takes long (cwi-dis/iotsa#244)
	  	yield();
	}
	fp.close();
  	// Write padding
  	if (filePadding) {
	  	memset(buf, '\0', filePadding);
  		app.server->sendContent_P(buf, filePadding);
	}
  }
}

void IotsaFilesBackupMod::lateSetup() {
  addWebHandler("/backup.tar", std::bind(&IotsaFilesBackupMod::handler, this), "backupfiles");
  name = "filesbackup";
}

#ifdef IOTSA_WITH_WEB
String IotsaFilesBackupMod::info() {
  return "<p>Download <a href=\"/backup.tar\">/backup.tar</a> for a backup of all files (in configuration mode).</p>";
}
#endif // IOTSA_WITH_WEB

void IotsaFilesBackupMod::loop() {

}
#endif // IOTSA_HAS_WEBSERVER