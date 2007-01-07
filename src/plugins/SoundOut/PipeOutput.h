#pragma once
#include "soundoutput.h"
#include "h/sndfile.h"

class PipeOutput :
  public SoundOutput
{
public:
  PipeOutput(PClip _child, IScriptEnvironment* _env);
public:
  virtual ~PipeOutput(void);
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
  void encodeLoop();
  virtual bool getParamsFromGUI();
  virtual bool setParamsToGUI();
private:
  SNDFILE* sndfile;
  SF_INFO info;
};
