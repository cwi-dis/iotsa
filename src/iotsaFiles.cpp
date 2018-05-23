#include <Esp.h>
#include "FS.h"
#ifdef ESP32
#include <SPIFFS.h>
#endif
#include "iotsaFiles.h"

#ifdef IOTSA_WITH_WEB
void IotsaFilesMod::setup() {
}

#ifdef ESP32

void
IotsaFilesMod::_listDir(String& message, const char *name)
{
  File d = SPIFFS.open(name);
  if (!d.isDirectory()) {
  	message += "<em>Not a directory: ";
  	message += name;
  	message += "</em>";
  	return;
  }
  message += "<ul>";
  File f = d.openNextFile();
  while (f) {
      message += "<li><a href=\"" + htmlEncode(f.name()) + "\">" + htmlEncode(f.name()) + "</a>";
      if (f.isDirectory()) {
      	message += ":";
      	_listDir(message, f.name());
      } else {
      	message += "(" + String(d.size()) + " bytes)";
      }
       
      message += "</li>";
  }
  message += "</ul>";
}

#else

void
IotsaFilesMod::_listDir(String& message, const char *name)
{
  message += "<ul>";
  Dir d = SPIFFS.openDir(name);
  while (d.next()) {
      message += "<li><a href=\"" + htmlEncode(d.fileName()) + "\">" + htmlEncode(d.fileName()) + "</a> (" + String(d.fileSize()) + " bytes)</li>";
  }
  message += "</ul>";
}

#endif // ESP32

void
IotsaFilesMod::listHandler() {
  if (needsAuthentication("listfiles")) return;
  String message = "<html><head><title>Files</title></head><body><h1>Files</h1>";
  _listDir(message, "/data");
  message += "</body></html>";
  server->send(200, "text/html", message);
}

bool
IotsaFilesMod::accessAllowed(String& path)
{
  return path.startsWith("/data/");
}

void
IotsaFilesMod::notFoundHandler() {
  String message = "File Not Found\n\n";
  String path = server->uri();
  File dataFile;
  if (!accessAllowed(path)) {
  	// Path doesn't refer to a file that may be accessed
  	message = "Access Not Allowed\n\n";
  } else if (!SPIFFS.exists(path)) {
  	// Path may be accessed, but doesn't exist
  	message = "File Does Not Exist\n\n";
  } else {
   	if (needsAuthentication("readfiles")) {
   		// Path may be accessed, and exists, but authentication is needed.
   		// Note we return, needsAuthentication() has filled in headers and such.
   		return;
   	}
  	
    if (dataFile = SPIFFS.open(path, "r")) {
    	// Everything is fine. Guess data type, and stream data back
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
		server->streamFile(dataFile, dataType);
		dataFile.close();
		return;
	} else {
		// File exists but cannot be opened. Should not happen.
		message = "Cannot Open File\n\n";
	}
  }
  // We get here in case of errors. Provide some more details.
  message += "URI: ";
  message += path;
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i=0; i<server->args(); i++){
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
}

void IotsaFilesMod::serverSetup() {
  server->on("/data", std::bind(&IotsaFilesMod::listHandler, this));
  server->onNotFound(std::bind(&IotsaFilesMod::notFoundHandler, this));
}

String IotsaFilesMod::info() {
  return "<p>See <a href=\"/data\">/data</a> for static files.</p>";
}

void IotsaFilesMod::loop() {
  
}
#endif // IOTSA_WITH_WEB