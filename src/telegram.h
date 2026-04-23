#pragma once
#include <Arduino.h>

class TelegramHelper {
public:
  void begin();
  bool sendMessage(String message);
  
  bool isConfigured();
  String getBotToken();
  String getChatId();
  
  void setBotToken(String token);
  void setChatId(String chatId);
  void saveConfig();
  void loadConfig();
  
private:
  String botToken;
  String chatId;
  bool configured = false;
};

extern TelegramHelper telegram;
