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

#ifndef __Convert_H__
#define __Convert_H__

#include "internal.h"
#include "field.h"
#include "transform.h"


/********************************************************************
********************************************************************/






/*****************************************************
 *******   Colorspace Single-Byte Conversions   ******
 ****************************************************/


inline void YUV2RGB(int y, int u, int v, BYTE* out) 
{
  const int crv = int(1.596*65536+0.5);
  const int cgv = int(0.813*65536+0.5);
  const int cgu = int(0.391*65536+0.5);
  const int cbu = int(2.018*65536+0.5);

  int scaled_y = (y - 16) * int((255.0/219.0)*65536+0.5);

  _PixelClip PixelClip;

  out[0] = ScaledPixelClip(scaled_y + (u-128) * cbu); // blue
  out[1] = ScaledPixelClip(scaled_y - (u-128) * cgu - (v-128) * cgv); // green
  out[2] = ScaledPixelClip(scaled_y + (v-128) * crv); // red
}


// not used here, but useful to other filters
inline int RGB2YUV(int rgb) 
{
  const int cyb = int(0.114*219/255*65536+0.5);
  const int cyg = int(0.587*219/255*65536+0.5);
  const int cyr = int(0.299*219/255*65536+0.5);

  _PixelClip PixelClip;

  // y can't overflow
  int y = (cyb*(rgb&255) + cyg*((rgb>>8)&255) + cyr*((rgb>>16)&255) + 0x108000) >> 16;
  int scaled_y = (y - 16) * int(255.0/219.0*65536+0.5);
  int b_y = ((rgb&255) << 16) - scaled_y;
  int u = ScaledPixelClip((b_y >> 10) * int(1/2.018*1024+0.5) + 0x800000);
  int r_y = (rgb & 0xFF0000) - scaled_y;
  int v = ScaledPixelClip((r_y >> 10) * int(1/1.596*1024+0.5) + 0x800000);
  return (y*256+u)*256+v;
}


// in convert_a.asm
extern "C" 
{
  extern void __cdecl mmx_YUY2toRGB24(const BYTE* src, BYTE* dst, const BYTE* src_end, int src_pitch, int row_size, bool rec709);
  extern void __cdecl mmx_YUY2toRGB32(const BYTE* src, BYTE* dst, const BYTE* src_end, int src_pitch, int row_size, bool rec709);
}






/********************************************************
 *******   Colorspace GenericVideoFilter Classes   ******
 *******************************************************/

class RGB24to32 : public GenericVideoFilter 
/**
  * RGB -> RGBA, setting alpha channel to 255
  */
{
public:
  RGB24to32(PClip src);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};


class RGB32to24 : public GenericVideoFilter 
/**
  * Class to strip alpha channel
  */
{
public:
  RGB32to24(PClip src);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};
  

class ConvertToRGB : public GenericVideoFilter 
/**
  * Class to handle conversion to RGB & RGBA
 **/
{
public:
  ConvertToRGB(PClip _child, bool rgb24, const char* matrix, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  
  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);  
  static AVSValue __cdecl Create32(AVSValue args, void*, IScriptEnvironment* env);
  static AVSValue __cdecl Create24(AVSValue args, void*, IScriptEnvironment* env);

private:
  bool use_mmx, rec709, is_yv12;
  int yv12_width;

};

class ConvertToYV12 : public GenericVideoFilter 
/**
  * Class for conversions to YV12
 **/
{
public:
  ConvertToYV12(PClip _child, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  static AVSValue __cdecl Create(AVSValue args,void*, IScriptEnvironment* env);

private:
  bool isYUY2, isRGB32, isRGB24;
};


class ConvertToYUY2 : public GenericVideoFilter 
/**
  * Class for conversions to YUY2
 **/
{
public:
  ConvertToYUY2(PClip _child, IScriptEnvironment* env);
	void mmx_ConvertRGB32toYUY2(unsigned int *src,unsigned int *dst,int src_pitch, int dst_pitch,int w, int h);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);

private:
  const int src_cs;  // Source colorspace

};

class ConvertBackToYUY2 : public GenericVideoFilter 
/**
  * Class for conversions to YUY2 (With Chroma copy)
 **/
{
public:
  ConvertBackToYUY2(PClip _child, IScriptEnvironment* env);
	void mmx_ConvertRGB32toYUY2(unsigned int *src,unsigned int *dst,int src_pitch, int dst_pitch,int w, int h);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);

private:
  const bool rgb32;

};



class Greyscale : public GenericVideoFilter 
/**
  * Class to convert YUY2 video to greyscale
 **/
{
public:
  Greyscale(PClip _child, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);
};



#endif  // __Convert_H__