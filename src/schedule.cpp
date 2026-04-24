#include "schedule.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <algorithm>

Schedule schedule;

void Schedule::begin() {
  load();
}

void Schedule::load() {
  times.clear();
  
  if (!LittleFS.exists("/schedule.txt")) {
    Serial.println("[SCHEDULE] No schedule.txt found");
    return;
  }
  
  File file = LittleFS.open("/schedule.txt", "r");
  if (!file) {
    Serial.println("[SCHEDULE] Failed to open schedule.txt");
    return;
  }
  
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    
    if (line.length() == 0) continue;
    
    // Parse HH:MM format
    int colonPos = line.indexOf(':');
    if (colonPos > 0) {
      int hour = line.substring(0, colonPos).toInt();
      int minute = line.substring(colonPos + 1).toInt();
      
      if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {
        times.push_back(ScheduleTime(hour, minute));
        Serial.printf("[SCHEDULE] Loaded: %02d:%02d\n", hour, minute);
      }
    }
  }
  
  file.close();
  sortTimes();
  Serial.printf("[SCHEDULE] Loaded %d times\n", times.size());
}

void Schedule::save() {
  File file = LittleFS.open("/schedule.txt", "w");
  if (!file) {
    Serial.println("[SCHEDULE] Failed to save schedule.txt");
    return;
  }
  
  for (const auto& t : times) {
    file.println(t.toString());
  }
  
  file.close();
  Serial.printf("[SCHEDULE] Saved %d times\n", times.size());
}

void Schedule::clear() {
  times.clear();
  save();
}

bool Schedule::addTime(int hour, int minute) {
  if (hour < 0 || hour >= 24 || minute < 0 || minute >= 60) {
    return false;
  }
  
  // Check for duplicates
  for (const auto& t : times) {
    if (t.hour == hour && t.minute == minute) {
      return false; // Duplicate
    }
  }
  
  times.push_back(ScheduleTime(hour, minute));
  sortTimes();
  save();
  
  Serial.printf("[SCHEDULE] Added: %02d:%02d\n", hour, minute);
  return true;
}

bool Schedule::removeTime(int index) {
  if (index < 0 || index >= (int)times.size()) {
    return false;
  }
  
  Serial.printf("[SCHEDULE] Removed: %s\n", times[index].toString().c_str());
  times.erase(times.begin() + index);
  save();
  return true;
}

ScheduleTime Schedule::getTime(int index) const {
  if (index >= 0 && index < (int)times.size()) {
    return times[index];
  }
  return ScheduleTime(0, 0);
}

int Schedule::checkStartTime(int currentHour, int currentMinute) {
  // Convert current time to minutes since midnight
  int currentMinutes = currentHour * 60 + currentMinute;
  
  for (size_t i = 0; i < times.size(); i++) {
    if (times[i].completed) continue;
    
    // Calculate start time (5 minutes before scheduled time)
    int scheduledMinutes = times[i].toMinutes();
    int startMinutes = (scheduledMinutes - 5 + 1440) % 1440;
    
    // Check if current minute matches start minute
    if (currentMinutes == startMinutes) {
      return i; // Return index of time to start
    }
  }
  
  return -1; // No match
}

void Schedule::resetCompleted() {
  bool changed = false;
  for (auto& t : times) {
    if (t.completed) {
      t.completed = false;
      changed = true;
    }
  }
  if (changed) {
    Serial.println("[SCHEDULE] Reset completed flags for new day");
  }
}

void Schedule::markCompleted(int index) {
  if (index >= 0 && index < (int)times.size()) {
    times[index].completed = true;
    Serial.printf("[SCHEDULE] Marked completed: %s\n", times[index].toString().c_str());
  }
}

void Schedule::sortTimes() {
  std::sort(times.begin(), times.end(), [](const ScheduleTime& a, const ScheduleTime& b) {
    return a.toMinutes() < b.toMinutes();
  });
}

String Schedule::toJson() const {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (const auto& t : times) {
    JsonObject obj = arr.add<JsonObject>();
    obj["time"] = t.toString();
    obj["completed"] = t.completed;
  }
  
  String output;
  serializeJson(doc, output);
  return output;
}
