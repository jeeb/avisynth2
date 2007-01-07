#pragma once
#include "soundout.h"
#include "commctrl.h"
#include "windows.h"
#include <stdio.h>
#include <map>

using namespace std;

class SampleFetcher;

struct ltstr
{
  bool operator()(const char* s1, const char* s2) const
  {
    return _stricmp(s1, s2) < 0;
  }
};

class SoundOutput: public GenericVideoFilter
{
public:
  SoundOutput(PClip _child, IScriptEnvironment* env);
public:
  virtual ~SoundOutput(void);
  void startEncoding();
  virtual void encodeLoop() = 0;  // Called as loop for encoding.
  void encodingFinished();
  HANDLE encodeThread;
  bool exitThread;
  char* outputFile;
  void setError(const char* err);
  map<const char*, AVSValue,ltstr> params;
protected:
  virtual bool initEncoder() { return true;}   // Called to Init the encoder, returns false if error occured.
  virtual void setDefaults() {}
  virtual bool getParamsFromGUI() {return true;}
  virtual bool setParamsToGUI() {return true;}
  void updatePercent(int p);
  void updateSampleStats(__int64 processed,__int64 total);
  HWND wnd;
  HWND statWnd;
  SampleFetcher *input;
  IScriptEnvironment* env;
  DWORD lastUpdateTick;
  DWORD encodingStartedTick;
  DWORD lastSPSTick;
};

// Defines how many samples should be fetched per block.
#define BLOCKSAMPLES 1024*1024 

class SampleBlock {
public:
  SampleBlock(VideoInfo* _vi, int _nSamples = BLOCKSAMPLES) :lastBlock(false) {numSamples = _nSamples; samples = new char[(int)_vi->BytesFromAudioSamples(_nSamples*_vi->AudioChannels())];}
  ~SampleBlock() { delete samples; }
  void* getSamples() {return samples;}
  int numSamples;
  bool lastBlock;
private:
  char* samples;
};

class SampleFetcher 
{
public:
  SampleFetcher(PClip _child, IScriptEnvironment* _env);
  ~SampleFetcher();
  void FetchLoop();
  SampleBlock* GetNextBlock();
  bool exitThread;
protected:
  SampleBlock* FinishedBlock;
  PClip child;
  HANDLE evtNextBlockReady;
  HANDLE evtProcessNextBlock;
  HANDLE thread;
  IScriptEnvironment* env;
  SampleBlock* prev_sb;
};


int ConvertDataSizes(__int64 bytes, const char** OutMetric);
