// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
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
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

#ifndef __Levels_H__
#define __Levels_H__

#include "internal.h"


/********************************************************************
********************************************************************/



class Levels : public GenericVideoFilter 
/**
  * Class for adjusting levels in a YUV clip
 **/
{
public:
  Levels( PClip _child, int in_min, double gamma, int in_max, int out_min, int out_max, 
          IScriptEnvironment* env );
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);

private:
  BYTE map[256], mapchroma[256];

  _PixelClip PixelClip;
};


class HSIAdjust : public GenericVideoFilter 
/**
  * Class for adjusting colors in HSI space
 **/
{
public:
  HSIAdjust(PClip _child, double h, double s, int min, double gamma, int max, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);

private:
  BYTE mapY[256], mapU[256], mapV[256];

};


class RGBAdjust : public GenericVideoFilter 
/**
  * Class for adjusting colors in RGBA space
 **/
{
public:
  RGBAdjust(PClip _child, double r, double g, double b, double a, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);

private:
  BYTE mapR[256], mapG[256], mapB[256], mapA[256];

};




class Tweak : public GenericVideoFilter
{
public:
  Tweak( PClip _child, double _hue, double _sat, double _bright, double _cont, 
         IScriptEnvironment* env );

  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  AVSValue __cdecl Tweak::FilterInfo(int request);

  static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);

private:
	double hue, sat, bright, cont;

};


/**** ASM Routines ****/

void asm_tweak_ISSE_YUY2( BYTE *srcp, int w, int h, int modulo, __int64 hue, __int64 satcont, 
                     __int64 bright );


class Limiter : public GenericVideoFilter
{
public:
	Limiter(PClip _child, int _min_luma, int _max_luma, int _min_chroma, int _max_chroma, IScriptEnvironment* env);
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);
  void isse_limiter(BYTE* p, int row_size, int height, int modulo, int cmin, int cmax);
  void isse_limiter_mod8(BYTE* p, int row_size, int height, int modulo, int cmin, int cmax);
private:
	int max_luma;
  int min_luma;
  int max_chroma;
  int min_chroma;
};



#endif  // __Levels_H__

