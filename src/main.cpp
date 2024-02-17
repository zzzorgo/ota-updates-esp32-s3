#define CONFIG_BOOTLOADER_LOG_LEVEL 5
#define CONFIG_ARDUHAL_ESP_LOG 5
#define CONFIG_LOG_DEFAULT_LEVEL 5
#include <Arduino.h>
#include <sdkconfig.h>
#include <esp_log.h>

static const char *TAG = "simple_ota_example";

void setup() {
  Serial.begin(115200);
}

void loop() {
  // Serial.println("Hello!");
  ESP_LOGI(TAG, "World");
}
