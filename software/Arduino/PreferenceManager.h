#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <vector>

class PreferenceManager {
public:
  static PreferenceManager &instance() {
    static PreferenceManager inst;
    return inst;
  }

  String loadLastGifPath();
  bool   storeLastGifPath(const String &gifPath);
  void   clearLastGifPath();

  void loadCandidateList(std::vector<String> &out);
  void storeCandidateList(const std::vector<String> &list);
  void clearCandidateList();

private:
  PreferenceManager();
  ~PreferenceManager();

  Preferences prefs;

  static constexpr const char* PREF_NAMESPACE    = "gif_player";
  static constexpr const char* PREF_LAST_GIFKEY  = "last_gif";
  static constexpr const char* PREF_GIFLIST_KEY  = "gif_list";

  // NVS string values are limited to ~3900 bytes after internal overhead.
  // If the serialized list exceeds this, caching is silently skipped and
  // the selector falls back to a live directory scan each boot.
  static constexpr size_t GIFLIST_MAX_BYTES = 3900;
};

