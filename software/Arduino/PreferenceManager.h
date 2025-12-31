#pragma once
#include <Arduino.h>
#include <Preferences.h>

class PreferenceManager {
public:
  static PreferenceManager &instance() {
    static PreferenceManager inst;
    return inst;
  }

  String loadLastGifPath();
  void storeLastGifPath(const String &gifPath);

private:
  PreferenceManager();
  ~PreferenceManager();

  Preferences prefs;

  static constexpr const char* PREF_NAMESPACE   = "gif_player";
  static constexpr const char* PREF_LAST_GIFKEY = "last_gif";
};

