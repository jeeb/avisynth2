#pragma once
#include "soundoutput.h"
#include "h/twolame.h"

class Mp2Output :
  public SoundOutput
{
public:
  Mp2Output(PClip _child, IScriptEnvironment* _env);
public:
  virtual ~Mp2Output(void);
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
  void encodeLoop();
  virtual void showGUI();
  bool GUI_ready;
private:
  twolame_options* encodeOptions;

};
