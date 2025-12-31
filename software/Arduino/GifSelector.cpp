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

bool GifSelector::collectCandidates(size_t &count) {
  count = 0;
  enumerateGifCandidates(candidates, MAX_GIFS, count);
  return (count > 0);
}

File GifSelector::openGifDirectory() {
  File dir = SD_MMC.open(GIF_DIR);
  if (!dir || !dir.isDirectory()) {
    return File();
  }
  return dir;
}

void GifSelector::enumerateGifCandidates(
  String outCandidates[],
  size_t maxCount,
  size_t &outCount
) {
  outCount = 0;

  File dir = openGifDirectory();
  if (!dir) {
    return;
  }

  File f = dir.openNextFile();
  while (f && outCount < maxCount) {
    tryAddGifCandidate(outCandidates, maxCount, outCount, f);
    f = dir.openNextFile();
  }

  dir.close();
}

void GifSelector::tryAddGifCandidate(
  String outCandidates[],
  size_t maxCount,
  size_t &outCount,
  File &f
) {
  if (outCount >= maxCount) {
    return;
  }
  if (f.isDirectory()) {
    return;
  }

  String path = normalizeGifPath(f.name());
  String base = extractFileName(path);

  if (!isValidGifBaseName(base)) {
    return;
  }

  outCandidates[outCount] = path;
  outCount++;
}

String GifSelector::normalizeGifPath(const String &rawName) {
  if (rawName.startsWith(GIF_DIR)) {
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
  String list[],
  size_t count,
  const String &lastGifPath
) {
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
  size_t count = 0;
  if (!collectCandidates(count)) {
    return false;
  }

  String lastGifPath = prefsManager.loadLastGifPath();
  gifPath = chooseGifPathWithPreference(candidates, count, lastGifPath);
  wavPath = buildWavPathFromGif(gifPath);

  return true;
}

