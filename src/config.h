#pragma once
#include <Arduino.h>
#include <WebServer.h>

// ================= CONFIG STRUCT =================
struct Config {
  String mode;
  String ssid;
  String pass;
};

extern Config cfg;

// ================= FUNCTIONS =================
void loadConfig();
void saveConfig();
void startWiFi();

// setup UI
void registerConfigRoutes(WebServer &server);
