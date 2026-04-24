#pragma once
#include <Arduino.h>
#include <vector>

struct ScheduleTime {
  int hour;
  int minute;
  bool completed;
  
  ScheduleTime(int h, int m) : hour(h), minute(m), completed(false) {}
  
  String toString() const {
    char buf[9];
    sprintf(buf, "%02d:%02d:00", hour, minute);
    return String(buf);
  }
  
  // Get time in minutes since midnight
  int toMinutes() const {
    return hour * 60 + minute;
  }
};

class Schedule {
public:
  void begin();
  void load();
  void save();
  void clear();
  
  bool addTime(int hour, int minute);
  bool removeTime(int index);
  int getCount() const { return times.size(); }
  ScheduleTime getTime(int index) const;
  
  // Check if it's time to start (5 minutes before scheduled time)
  int checkStartTime(int currentHour, int currentMinute);
  
  // Mark a time as completed
  void markCompleted(int index);

  // Reset completed flags (e.g. at a new day)
  void resetCompleted();
  
  // Get all times as JSON array
  String toJson() const;
  
private:
  std::vector<ScheduleTime> times;
  void sortTimes();
};

extern Schedule schedule;
