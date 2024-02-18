#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <WiFi.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#include <vector>

#include "sdkconfig.h"
#include "serverSetup.h"
#include "smartLogger.h"
#include "otaMain.h"

#define HASH_LEN 32

int loadedBytes = 0;
int64_t lastDataNotificationTime = 0;

char username[32];
char password[32];
char caCert[4096];
char mac[32];
char passKey[7];

OtaSecretValues populatedOtaSecrets = {
    .wifiSsid = username,
    .wifiPassword = password,
    .caCert = caCert,
    .bikeMac = mac,
    .bikePassKey = passKey,
};

void readValueFromNvs(nvs_handle_t* nvsHandle, const char* key, char* output) {
    size_t requiredSize;
    esp_err_t returnStatus = nvs_get_str(*nvsHandle, key, NULL, &requiredSize);

    if (returnStatus == ESP_OK)
    {
        nvs_get_str(*nvsHandle, key, output, &requiredSize);
    }
    else
    {
        smartLog("Error reading %s (%s)!\n", key, esp_err_to_name(returnStatus));
    }
}

void loadSecretsFromNvs(OtaSecretKeys *secretKeys)
{
    nvs_handle_t nvsHandle;
    esp_err_t returnStatus;

    returnStatus = nvs_open(secretKeys->nvsNamespace, NVS_READONLY, &nvsHandle);

    if (returnStatus != ESP_OK)
    {
        smartLog("Error (%s) opening NVS handle!\n", esp_err_to_name(returnStatus));
        return;
    }

    readValueFromNvs(&nvsHandle, secretKeys->wifiSsidNvsKey, username);
    readValueFromNvs(&nvsHandle, secretKeys->wifiPasswordNvsKey, password);
    readValueFromNvs(&nvsHandle, secretKeys->caCertNvsKey, caCert);
    readValueFromNvs(&nvsHandle, secretKeys->bikeMacNvsKey, mac);
    readValueFromNvs(&nvsHandle, secretKeys->bikePassKeyNvsKey, passKey);

    populatedOtaSecrets.wifiSsid = username;
    populatedOtaSecrets.wifiPassword = password;
    populatedOtaSecrets.caCert = caCert;
    populatedOtaSecrets.bikeMac = mac;
    populatedOtaSecrets.bikePassKey = passKey;

    // Close the NVS handle
    nvs_close(nvsHandle);
}

void writeValueToNvs(nvs_handle_t* nvsHandle, const char* key, const char* value) {
    esp_err_t returnStatus = nvs_set_str(*nvsHandle, key, value);
    if (returnStatus != ESP_OK)
    {
        printf("Error setting %s (%s)!\n", key, esp_err_to_name(returnStatus));
    }
}

void saveSecretsToNvs(OtaSecretKeys *secretKeys, OtaSecretValues *secretValues)
{
    nvs_handle_t nvsHandle;
    esp_err_t returnStatus;

    returnStatus = nvs_open(secretKeys->nvsNamespace, NVS_READWRITE, &nvsHandle);
    if (returnStatus != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(returnStatus));
        return;
    }

    writeValueToNvs(&nvsHandle, secretKeys->wifiSsidNvsKey, secretValues->wifiSsid);
    writeValueToNvs(&nvsHandle, secretKeys->wifiPasswordNvsKey, secretValues->wifiPassword);
    writeValueToNvs(&nvsHandle, secretKeys->caCertNvsKey, secretValues->caCert);
    writeValueToNvs(&nvsHandle, secretKeys->bikeMacNvsKey, secretValues->bikeMac);
    writeValueToNvs(&nvsHandle, secretKeys->bikePassKeyNvsKey, secretValues->bikePassKey);

    // Commit the changes to flash
    nvs_commit(nvsHandle);

    // Close the NVS handle
    nvs_close(nvsHandle);
}

static void printSha256(const uint8_t *image_hash, const char *label)
{
    char hashPrint[HASH_LEN * 2 + 1];
    hashPrint[HASH_LEN * 2] = 0;

    for (int i = 0; i < HASH_LEN; ++i)
    {
        sprintf(&hashPrint[i * 2], "%02x", image_hash[i]);
    }

    smartLog("%s %s", label, hashPrint);
}

static void getPartitionsSha256(void)
{
    uint8_t sha256[HASH_LEN] = {0};
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address = ESP_BOOTLOADER_OFFSET;
    partition.size = ESP_PARTITION_TABLE_OFFSET;
    partition.type = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha256);
    printSha256(sha256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha256);
    printSha256(sha256, "SHA-256 for current firmware: ");
}

esp_err_t httpEventHandler(esp_http_client_event_t *evt)
{
    int64_t nowTimeMicroS = esp_timer_get_time();

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        smartLog("HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        smartLog("HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        smartLog("HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        // SMART_LOG(CONFIG_APP_LOG_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        smartLog("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        loadedBytes += evt->data_len;

        if (nowTimeMicroS - lastDataNotificationTime > 500000 || nowTimeMicroS <= lastDataNotificationTime)
        {
            lastDataNotificationTime = nowTimeMicroS;
            smartLog("HTTP_EVENT_ON_DATA %d", loadedBytes);
            loadedBytes = 0;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        smartLog("HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        smartLog("HTTP_EVENT_DISCONNECTED");
        break;
        // case HTTP_EVENT_REDIRECT:
        //     ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        //     break;
    }
    return ESP_OK;
}

void firmwareUpdateTask(void *parameter)
{
    smartLog("Starting OTA example task");

    const esp_http_client_config_t config = {
        .url = "https://zzzorgo.dev/esp32/firmware.bin",
        .cert_pem = caCert,
        .event_handler = httpEventHandler,
        .keep_alive_enable = true,
    };

    smartLog("Attempting to download update");
    smartLog("Attempting to download update from %s", config.url);
    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK)
    {
        smartLog("OTA Succeed, Rebooting...");
        delay(2000);
        destroySmartLog();
        delay(2000);
        esp_restart();
    }
    else
    {
        smartLog("Firmware upgrade failed");
    }

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void firmwareUpdate()
{
    xTaskCreate(
        firmwareUpdateTask,
        "firmwareUpdateTask",
        8192,
        NULL,
        tskIDLE_PRIORITY,
        NULL /* Task handle. */
    );
}

void setupOta(OtaSecretKeys *secretKeys, std::vector<OtaCommand> commands, OtaSecretValues *secretValues)
{
    smartLog("Setting up OTA");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    getPartitionsSha256();

    ESP_ERROR_CHECK(esp_netif_init());

    if (secretValues != nullptr)
    {
        saveSecretsToNvs(secretKeys, secretValues);
    }

    loadSecretsFromNvs(secretKeys);

    WiFi.begin(username, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        smartLog("[Wifi] Connecting...");
    }

    smartLog("[Wifi] Connected! %s", WiFi.localIP().toString().c_str());

    commands.push_back({
        .key = "update",
        .callback = firmwareUpdate,
    });

    setupServer(commands);
    smartLog("OTA is ready");
}
