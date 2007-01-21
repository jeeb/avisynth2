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

