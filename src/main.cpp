#include <Arduino.h>
#include "otaMain.h"

void setup() {
  OtaSecretKeys secretKeys = {
    .nvsNamespace = "credentials",
    .wifiSsidNvsKey = "username",
    .wifiPasswordNvsKey = "password",
    .caCertNvsKey = "cert"
  };

  setupOta(&secretKeys);
}

void loop() {
  delay(1000);
}
