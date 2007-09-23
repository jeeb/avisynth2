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

// ConvertPlanar (c) 2005 by Klaus Post

#ifndef __Convert_PLANAR_H__
#define __Convert_PLANAR_H__

#include "../internal.h"
#include "convert_matrix.h"
#include "../filters/resample.h"

enum {Rec601=0, Rec709=1, PC_601=2, PC_709=3 };

static int getMatrix( const char* matrix, IScriptEnvironment* env) {
  if (matrix) {
    if (!lstrcmpi(matrix, "rec601"))
      return Rec601;
    if (!lstrcmpi(matrix, "rec709"))
      return Rec709;
    else if (!lstrcmpi(matrix, "PC.601"))
      return PC_601;
    else if (!lstrcmpi(matrix, "PC.709"))
      return PC_709;
    else if (!lstrcmpi(matrix, "PC601"))
      return PC_601;
    else if (!lstrcmpi(matrix, "PC709"))
      return PC_709;
  }
  env->ThrowError("Convert: Unknown colormatrix");
  return Rec601; // Default colorspace conversion for AviSynth
}

enum   {PLACEMENT_MPEG2, PLACEMENT_MPEG1, PLACEMENT_DV } ;

static int getPlacement( const char* placement, IScriptEnvironment* env) {
  if (placement) {
    if (!lstrcmpi(placement, "mpeg2"))
      return PLACEMENT_MPEG2;
    if (!lstrcmpi(placement, "mpeg1"))
      return PLACEMENT_MPEG1;
    if (!lstrcmpi(placement, "dv"))
      return PLACEMENT_DV;
  }
  env->ThrowError("Convert: Unknown chromaplacement");
  return PLACEMENT_MPEG2;
}

static ResamplingFunction* getResampler( const char* resampler, IScriptEnvironment* env) {
  if (resampler) {
    if      (!lstrcmpi(resampler, "point"))
      return new PointFilter();
    else if (!lstrcmpi(resampler, "bilinear"))
      return new TriangleFilter();
    else if (!lstrcmpi(resampler, "bicubic"))
      return new MitchellNetravaliFilter(1./3,1./3); // Parse out optional B= and C= from string
    else if (!lstrcmpi(resampler, "lanczos"))
      return new LanczosFilter(3);
    else if (!lstrcmpi(resampler, "lanczos4"))
      return new LanczosFilter(4);
    else if (!lstrcmpi(resampler, "blackman"))
      return new BlackmanFilter(4);
    else if (!lstrcmpi(resampler, "spline16"))
      return new Spline16Filter();
    else if (!lstrcmpi(resampler, "spline36"))
      return new Spline36Filter();
    else if (!lstrcmpi(resampler, "spline64"))
      return new Spline64Filter();
    else if (!lstrcmpi(resampler, "gauss"))
      return new GaussianFilter(30.0);
    else
      env->ThrowError("Convert: Unknown chroma resampler, '%s'", resampler);
  }
  return new MitchellNetravaliFilter(1./3,1./3); // Default colorspace conversion for AviSynth
}


class ConvertToY8 : public GenericVideoFilter
{
public:
  ConvertToY8(PClip src, int matrix, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n,IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);  
  ~ConvertToY8();
private:
  void convYUV422toY8(const unsigned char *src, unsigned char *py, int pitch1, int pitch2y, int width, int height);
  bool blit_luma_only;
  bool yuy2_input;
  bool rgb_input;
  int pixel_step;
  signed short* matrix;
  int offset_y;
};


class ConvertRGBToYV24 : public GenericVideoFilter, public MatrixGenerator3x3
{
public:
  ConvertRGBToYV24(PClip src, int matrix, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);
  ~ConvertRGBToYV24();
private:
  signed short* matrix;
  int offset_y;
  int mul_out;
  int pixel_step;
};

class ConvertYUY2ToYV16 : public GenericVideoFilter
{
public:
  ConvertYUY2ToYV16(PClip src, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);
private:
  void convYUV422to422(const unsigned char *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
       int pitch1, int pitch2y, int pitch2uv, int width, int height);

};

class ConvertYV24ToRGB : public GenericVideoFilter, public MatrixGenerator3x3
{
public:
  ConvertYV24ToRGB(PClip src, int matrix, int pixel_step, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create24(AVSValue args, void*, IScriptEnvironment* env);
  static AVSValue __cdecl Create32(AVSValue args, void*, IScriptEnvironment* env);
  ~ConvertYV24ToRGB();
private:
  signed short* matrix;
  int offset_y;
  int pixel_step;
};

class ConvertYV16ToYUY2 : public GenericVideoFilter
{
public:
  ConvertYV16ToYUY2(PClip src, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);
private:
  void conv422toYUV422(const unsigned char *py, const unsigned char *pu, const unsigned char *pv, unsigned char *dst, 
					   int pitch1Y, int pitch1UV, int pitch2, int width, int height);
};

class ConvertToPlanarGeneric : public GenericVideoFilter
{
public:
  ConvertToPlanarGeneric(PClip src, int dst_space, bool interlaced, AVSValue* UsubsSampling, AVSValue* VsubsSampling, int cp,  const AVSValue* ChromaResampler, IScriptEnvironment* env);
  ~ConvertToPlanarGeneric() {}
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl CreateYV12(AVSValue args, void*, IScriptEnvironment* env);   
  static AVSValue __cdecl CreateYV16(AVSValue args, void*, IScriptEnvironment* env);   
  static AVSValue __cdecl CreateYV24(AVSValue args, void*, IScriptEnvironment* env);   
  static AVSValue __cdecl CreateYV411(AVSValue args, void*, IScriptEnvironment* env);   
private:
  bool Y8input;
  PClip Usource;
  PClip Vsource;
};


#endif
