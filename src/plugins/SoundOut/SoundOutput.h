#pragma once
#include "soundout.h"
#include "commctrl.h"
#include "windows.h"
#include <stdio.h>

class SampleFetcher;

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
  const char* outputFileFilter;
protected:
  virtual bool initEncoder() { return true;}   // Called to Init the encoder, returns false if error occured.
  void updatePercent(int p);
  void updateSampleStats(__int64 processed,__int64 total);
  HWND wnd;
  HWND statWnd;
  SampleFetcher *input;
  IScriptEnvironment* env;
  DWORD lastUpdateTick;
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
};


int ConvertDataSizes(__int64 bytes, const char** OutMetric);
