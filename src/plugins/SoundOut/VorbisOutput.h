#pragma once
#include "soundoutput.h"

class VorbisOutput :
  public SoundOutput
{
public:
  VorbisOutput(PClip _child, IScriptEnvironment* _env);
public:
  virtual ~VorbisOutput(void);
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
  void encodeLoop();
  virtual void showGUI();
  virtual bool getParamsFromGUI();
  virtual bool setParamsToGUI();
protected:
};
