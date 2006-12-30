#pragma once
#include "soundoutput.h"
#include "h\MAC\all.h"
#include "h\MAC\MAClib.h"

class MacOutput :
  public SoundOutput
{
public:
  MacOutput(PClip _child, IScriptEnvironment* _env);
  ~MacOutput(void);
  void encodeLoop();
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
private:
  IAPECompress * pAPECompress;
};


