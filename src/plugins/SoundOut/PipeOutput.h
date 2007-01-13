#pragma once
#include "soundoutput.h"

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
  void fetchResults();
  void writeSamples(const void *ptr, int count);

HANDLE hChildStdinRd, hChildStdinWr,  
   hChildStdoutRd, hChildStdoutWr, 
   hChildStderrRd, hChildStderrWr; 

   HWND hConsole;
private:
  HANDLE inputThread;
};
