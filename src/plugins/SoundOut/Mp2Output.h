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
