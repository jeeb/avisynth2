#pragma once
#include "soundoutput.h"
#include "FLAC++/encoder.h"

using namespace FLAC;

class FlacOutput :
  public SoundOutput, public Encoder::File
{
public:
  FlacOutput(PClip _child, IScriptEnvironment* _env) ;
  void FlacOutput::progress_callback (FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate);
public:
  virtual ~FlacOutput(void);
  void encodeLoop();
  bool initEncoder();  // Called to Init the encoder, returns false if error occured.
  virtual bool getParamsFromGUI();
  virtual bool setParamsToGUI();
  virtual void showGUI();
};

extern const char * const _FLAC__StreamEncoderStateString[];
extern const char * const _FLAC__StreamEncoderInitStatusString[];

