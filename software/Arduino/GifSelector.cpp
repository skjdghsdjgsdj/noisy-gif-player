#include "GifSelector.h"
#include <SD_MMC.h>
#include "PreferenceManager.h"

GifSelector::GifSelector(PreferenceManager &prefsRef)
  : prefsManager(prefsRef) {
}

GifSelector &GifSelector::instance() {
  static GifSelector inst(PreferenceManager::instance());
  return inst;
}

bool GifSelector::collectCandidates() {
  candidates.clear();
  enumerateGifCandidates(candidates);
  return !candidates.empty();
}

File GifSelector::openGifDirectory() {
  File dir = SD_MMC.open(GIF_DIR);
  if (!dir || !dir.isDirectory()) {
    return File();
  }
  return dir;
}

void GifSelector::enumerateGifCandidates(std::vector<String> &outCandidates) {
  File dir = openGifDirectory();
  if (!dir) {
    return;
  }

  File f = dir.openNextFile();
  while (f) {
    tryAddGifCandidate(outCandidates, f);
    f = dir.openNextFile();
  }

  dir.close();
}

void GifSelector::tryAddGifCandidate(std::vector<String> &outCandidates, File &f) {
  if (f.isDirectory()) {
    return;
  }

  String path = normalizeGifPath(f.name());
  String base = extractFileName(path);

  if (!isValidGifBaseName(base)) {
    return;
  }

  outCandidates.push_back(path);
}

String GifSelector::normalizeGifPath(const String &rawName) {
  String gifDirSlash = String(GIF_DIR) + "/";
  if (rawName.startsWith(gifDirSlash)) {
    return rawName;
  }

  return rawName.startsWith("/")
    ? String(GIF_DIR) + rawName
    : String(GIF_DIR) + "/" + rawName;
}

String GifSelector::extractFileName(const String &path) {
  int slashPos = path.lastIndexOf('/');
  return (slashPos >= 0)
    ? path.substring(slashPos + 1)
    : path;
}

bool GifSelector::isValidGifBaseName(const String &base) {
  if (base.length() == 0) {
    return false;
  }
  if (base[0] == '.') {
    return false;
  }

  String lower = base;
  lower.toLowerCase();
  return lower.endsWith(".gif");
}

String GifSelector::stripExtension(const String &base) {
  int dotPos = base.lastIndexOf('.');
  return (dotPos < 0)
    ? base
    : base.substring(0, dotPos);
}

String GifSelector::buildWavPathFromGif(const String &gifPath) {
  String base = extractFileName(gifPath);
  String noExt = stripExtension(base);
  String wavPath = String(WAV_DIR) + "/" + noExt + ".wav";
  return wavPath;
}

String GifSelector::chooseGifPathWithPreference(
  const std::vector<String> &list,
  const String &lastGifPath
) {
  size_t count = list.size();

  if (count == 1) {
    return list[0];
  }

  if (lastGifPath.length() == 0) {
    return list[random(0, (long)count)];
  }

  String chosen = lastGifPath;
  while (chosen == lastGifPath) {
    chosen = list[random(0, (long)count)];
  }

  return chosen;
}

bool GifSelector::chooseRandomGifAndWav(String &gifPath, String &wavPath) {
  if (!collectCandidates()) {
    return false;
  }

  String lastGifPath = prefsManager.loadLastGifPath();
  gifPath = chooseGifPathWithPreference(candidates, lastGifPath);
  wavPath = buildWavPathFromGif(gifPath);

  if (gifPath != lastGifPath) {
    prefsManager.storeLastGifPath(gifPath);
  }

  return true;
}

