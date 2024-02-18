#include "ESPAsyncWebServer.h"
#include <Arduino.h>

AsyncWebSocketClient* webSocketClient;

void initSmartLog(void* ws) {
    webSocketClient = (AsyncWebSocketClient*) ws;
}

void destroySmartLog() {
    // webSocketClient->close();
    webSocketClient = nullptr;
}

void smartLog(const char* str) {
    if (webSocketClient != nullptr) {
        webSocketClient->printf(str);
    }
}
