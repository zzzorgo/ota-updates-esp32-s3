#include "ESPAsyncWebServer.h"
#include <Arduino.h>

AsyncWebSocketClient* webSocketClient;
AsyncWebSocket* wsc;

void initSmartLog(void* ws, void* wsclient) {
    webSocketClient = (AsyncWebSocketClient*) ws;
    wsc = (AsyncWebSocket*) wsclient;
}

void destroySmartLog() {
    wsc->closeAll();
    webSocketClient = nullptr;
}

void smartLog(const char* str, ...) {
    // Get the variadic arguments using va_list
    va_list args;
    va_start(args, str);

    // Print the formatted message using vsnprintf
    char buffer[256]; // Adjust the buffer size as needed
    vsnprintf(buffer, sizeof(buffer), str, args);

    if (webSocketClient != nullptr) {
        webSocketClient->printf(buffer);
    }

    printf(buffer);
    printf("\n");

    // Clean up the va_list
    va_end(args);
}
