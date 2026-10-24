#include <Esp.h>
#include "iotsaFiles.h"
#include "iotsaFS.h"

#ifdef IOTSA_HAS_WEBSERVER
void IotsaFilesMod::setup() {
}

#ifdef ESP32
// xxxjack: need to test whether this workaround is still needed (2022), I think the switch from spiffs to littlefs may have fixed it.

void
IotsaFilesMod::_listDir(String& message, const char *name)
{
  File d = IOTSA_FS.open(name);
  if (!d.isDirectory()) {
  	message += "<em>Not a directory: ";
  	message += name;
  	message += "</em>";
  	return;
  }
  message += "<ul>";
  File f = d.openNextFile();
  while (f) {
      // name() is only the last part of the name, path() the full one (cwi-dis/iotsa#306)
      message += "<li><a href=\"" + htmlEncode(f.path()) + "\">" + htmlEncode(f.name()) + "</a>";
      if (f.isDirectory()) {
      	message += ":";
      	_listDir(message, f.path());
      } else {
      	message += "(" + String(f.size()) + " bytes)";
      }
       
      message += "</li>";
      f = d.openNextFile();
  }
  message += "</ul>";
}

#else

void
IotsaFilesMod::_listDir(String& message, const char *name)
{
  message += "<ul>";
  String prefix = String(name) + "/";
  Dir d = IOTSA_FS.openDir(name);
  while (d.next()) {
      // fileName() is relative to the directory (cwi-dis/iotsa#306)
      String path = prefix + d.fileName();
      message += "<li><a href=\"" + htmlEncode(path) + "\">" + htmlEncode(d.fileName()) + "</a>";
      if (d.isDirectory()) {
      	message += ":";
      	_listDir(message, path.c_str());
      } else {
      	message += " (" + String(d.fileSize()) + " bytes)";
      }
      message += "</li>";
  }
  message += "</ul>";
}

#endif // ESP32

void
IotsaFilesMod::listHandler() {
  String message = "<html><head><title>Files</title></head><body><h1>Files</h1>";
  _listDir(message, "/data");
  message += "</body></html>";
  app.server->send(200, "text/html", message);
}

// A file under /data/ (the framework checked the right "readfiles").
void
IotsaFilesMod::fileHandler() {
  String path = app.server->uri();
  File dataFile;
  if (!IOTSA_FS.exists(path) || !(dataFile = IOTSA_FS.open(path, "r"))) {
    app.server->send(404, "text/plain", "File not found: " + path + "\n");
    return;
  }
  // Guess the data type, and stream the data back
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
  else if(path.endsWith(".bin")) dataType = "application/octet-stream";
  app.server->streamFile(dataFile, dataType);
  dataFile.close();
}

void IotsaFilesMod::lateSetup() {
  addWebHandler("/data", std::bind(&IotsaFilesMod::listHandler, this), "listfiles");
  addWebPrefixHandler("/data/", std::bind(&IotsaFilesMod::fileHandler, this), "readfiles");
  name = "files";
}

#ifdef IOTSA_WITH_WEB
String IotsaFilesMod::info() {
  return "<p>See <a href=\"/data\">/data</a> for static files.</p>";
}
#endif // IOTSA_WITH_WEB

void IotsaFilesMod::loop() {

}
#endif // IOTSA_HAS_WEBSERVER