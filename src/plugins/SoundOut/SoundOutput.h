// SoundOut Copyright Klaus Post 2006-2007
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//

// SoundOut (c) 2006-2007 by Klaus Post

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
  bool autoCloseWindow;
  char* outputFile;
  void setError(const char* err);
  map<const char*, AVSValue,ltstr> params;
  virtual void showGUI() = 0;
  virtual bool setParamsToGUI() {return true;}
  bool quietExit;
  virtual void setDefaults() {}
  virtual bool getParamsFromGUI() {return true;}
protected:
  virtual bool initEncoder() { return true;}   // Called to Init the encoder, returns false if error occured.
  void updatePercent(int p);
  void updateSampleStats(__int64 processed,__int64 total,bool force=false);
  HWND wnd;
  HWND statWnd;
  SampleFetcher *input;
  IScriptEnvironment* env;
  DWORD lastUpdateTick;
  DWORD encodingStartedTick;
  DWORD lastSPSTick;
};

#define BLOCKSAMPLES 200 * 1024

class SampleBlock {
public:
  SampleBlock(VideoInfo* _vi, int _nSamples) :lastBlock(false) {numSamples = _nSamples; samples = new char[(int)_vi->BytesFromAudioSamples(_nSamples*_vi->AudioChannels())];}
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
  SampleFetcher(PClip _child, IScriptEnvironment* _env, int _maxSamples = BLOCKSAMPLES);
  ~SampleFetcher();
  void FetchLoop();
  SampleBlock* GetNextBlock();
  bool exitThread;
  IScriptEnvironment* env;
  PClip child;
  int maxSamples;   // Defines how many samples should be fetched per block.
protected:
  SampleBlock* FinishedBlock;
  HANDLE evtNextBlockReady;
  HANDLE evtProcessNextBlock;
  HANDLE thread;
  SampleBlock* prev_sb;
};


int ConvertDataSizes(__int64 bytes, const char** OutMetric);
