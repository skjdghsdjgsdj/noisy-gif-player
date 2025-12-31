#pragma once
#include <Arduino.h>
#include <FS.h>

class PreferenceManager;

class GifSelector {
public:
  static GifSelector &instance();

  bool chooseRandomGifAndWav(String &gifPath, String &wavPath);

private:
  GifSelector(PreferenceManager &prefsRef);

  static constexpr size_t MAX_GIFS = 64;
  static constexpr const char* GIF_DIR = "/gifs";
  static constexpr const char* WAV_DIR = "/wavs";

  String        candidates[MAX_GIFS];
  PreferenceManager &prefsManager;

  bool  collectCandidates(size_t &count);
  File  openGifDirectory();
  void  enumerateGifCandidates(String outCandidates[], size_t maxCount, size_t &outCount);
  void  tryAddGifCandidate(String outCandidates[], size_t maxCount, size_t &outCount, File &f);

  String normalizeGifPath(const String &rawName);
  String extractFileName(const String &path);
  bool   isValidGifBaseName(const String &base);
  String stripExtension(const String &base);
  String buildWavPathFromGif(const String &gifPath);
  String chooseGifPathWithPreference(String list[], size_t count, const String &lastGifPath);
};

