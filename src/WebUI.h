#pragma once
#include <WebSocketsServer.h>
#include <RTClib.h>

typedef void (*WebSocketMessageHandler)(String message);

class WebUI {

public:
  void begin();
  void update();
  void setMessageHandler(WebSocketMessageHandler handler);

  WebSocketsServer ws = WebSocketsServer(81);

  void broadcastState(bool running, bool sequence, unsigned long remaining, unsigned long elapsed, RTC_DS3231* rtc);
  
  static WebSocketMessageHandler messageHandler;
};
