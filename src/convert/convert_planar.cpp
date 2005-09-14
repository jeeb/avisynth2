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


#include "stdafx.h"

#include "convert_planar.h"
#include "../filters/resample.h"
#include "../filters/planeswap.h"



ConvertToY8::ConvertToY8(PClip src, IScriptEnvironment* env) : GenericVideoFilter(src) {
  yuy2_input = blit_luma_only = true;

  if (vi.IsYV12()||vi.IsYV16()||vi.IsYV24()||vi.IsYV411()) {
    blit_luma_only = true;
    vi.pixel_type = VideoInfo::CS_Y8;
    return;
  }
  if (vi.IsYUY2()) {
    yuy2_input = true;
    vi.pixel_type = VideoInfo::CS_Y8;
    return;
  }
  if (vi.IsRGB())
    env->ThrowError("ConvertToY8: RGB to Y8 not supported");

  env->ThrowError("ConvertToY8: Unknown input format");
}

PVideoFrame __stdcall ConvertToY8::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  if (blit_luma_only) {
    env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y_ALIGNED), src->GetHeight(PLANAR_Y));
    return dst;
  }

  // OPTME: Fast MMX should be obvious.
  if (yuy2_input) {
    const BYTE* srcP = src->GetReadPtr();
    int srcPitch = src->GetPitch();

    PVideoFrame dst = env->NewVideoFrame(vi);
    BYTE* dstY = dst->GetWritePtr(PLANAR_Y);

    int dstPitch = dst->GetPitch(PLANAR_Y);

    int w = dst->GetRowSize(PLANAR_Y);
    int h = dst->GetHeight(PLANAR_Y);

    for (int y=0; y<h; y++) {
      for (int x=0; x<w; x+=2) {
        int x2 = x<<1;
        dstY[x] = srcP[x2];
        dstY[x+1] = srcP[x2+2];
      }
      srcP+=srcPitch;
      dstY+=dstPitch;
    }
  }
  return dst;
}

AVSValue __cdecl ConvertToY8::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsY8())
    return clip;
  return new ConvertToY8(clip, env);
}

/**********************************************
 * Converter between arbitrary planar formats 
 **********************************************/



ConvertToPlanarGeneric::ConvertToPlanarGeneric(PClip src, int dst_space, bool interlaced, AVSValue* UsubSampling, AVSValue* VsubSampling, IScriptEnvironment* env) : GenericVideoFilter(src) {
  Usource = new SwapUVToY(child, SwapUVToY::UToY8, env);  
  Vsource = new SwapUVToY(child, SwapUVToY::VToY8, env);
  int uv_width = vi.width;
  int uv_height = vi.height;
  switch (dst_space) {
  case VideoInfo::CS_YV12:
      uv_width /= 2; uv_height /= 2;
      break;
    case VideoInfo::CS_YV24:
      uv_width /= 1; uv_height /= 1;
      break;
    case VideoInfo::CS_YV16:
      uv_width /= 2; uv_height /= 1;
      break;
    case VideoInfo::CS_YV411:
      uv_width /= 4; uv_height /= 1;
      break;
    default:
      env->ThrowError("Convert: Cannot convert to destination format.");

  }
  vi.pixel_type = dst_space;
  MitchellNetravaliFilter filter(1./3., 1./3.);
  UsubSampling[2] = AVSValue(Usource->GetVideoInfo().width+UsubSampling[0].AsFloat(0.0));
  UsubSampling[3] = AVSValue(Usource->GetVideoInfo().height+UsubSampling[1].AsFloat(0.0));
  VsubSampling[2] = AVSValue(Vsource->GetVideoInfo().width+VsubSampling[0].AsFloat(0.0));
  VsubSampling[3] = AVSValue(Vsource->GetVideoInfo().height+VsubSampling[1].AsFloat(0.0));
  Usource = FilteredResize::CreateResize(Usource, uv_width, uv_height, UsubSampling, &filter, env);
  Vsource = FilteredResize::CreateResize(Vsource, uv_width, uv_height, VsubSampling, &filter, env);
}

PVideoFrame __stdcall ConvertToPlanarGeneric::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y_ALIGNED), src->GetHeight(PLANAR_Y));
  src = Usource->GetFrame(n, env);
  env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y_ALIGNED), src->GetHeight(PLANAR_Y));
  src = Vsource->GetFrame(n, env);
  env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y_ALIGNED), src->GetHeight(PLANAR_Y));
  return dst;
}

AVSValue __cdecl ConvertToPlanarGeneric::CreateYV12(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (!clip->GetVideoInfo().IsPlanar())
    env->ThrowError("ConvertToYV12: Can only convert from Planar YUV.");
  AVSValue subs[4] = { 0.0, -0.5, 0.0, 0.0 }; 
  return new ConvertToPlanarGeneric(clip ,VideoInfo::CS_YV12,args[1].AsBool(false),subs, subs, env);
}

AVSValue __cdecl ConvertToPlanarGeneric::CreateYV16(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (!clip->GetVideoInfo().IsPlanar())
    env->ThrowError("ConvertToYV16: Can only convert from Planar YUV.");
  AVSValue subs[4] = { 0.0, 0.0, 0.0, 0.0 }; 
  return new ConvertToPlanarGeneric(clip ,VideoInfo::CS_YV16,args[1].AsBool(false),subs, subs, env);
}

AVSValue __cdecl ConvertToPlanarGeneric::CreateYV24(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (!clip->GetVideoInfo().IsPlanar())
    env->ThrowError("ConvertToYV24: Can only convert from Planar YUV.");
  AVSValue subs[4] = { 0.0, 0.0, 0.0, 0.0 }; 
  return new ConvertToPlanarGeneric(clip ,VideoInfo::CS_YV24,args[1].AsBool(false),subs, subs, env);
}

AVSValue __cdecl ConvertToPlanarGeneric::CreateYV411(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (!clip->GetVideoInfo().IsPlanar())
    env->ThrowError("ConvertToYV411: Can only convert from Planar YUV.");
  AVSValue subs[4] = { 0.0, 0.0, 0.0, 0.0 }; 
  return new ConvertToPlanarGeneric(clip ,VideoInfo::CS_YV411,args[1].AsBool(false),subs, subs, env);
}
