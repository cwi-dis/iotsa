#include <ESP.h>
#include "FS.h"
#include "iotsaFiles.h"

void IotsaFilesMod::setup() {
}

void
IotsaFilesMod::listHandler() {
  LED digitalWrite(led, 1);
  String message = "<html><head><title>Files</title></head><body><h1>Files</h1><ul>";
  Dir d = SPIFFS.openDir("/data");
  while (d.next()) {
      message += "<li><a href=\"" + d.fileName() + "\">" + d.fileName() + "</a> (" + String(d.fileSize()) + " bytes)</li>";
  }
  message += "</ul></body></html>";
  server.send(200, "text/html", message);
  LED digitalWrite(led, 0);
}

void
IotsaFilesMod::notFoundHandler() {
  LED digitalWrite(led, 1);
  String path = server.uri();
  File dataFile;
  if (SPIFFS.exists(path) && (dataFile = SPIFFS.open(path, "r"))) {
    String dataType = "text/plain";
    if(path.endsWith(".html")) dataType = "text/html";
    else if(path.endsWith(".css")) dataType = "text/css";
    else if(path.endsWith(".js")) dataType = "application/javascript";
    else if(path.endsWith(".png")) dataType = "image/png";
    else if(path.endsWith(".gif")) dataType = "image/gif";
    else if(path.endsWith(".jpg")) dataType = "image/jpeg";
    else if(path.endsWith(".ico")) dataType = "image/x-icon";
    else if(path.endsWith(".xml")) dataType = "text/xml";
    else if(path.endsWith(".pdf")) dataType = "application/pdf";
    else if(path.endsWith(".zip")) dataType = "application/zip";
    server.streamFile(dataFile, dataType);
    dataFile.close();
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += path;
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  LED digitalWrite(led, 0);
}

File _uploadFile;
bool _uploadOK;

void
IotsaFilesMod::uploadHandler() {
  LED digitalWrite(led, 1);
  HTTPUpload& upload = server.upload();
  _uploadOK = false;
  if(upload.status == UPLOAD_FILE_START){
    String _uploadfilename = "/data/" + upload.filename;
    if(SPIFFS.exists(_uploadfilename)) SPIFFS.remove(_uploadfilename);
    _uploadFile = SPIFFS.open(_uploadfilename, "w");
    //DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(_uploadFile) _uploadFile.write(upload.buf, upload.currentSize);
    //DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(_uploadFile) {
        _uploadFile.close();
        _uploadOK = true;
    }
    //DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
  LED digitalWrite(led, 0);
}

void
IotsaFilesMod::uploadOkHandler() {
  String message;
  if (_uploadOK) {
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "FAIL");
  }
}

void IotsaFilesMod::uploadFormHandler() {
  String message = "<form method='POST' action='/upload' enctype='multipart/form-data'>Select file to upload:<input type='file' name='blob'><br>Filename:<input name='filename'><br><input type='submit' value='Update'></form>";
  server.send(200, "text/html", message);
}
void IotsaFilesMod::serverSetup() {
  server.on("/upload", HTTP_POST, std::bind(&IotsaFilesMod::uploadOkHandler, this), std::bind(&IotsaFilesMod::uploadHandler, this));
  server.on("/upload", HTTP_GET, std::bind(&IotsaFilesMod::uploadFormHandler, this));
  server.on("/data", std::bind(&IotsaFilesMod::listHandler, this));
  server.onNotFound(std::bind(&IotsaFilesMod::notFoundHandler, this));
}

String IotsaFilesMod::info() {
  return "<p>See <a href=\"/data\">/data</a> for static files, <a href=\"/upload\">/upload</a> for uploading new files.</p>";
}

void IotsaFilesMod::loop() {
  
}
