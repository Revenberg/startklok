#include "telegram.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <LittleFS.h>

TelegramHelper telegram;

static WiFiClientSecure client;
static UniversalTelegramBot* bot = nullptr;

void TelegramHelper::begin() {
  loadConfig();
  
  if (configured) {
    client.setInsecure();  // For testing - in production use setCACert
    bot = new UniversalTelegramBot(botToken, client);
    Serial.println("[Telegram] Initialized");
  } else {
    Serial.println("[Telegram] Not configured");
  }
}

void TelegramHelper::sendMessage(String message) {
  if (!configured || !bot) {
    Serial.println("[Telegram] Not configured, cannot send message");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Telegram] WiFi not connected");
    return;
  }
  
  Serial.printf("[Telegram] Sending: %s\n", message.c_str());
  
  if (bot->sendMessage(chatId, message, "")) {
    Serial.println("[Telegram] ✓ Message sent");
  } else {
    Serial.println("[Telegram] ✗ Failed to send message");
  }
}

void TelegramHelper::sendRaceStatus(bool running, bool sequence, unsigned long remaining, unsigned long elapsed) {
  String status = "🏁 Race Status\n\n";
  
  if (sequence) {
    int sec = remaining / 1000;
    int m = sec / 60;
    int s = sec % 60;
    status += "⏱️ COUNTDOWN: " + String(m) + ":" + (s < 10 ? "0" : "") + String(s);
  } else if (running) {
    if (elapsed > 300000) {
      // Overtime
      unsigned long overtimeMs = elapsed - 300000;
      int sec = overtimeMs / 1000;
      int m = sec / 60;
      int s = sec % 60;
      status += "🏁 RACE STARTED\n";
      status += "Time: +" + String(m) + ":" + (s < 10 ? "0" : "") + String(s);
    } else {
      // Still in countdown
      int sec = (300000 - elapsed) / 1000;
      int m = sec / 60;
      int s = sec % 60;
      status += "⏱️ Time: " + String(m) + ":" + (s < 10 ? "0" : "") + String(s);
    }
  } else {
    status += "✅ READY";
  }
  
  sendMessage(status);
}

bool TelegramHelper::isConfigured() {
  return configured;
}

String TelegramHelper::getBotToken() {
  return botToken;
}

String TelegramHelper::getChatId() {
  return chatId;
}

void TelegramHelper::setBotToken(String token) {
  botToken = token;
}

void TelegramHelper::setChatId(String id) {
  chatId = id;
}

void TelegramHelper::saveConfig() {
  File f = LittleFS.open("/telegram.txt", "w");
  if (f) {
    f.print(botToken);
    f.print(" ");
    f.println(chatId);
    f.close();
    
    configured = (botToken.length() > 0 && chatId.length() > 0);
    
    Serial.println("[Telegram] Config saved");
  }
}

void TelegramHelper::loadConfig() {
  if (!LittleFS.exists("/telegram.txt")) {
    Serial.println("[Telegram] No config file found");
    configured = false;
    return;
  }
  
  File f = LittleFS.open("/telegram.txt", "r");
  if (f) {
    String line = f.readStringUntil('\n');
    line.trim();
    
    int spacePos = line.indexOf(' ');
    if (spacePos > 0) {
      botToken = line.substring(0, spacePos);
      chatId = line.substring(spacePos + 1);
      chatId.trim();
      
      configured = (botToken.length() > 0 && chatId.length() > 0);
      
      Serial.printf("[Telegram] Config loaded - Token: %s..., ChatID: %s\n", 
                    botToken.substring(0, 10).c_str(), chatId.c_str());
    }
    
    f.close();
  }
}
