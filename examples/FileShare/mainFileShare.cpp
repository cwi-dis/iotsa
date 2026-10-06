//
// Demonstrates the static-file modules together: upload a file, then fetch
// it back over HTTP. IotsaFilesMod serves whatever is in /data (and lists it
// at /data), IotsaFilesUploadMod adds an upload form at /upload,
// IotsaFilesBackupMod adds a full tar backup of all files at /backup.tar.
//

#include "iotsa.h"
#include "iotsaFiles.h"
#include "iotsaFilesUpload.h"
#include "iotsaFilesBackup.h"

IotsaApplication application("Iotsa FileShare Server");

IotsaFilesMod filesMod(application);
IotsaFilesUploadMod filesUploadMod(application);
IotsaFilesBackupMod filesBackupMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
