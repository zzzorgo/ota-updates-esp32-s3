#include <Arduino.h>
#include "otaMain.h"
#include "smartLogger.h"
#include <vector>

static uint8_t unlockCmd[] = {1};
static uint8_t lockCmd[] = {0};
static uint8_t lightsOffCmd[] = {0x0A, 0x10, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x00, 0xD4, 0xB1};
static uint8_t lightsOnCmd[] = {0x0A, 0x10, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x01, 0x15, 0x71};

#include <NimBLEDevice.h>
#define CONFIG_NIMBLE_CPP_ENABLE_RETURN_CODE_TEXT

TaskHandle_t *taskToDelete;
std::vector<OtaCommand> commands;

class ClientCallbacks : public NimBLEClientCallbacks
{
  uint32_t onPassKeyRequest()
  {
    smartLog("Starting NimBLE Client");
    /** return the passkey to send to the server */
    /** Change this to be different from NimBLE_Secure_Server if you want to test what happens on key mismatch */
    return atoi(populatedOtaSecrets.bikePassKey); // e.g. 123456, no quotes.
  };
  void onAuthenticationComplete(ble_gap_conn_desc *desc)
  {
    smartLog("Auth completed");
    if (!desc->sec_state.encrypted)
    {
      smartLog("Encrypt connection failed - disconnecting");
      /** Find the client with the connection handle provided in desc */
      NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
      return;
    }
  };
  bool onConfirmPIN(uint32_t pass_key)
  {
    smartLog("The passkey YES/NO number: %d", pass_key);
    /** Return false if passkeys don't match. */
    return true;
  };
};
static ClientCallbacks clientCB;

static void uglyCallback(NimBLERemoteCharacteristic *characteristic, unsigned char *data, unsigned int len, bool q)
{
  smartLog("Callback data for handle: %d", characteristic->getHandle());
  for (int i = 0; i < len; i++)
  {
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println();
}

NimBLERemoteCharacteristic *pUnlock;
NimBLERemoteCharacteristic *pWrite;

void unlockCowboy()
{
  pUnlock->writeValue(unlockCmd, 1, true);
  smartLog("unlocked");
}

void lockCowboy()
{
  pUnlock->writeValue(lockCmd, 1, true);
  smartLog("locked");
}

void setupBle(void *param)
{
  smartLog("Starting NimBLE Client");
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(false, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);

  NimBLEUUID serviceUuid("c0b0a000-18eb-499d-b266-2f2910744274");
  NimBLEUUID unlockChar("c0b0a001-18eb-499d-b266-2f2910744274");
  NimBLEUUID uartServiceUuid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  NimBLEUUID writeUartChar("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
  NimBLEUUID notifyUartChar("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

  NimBLEClient *pClient = NimBLEDevice::createClient(NimBLEAddress(populatedOtaSecrets.bikeMac, 1)); // TODO your mac address here
  pClient->setClientCallbacks(&clientCB, false);

  smartLog("Connecting");
  //      if (pClient->connect(&device))
  if (pClient->connect())
  {
    smartLog("Connection, going into secure mode");
    pClient->secureConnection();
    smartLog("waiting a bit..");
    smartLog("Getting services");
    //    pClient->discoverAttributes();
    smartLog("MTU: %d", pClient->getMTU());

    Serial.println("");
    Serial.println("");
    Serial.println("");
    Serial.println("");

    NimBLERemoteService *pService = pClient->getService(serviceUuid);
    NimBLERemoteService *pUartService = pClient->getService(uartServiceUuid);
    if (pService != nullptr)
    {
      pUnlock = pService->getCharacteristic(unlockChar);
      pWrite = pUartService->getCharacteristic(writeUartChar);
      NimBLERemoteCharacteristic *pRx = pUartService->getCharacteristic(notifyUartChar);

      pRx->subscribe(true, &uglyCallback, false);

      if (pWrite == nullptr)
      {
        Serial.println("this is a problem!!");
      }
      std::string x = pWrite->toString();
      smartLog(x.c_str());

      smartLog("connecting done");
      if (pUnlock == nullptr)
      {
        smartLog("not found");
      }
      else
      {
        smartLog("current lock status");
        smartLog("unlocking...");

        pUnlock->writeValue(unlockCmd, 1, true);
        smartLog("unlocked");
        delay(5 * 1000);
        // pWrite->writeValue(lightsOffCmd, 11);
      //   smartLog("light off");
      //   delay(2 * 1000);
      //   pWrite->writeValue(lightsOnCmd, 11);
      //   smartLog("light on");
      //   delay(2 * 1000);
        pUnlock->writeValue(lockCmd, 1, false);
        smartLog("locked");
      }
    }
  }
  else
  {
    // failed to connect
    smartLog("failed to connect");
  }
  // smartLog("stopping and going to deep sleep");
  // NimBLEDevice::deleteClient(pClient);
  // Serial.flush();
  // esp_sleep_enable_timer_wakeup(1000 * 1000000);
  // esp_deep_sleep_start();

  //    }
  //  }

  while (1)
  {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);

  OtaSecretKeys secretKeys = {
      .nvsNamespace = "credentials",
      .wifiSsidNvsKey = "username",
      .wifiPasswordNvsKey = "password",
      .caCertNvsKey = "cert",
      .bikeMacNvsKey = "bikemac",
      .bikePassKeyNvsKey = "bikepasskey",
    };

  commands.push_back({
      .key = "connect-cowboy",
      .callback = []()
      {
        xTaskCreate(
            setupBle,
            "setupBle",
            8192,
            NULL,
            tskIDLE_PRIORITY,
            taskToDelete /* Task handle. */
        );
      },
    });

  commands.push_back({
      .key = "restart",
      .callback = esp_restart,
  });

  commands.push_back({
      .key = "unlock",
      .callback = unlockCowboy,
  });

  commands.push_back({
      .key = "lock",
      .callback = lockCowboy,
  });

  setupOta(&secretKeys, commands);
}

void loop()
{
  delay(1000);
}
