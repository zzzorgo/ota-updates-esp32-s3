// #include "ESPAsyncTCP.h"
#include "serverSetup.h"
#include "ESPAsyncWebServer.h"
#include "smartLogger.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");           // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

const char *http_username = "admin";
const char *http_password = "admin";

// flag to use from web update to reboot the ESP
bool shouldReboot = false;
void (*fwUpdate)();

void onRequest(AsyncWebServerRequest *request)
{
  // Handle Unknown Request
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  // Handle upload
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    //client connected
    initSmartLog(client);
    ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client (hey) %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    destroySmartLog();
    ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
    ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if(info->opcode == WS_TEXT){
        data[len] = 0;
        ESP_LOGI(CONFIG_APP_LOG_TAG, "%s\n", (char*)data);
      } else {
        for(size_t i=0; i < info->len; i++){
          ESP_LOGI(CONFIG_APP_LOG_TAG, "%02x ", data[i]);
        }
        ESP_LOGI(CONFIG_APP_LOG_TAG, "\n");
      }
      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
        if (strcmp((char*)data, "update") == 0) {
          fwUpdate();
        }
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        ESP_LOGI(CONFIG_APP_LOG_TAG, "%s\n", (char*)data);
      } else {
        for(size_t i=0; i < len; i++){
          ESP_LOGI(CONFIG_APP_LOG_TAG, "%02x ", data[i]);
        }
        ESP_LOGI(CONFIG_APP_LOG_TAG, "\n");
      }

      if((info->index + len) == info->len){
        ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          ESP_LOGI(CONFIG_APP_LOG_TAG, "ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void setupServer(void (*firmwareUpdate)(void))
{
  fwUpdate = firmwareUpdate;

  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // attach AsyncEventSource
  server.addHandler(&events);

  // respond to GET requests on URL /heap
  server.on("/heap", HTTP_GET, [firmwareUpdate](AsyncWebServerRequest *request)
            {
              request->send(200, "text/plain", String(ESP.getFreeHeap()));
              firmwareUpdate();
            });

  // upload a file to /upload
  server.on(
      "/upload", HTTP_POST, [](AsyncWebServerRequest *request)
      { request->send(200); },
      onUpload);

  // send a file when /index is requested
  // server.on("/index", HTTP_ANY, [](AsyncWebServerRequest *request)
  //           { request->send(SPIFFS, "/index.htm"); });

  // HTTP basic authentication
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if(!request->authenticate(http_username, http_password))
        return request->requestAuthentication();
    request->send(200, "text/plain", "Login Success!"); });

  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>"); });

  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();
}

void loopServer()
{
  if (shouldReboot)
  {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
  static char temp[128];
  sprintf(temp, "Seconds since boot: %u", millis() / 1000);
  events.send(temp, "time"); // send event "time"
}
