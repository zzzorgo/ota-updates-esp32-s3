#include "serverSetup.h"
#include "ESPAsyncWebServer.h"
#include "smartLogger.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

std::vector<OtaCommand> localCommands;

void onRequest(AsyncWebServerRequest *request)
{
  request->send(404);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    // client connected
    initSmartLog(client);
    smartLog("ws[%s][%u] connect\n", server->url(), client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    // client disconnected
    destroySmartLog();
    smartLog("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    // error was received from the other end
    smartLog("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    // pong message was received (in response to a ping request maybe)
    smartLog("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    // data packet
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len)
    {
      // the whole message is in a single frame and we got all of it's data
      smartLog("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT)
      {
        data[len] = 0;
        smartLog("%s\n", (char *)data);

        for (int i = 0; i < 10; i++)
        {
          OtaCommand command = localCommands.at(i);

          if (strcmp((char *)data, command.key) == 0)
          {
            command.callback();
            smartLog("Command executed successfully: %s", command.key);

            break;
          }
        }

        smartLog("Done");
      }
      else
      {
        for (size_t i = 0; i < info->len; i++)
        {
          smartLog("%02x ", data[i]);
        }
        smartLog("\n");
      }
    }
    else
    {
      // message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          smartLog("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        smartLog("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      smartLog("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT)
      {
        data[len] = 0;
        smartLog("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < len; i++)
        {
          smartLog("%02x ", data[i]);
        }
        smartLog("\n");
      }

      if ((info->index + len) == info->len)
      {
        smartLog("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          smartLog("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void setupServer(std::vector<OtaCommand> commands)
{
  localCommands = commands;
  ws.onEvent(onEvent);

  server.addHandler(&ws);
  server.onNotFound(onRequest);

  server.begin();
}
