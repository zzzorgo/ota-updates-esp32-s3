#define ARDUHAL_LOG_LEVEL_DEBUG 5
#include <Arduino.h>
#include <esp_log.h>

static const char *TAG = "simple_ota_example";

void setup() {
  Serial.begin(115200);
}

void loop() {
  // Serial.println("Hello!");
  ESP_LOGI(TAG, "World");
}
