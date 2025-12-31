#include "PreferenceManager.h"

PreferenceManager::PreferenceManager() {
  prefs.begin(PREF_NAMESPACE, false);
}

PreferenceManager::~PreferenceManager() {
  prefs.end();
}

String PreferenceManager::loadLastGifPath() {
  return prefs.getString(PREF_LAST_GIFKEY, "");
}

void PreferenceManager::storeLastGifPath(const String &gifPath) {
  prefs.putString(PREF_LAST_GIFKEY, gifPath);
}

