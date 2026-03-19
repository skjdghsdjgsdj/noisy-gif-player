#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>

class PreferenceManager;

class GifSelector {
public:
  static GifSelector &instance();

  bool chooseRandomGifAndWav(String &gifPath, String &wavPath);

private:
  GifSelector(PreferenceManager &prefsRef);

  static constexpr const char* GIF_DIR = "/gifs";
  static constexpr const char* WAV_DIR = "/wavs";

  std::vector<String> candidates;
  PreferenceManager &prefsManager;

  bool  collectCandidates();
  File  openGifDirectory();
  void  enumerateGifCandidates(std::vector<String> &outCandidates);
  void  tryAddGifCandidate(std::vector<String> &outCandidates, File &f);

  String normalizeGifPath(const String &rawName);
  String extractFileName(const String &path);
  bool   isValidGifBaseName(const String &base);
  String stripExtension(const String &base);
  String buildWavPathFromGif(const String &gifPath);
  String chooseGifPathWithPreference(const std::vector<String> &list, const String &lastGifPath);
};

