#include <Esp.h>
#include "FS.h"
#include "iotsaFilesBackup.h"

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

void
IotsaFilesBackupMod::handler() {
  if (needsAuthentication("backupfiles")) return;
#ifdef ESP32
  server.send(404, "text/plain", "404 Not Found: cannot list files on esp32 yet");
#else
  IFDEBUG IotsaSerial.println("Creating backup");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/x-tar");
  
  Dir d = SPIFFS.openDir("/");
  while (d.next()) {
  	// Get header information
  	const String& fileName = d.fileName();
  	IFDEBUG IotsaSerial.print("File ");
  	IFDEBUG IotsaSerial.println(fileName);
  	File fp = SPIFFS.open(fileName, "r");
  	if (!fp) continue;
  	//fileName = fileName.substring(1);
  	size_t fileSize = d.fileSize();
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
  	
  	server.sendContent_P(buf, 512);
  	// Write data
  	while (fileSize > 0) {
	  	int curLen = fp.read((uint8_t *)buf, 512);
	  	server.sendContent_P(buf, curLen);
	  	fileSize -= curLen;
	}
	fp.close();
  	// Write padding
  	if (filePadding) {
	  	memset(buf, '\0', filePadding);
  		server.sendContent_P(buf, filePadding);
	}
  }
#endif
}

void IotsaFilesBackupMod::serverSetup() {
  server.on("/backup.tar", std::bind(&IotsaFilesBackupMod::handler, this));
}

String IotsaFilesBackupMod::info() {
  return "<p>Download <a href=\"/backup.tar\">/backup.tar</a> for a backup of all files.</p>";
}

void IotsaFilesBackupMod::loop() {
  
}
