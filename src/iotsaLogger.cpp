#include <Esp.h>

// Define this before including iotsaLogger

Print *iotsaOriginalSerial = &Serial;

#include "iotsaLogger.h"
#include "iotsaConfigFile.h"

#define BUFFER_MAGIC 0xaddedbed
static struct LogBuffer {
  uint32_t magic;
  uint32_t generation;
  uint32_t inp;
  uint32_t outp;
  uint8_t buffer[IOTSA_LOGGER_BUFFER_SIZE];
} *logBuffer;

class IotsaLogPrinter : public Print {
public:
  virtual size_t write(uint8_t ch);
};

static IotsaLogPrinter iotsaLogPrinter;

size_t
IotsaLogPrinter::write(uint8_t ch) {
  size_t rv = iotsaOriginalSerial->write(ch);
  if (logBuffer == 0) logBuffer = (struct LogBuffer *)malloc(sizeof(*logBuffer));
  if (logBuffer->magic != BUFFER_MAGIC || logBuffer->inp >= IOTSA_LOGGER_BUFFER_SIZE || logBuffer->outp >= IOTSA_LOGGER_BUFFER_SIZE) {
    // Buffer seems invalid. Re-initialize.
    logBuffer->magic = BUFFER_MAGIC;
    logBuffer->generation = 0;
    logBuffer->inp = 0;
    logBuffer->outp = 0;
  }
  logBuffer->buffer[logBuffer->inp++] = ch;
  if (logBuffer->inp >= IOTSA_LOGGER_BUFFER_SIZE) {
    logBuffer->inp = 0;
    logBuffer->generation++;
  }
  if (logBuffer->inp == logBuffer->outp) logBuffer->outp++;
  if (logBuffer->outp >= IOTSA_LOGGER_BUFFER_SIZE) logBuffer->outp = 0;
  return rv;
};

IotsaLoggerMod::IotsaLoggerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth)
: IotsaMod(_app, _auth, true)
{
  iotsaOverrideSerial = &iotsaLogPrinter;
  iotsaOverrideSerial->println("iotsa logger enabled");
}

void
IotsaLoggerMod::handler() {
  if (needsAuthentication("logger")) return;
  server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server->send(200, "text/plain");
  String msg;
  msg = "log generation="+String(logBuffer->generation)+", inp="+String(logBuffer->inp)+", outp="+String(logBuffer->outp)+"\n\n";
  server->sendContent(msg);
  if (logBuffer->inp == logBuffer->outp) return;
  if (logBuffer->inp > logBuffer->outp) {
    server->sendContent_P((char *)logBuffer->buffer+logBuffer->outp, logBuffer->inp-logBuffer->outp);
  } else {
    server->sendContent_P((char *)logBuffer->buffer+logBuffer->outp, IOTSA_LOGGER_BUFFER_SIZE-logBuffer->outp);
    if (logBuffer->inp) server->sendContent_P((char *)logBuffer->buffer, logBuffer->inp); 
  }
}

void IotsaLoggerMod::setup() {
  configLoad();
}

void IotsaLoggerMod::serverSetup() {
  server->on("/logger", std::bind(&IotsaLoggerMod::handler, this));
}

void IotsaLoggerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/logger.cfg");
  cf.get("argument", argument, "");
 
}

void IotsaLoggerMod::configSave() {
  IotsaConfigFileSave cf("/config/logger.cfg");
  cf.put("argument", argument);
}

void IotsaLoggerMod::loop() {
}

String IotsaLoggerMod::info() {
  String message = "<p>Built with logger. See <a href=\"/logger\">/logger</a> to see the log.</p>";
  return message;
}
