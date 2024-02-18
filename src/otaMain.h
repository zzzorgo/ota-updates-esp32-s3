#ifndef __ESP_OTA_MAIN__
#define __ESP_OTA_MAIN__

struct OtaSecretKeys {
    const char* nvsNamespace;
    const char* wifiSsidNvsKey;
    const char* wifiPasswordNvsKey;
    const char* caCertNvsKey;
};

struct OtaSecretValues {
    const char* wifiSsid;
    const char* wifiPassword;
    const char* caCert;
};

void setupOta(OtaSecretKeys* secretKeys, OtaSecretValues* secretValues = nullptr);
void saveSecretsToNvs(OtaSecretKeys* secretKeys, OtaSecretValues* secretValues);

#endif // __ESP_OTA_MAIN__
