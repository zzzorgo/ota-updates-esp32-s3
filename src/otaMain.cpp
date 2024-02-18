#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sdkconfig.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <WiFi.h>
#include "esp_http_client.h"

#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"

#include "serverSetup.h"
#include "smartLogger.h"

#include "otaMain.h"

#define HASH_LEN 32

int messageCounter = 0;

char username[32];
char password[32];
char caCert[4096];

void readValueFromNvs(nvs_handle_t* nvsHandle, const char* key, char* output) {
    size_t requiredSize;
    esp_err_t returnStatus = nvs_get_str(*nvsHandle, key, NULL, &requiredSize);

    if (returnStatus == ESP_OK)
    {
        nvs_get_str(*nvsHandle, key, output, &requiredSize);
        ESP_LOGI(CONFIG_APP_LOG_TAG, "Password: %s\n", output);
    }
    else
    {
        ESP_LOGI(CONFIG_APP_LOG_TAG, "Error reading %s (%s)!\n", key, esp_err_to_name(returnStatus));
    }
}

void loadSecretsFromNvs(OtaSecretKeys *secretKeys)
{
    nvs_handle_t nvsHandle;
    esp_err_t returnStatus;

    returnStatus = nvs_open(secretKeys->nvsNamespace, NVS_READONLY, &nvsHandle);

    if (returnStatus != ESP_OK)
    {
        ESP_LOGI(CONFIG_APP_LOG_TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(returnStatus));
        return;
    }

    readValueFromNvs(&nvsHandle, secretKeys->wifiSsidNvsKey, username);
    readValueFromNvs(&nvsHandle, secretKeys->wifiPasswordNvsKey, password);
    readValueFromNvs(&nvsHandle, secretKeys->caCertNvsKey, caCert);

    // Close the NVS handle
    nvs_close(nvsHandle);
}

void saveSecretsToNvs(OtaSecretKeys *secretKeys, OtaSecretValues *secretValues)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open(secretKeys->nvsNamespace, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
        return;
    }

    ret = nvs_set_str(nvs_handle, secretKeys->wifiSsidNvsKey, secretValues->wifiSsid);
    if (ret != ESP_OK)
    {
        printf("Error setting ssid (%s)!\n", esp_err_to_name(ret));
    }

    ret = nvs_set_str(nvs_handle, secretKeys->wifiPasswordNvsKey, secretValues->wifiPassword);
    if (ret != ESP_OK)
    {
        printf("Error setting password (%s)!\n", esp_err_to_name(ret));
    }

    ret = nvs_set_str(nvs_handle, secretKeys->caCertNvsKey, secretValues->caCert);
    if (ret != ESP_OK)
    {
        printf("Error setting ca cert (%s)!\n", esp_err_to_name(ret));
    }

    // Commit the changes to flash
    nvs_commit(nvs_handle);

    // Close the NVS handle
    nvs_close(nvs_handle);
}

static void printSha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i)
    {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(CONFIG_APP_LOG_TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = {0};
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address = ESP_BOOTLOADER_OFFSET;
    partition.size = ESP_PARTITION_TABLE_OFFSET;
    partition.type = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    printSha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    printSha256(sha_256, "SHA-256 for current firmware: ");
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(CONFIG_APP_LOG_TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(CONFIG_APP_LOG_TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(CONFIG_APP_LOG_TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(CONFIG_APP_LOG_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        messageCounter++;

        if (messageCounter >= 80)
        {
            messageCounter = 0;
            smartLog("HTTP_EVENT_ON_DATA");
        }
        // ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(CONFIG_APP_LOG_TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(CONFIG_APP_LOG_TAG, "HTTP_EVENT_DISCONNECTED");
        break;
        // case HTTP_EVENT_REDIRECT:
        //     ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        //     break;
    }
    return ESP_OK;
}

// extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");

void firmwareUpdateTask(void *parameter)
{
    smartLog("Starting OTA example task");
    ESP_LOGI(CONFIG_APP_LOG_TAG, "Starting OTA example task");

    // esp_crt_bundle_init(x509_crt_imported_bundle_bin_start);

    const esp_http_client_config_t config = {
        .url = "https://zzzorgo.dev/esp32/firmware.bin",
        .cert_pem = caCert,
        .event_handler = _http_event_handler,
        // .crt_bundle_attach = arduino_esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    //   esp_https_ota_config_t ota_config = {
    //     .http_config = &config,
    // };
    smartLog("Attempting to download update");
    ESP_LOGI(CONFIG_APP_LOG_TAG, "Attempting to download update from %s", config.url);
    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK)
    {
        smartLog("OTA Succeed, Rebooting...");
        ESP_LOGI(CONFIG_APP_LOG_TAG, "OTA Succeed, Rebooting...");
        // destroySmartLog();
        delay(1000);
        esp_restart();
    }
    else
    {
        ESP_LOGE(CONFIG_APP_LOG_TAG, "Firmware upgrade failed");
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

void setupOta(OtaSecretKeys *secretKeys, OtaSecretValues *secretValues)
{
    ESP_LOGI(CONFIG_APP_LOG_TAG, "STARTED");
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

    // get_sha256_of_partitions();

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
        ESP_LOGI(CONFIG_APP_LOG_TAG, "[Wifi] Connecting...");
    }

    ESP_LOGI(CONFIG_APP_LOG_TAG, "[Wifi] Connected! %s", WiFi.localIP());

    setupServer(firmwareUpdate);
}

void loopOta()
{
    loopServer();
}
