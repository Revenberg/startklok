#pragma once
#include <WebSocketsServer.h>

class WebUI {

public:
  void begin();
  void update();

  WebSocketsServer ws = WebSocketsServer(81);

  void broadcastState(bool running, bool sequence, unsigned long remaining);
};
