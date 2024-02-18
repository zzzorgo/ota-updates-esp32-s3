#ifndef __ESP_HTTP_SERVER__
#define __ESP_HTTP_SERVER__

void setupServer(void (*firmwareUpdate)(void));
void loopServer();

#endif // __ESP_HTTP_SERVER__
