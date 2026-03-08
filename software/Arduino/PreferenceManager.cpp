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

void PreferenceManager::loadCandidateList(std::vector<String> &out) {
  out.clear();
  String stored = prefs.getString(PREF_GIFLIST_KEY, "");
  if (stored.length() == 0) {
    return;
  }

  int start = 0;
  while (start < (int)stored.length()) {
    int nl = stored.indexOf('\n', start);
    String entry = (nl < 0) ? stored.substring(start) : stored.substring(start, nl);
    if (entry.length() > 0) {
      out.push_back(entry);
    }
    if (nl < 0) break;
    start = nl + 1;
  }
}

void PreferenceManager::storeCandidateList(const std::vector<String> &list) {
  String serialized;
  for (size_t i = 0; i < list.size(); i++) {
    if (i > 0) serialized += '\n';
    serialized += list[i];
  }
  if (serialized.length() > GIFLIST_MAX_BYTES) {
    return;
  }
  prefs.putString(PREF_GIFLIST_KEY, serialized);
}

void PreferenceManager::clearCandidateList() {
  prefs.remove(PREF_GIFLIST_KEY);
}

