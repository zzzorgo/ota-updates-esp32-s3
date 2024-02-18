#ifndef __ESP_OTA_MAIN__
#define __ESP_OTA_MAIN__

#include <vector>
#include "serverSetup.h"

struct OtaSecretKeys {
    const char* nvsNamespace;
    const char* wifiSsidNvsKey;
    const char* wifiPasswordNvsKey;
    const char* caCertNvsKey;
    const char* bikeMacNvsKey;
    const char* bikePassKeyNvsKey;
};

struct OtaSecretValues {
    const char* wifiSsid;
    const char* wifiPassword;
    const char* caCert;
    const char* bikeMac;
    const char* bikePassKey;
};

extern OtaSecretValues populatedOtaSecrets;

void setupOta(OtaSecretKeys* secretKeys, std::vector<OtaCommand> commands, OtaSecretValues* secretValues = nullptr);
void saveSecretsToNvs(OtaSecretKeys* secretKeys, OtaSecretValues* secretValues);

#endif // __ESP_OTA_MAIN__
