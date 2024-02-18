#ifndef __ESP_SMART_LOGGER__
#define __ESP_SMART_LOGGER__

void initSmartLog(void* ws);
void destroySmartLog();
void smartLog(const char* str, ...);

#endif // __ESP_SMART_LOGGER__
