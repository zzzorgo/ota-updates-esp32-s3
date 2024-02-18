#include "ESPAsyncWebServer.h"
#include <Arduino.h>

AsyncWebSocketClient* webSocketClient;

void initSmartLog(void* ws) {
    webSocketClient = (AsyncWebSocketClient*) ws;
}

void destroySmartLog() {
    webSocketClient->server()->closeAll();
}

void smartLog(const char* str, ...) {
    // Get the variadic arguments using va_list
    va_list args;
    va_start(args, str);

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), str, args);

    if (webSocketClient != nullptr) {
        webSocketClient->printf(buffer);
    }

    printf(buffer);
    printf("\n");

    va_end(args);
}
