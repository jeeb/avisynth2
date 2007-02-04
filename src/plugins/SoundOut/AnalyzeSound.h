#pragma once
#include "soundoutput.h"

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

private:
  float* maximum;
  double* accumulated;
  double* squared_accumulated;
  void updateStats(__int64 processed,__int64 total, bool force);
};
