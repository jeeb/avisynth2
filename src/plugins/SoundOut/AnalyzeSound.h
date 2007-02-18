#pragma once
#include "soundoutput.h"
#include "gain_analysis.h"

class AnalyzeSound :
  public SoundOutput
{
public:
  AnalyzeSound(PClip _child, IScriptEnvironment* _env);
  virtual ~AnalyzeSound(void);
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
  void encodeLoop();
  void showGUI(void) {}
  HWND hStats;
  replaygain_t** rGain;
  bool canReplayGain;

private:
  float* maximum;
  double* accumulated;
  double* squared_accumulated;
  double** squared_50ms;

  void updateStats(__int64 processed,__int64 total, bool force);
};
