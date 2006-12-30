#pragma once
#include "soundoutput.h"
#include "h/sndfile.h"

class WavOutput :
  public SoundOutput
{
public:
  WavOutput(PClip _child, IScriptEnvironment* _env);
public:
  virtual ~WavOutput(void);
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
  void encodeLoop();
private:
  SNDFILE* sndfile;
  SF_INFO info;
};
