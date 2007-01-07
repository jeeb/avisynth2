#pragma once
#include "soundoutput.h"
#include "h\aften.h"

class AC3Output :
  public SoundOutput
{
public:
  AC3Output(PClip _child, IScriptEnvironment* _env);
  virtual ~AC3Output(void);
public:
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
  void encodeLoop();
protected:
  virtual bool getParamsFromGUI();
  virtual bool setParamsToGUI();
  AftenContext aften;
  void encodeBlock(unsigned char* in);
  FILE *f;
  __int64 encodedSamples;


};
