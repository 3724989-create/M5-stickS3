#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "globals.h"

#define FW_VERESION "0.1.0"
#define MODEL_NAME "stickS3"

void pump_Serial();
void sendHello();
void sendAck(bool ok,const char *msg);
void sendState();