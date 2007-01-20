#pragma once
#include "soundoutput.h"
#include "h\lame.h"

class Mp3Output :
  public SoundOutput
{
public:
  Mp3Output(PClip _child, IScriptEnvironment* _env);
public:
  virtual ~Mp3Output(void);
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
  void encodeLoop();
  virtual void showGUI();
  virtual bool getParamsFromGUI();
  virtual bool setParamsToGUI();
  bool GUI_ready;
protected:
  lame_global_struct* lame;
};
