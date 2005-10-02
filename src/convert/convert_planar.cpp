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
#include "../filters/field.h"


#define USE_DYNAMIC_COMPILER true


ConvertToY8::ConvertToY8(PClip src, int in_matrix, IScriptEnvironment* env) : GenericVideoFilter(src) {
  yuy2_input = blit_luma_only = rgb_input = false;

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

  if (vi.IsRGB()) {
    rgb_input = true;
    pixel_step = vi.BytesFromPixels(1);
    vi.pixel_type = VideoInfo::CS_Y8;
    matrix = (signed short*)_aligned_malloc(sizeof(short)*4,64);
    signed short* m = matrix;
    if (in_matrix == Rec601) {
      *m++ = (signed short)((219.0/255.0)*0.114*32768.0+0.5);  //B
      *m++ = (signed short)((219.0/255.0)*0.587*32768.0+0.5);  //G
      *m++ = (signed short)((219.0/255.0)*0.299*32768.0+0.5);  //R
      offset_y = 16;
    } else if (in_matrix == PC_601) {
      *m++ = (signed short)(0.114*32768.0+0.5);  //B
      *m++ = (signed short)(0.587*32768.0+0.5);  //G
      *m++ = (signed short)(0.299*32768.0+0.5);  //R
      offset_y = 0;
    } else if (in_matrix == Rec709) {
      *m++ = (signed short)((219.0/255.0)*0.0721*32768.0+0.5);  //B
      *m++ = (signed short)((219.0/255.0)*0.7154*32768.0+0.5);  //G
      *m++ = (signed short)((219.0/255.0)*0.2215*32768.0+0.5);  //R
      offset_y = 16;
    } else if (in_matrix == PC_709) {
      *m++ = (signed short)(0.0721*32768.0+0.5);  //B
      *m++ = (signed short)(0.7154*32768.0+0.5);  //G
      *m++ = (signed short)(0.2215*32768.0+0.5);  //R
      offset_y = 0;
    } else {
      env->ThrowError("ConvertToY8: Unknown matrix.");
    }
    return;
  }

  env->ThrowError("ConvertToY8: Unknown input format");
}

PVideoFrame __stdcall ConvertToY8::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  if (blit_luma_only) {
    env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y_ALIGNED), src->GetHeight(PLANAR_Y));
    return dst;
  }

  // OPTME: Fast MMX should be obvious. Load+pack+store
  if (yuy2_input) {
    const BYTE* srcP = src->GetReadPtr();
    int srcPitch = src->GetPitch();

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

  if (rgb_input) {
    const BYTE* srcp = src->GetReadPtr();

    BYTE* dstY = dst->GetWritePtr(PLANAR_Y);

    signed short* m = (signed short*)matrix;
    srcp += src->GetPitch() * (vi.height-1);  // We start at last line
    for (int y=0; y<vi.height; y++) {
      for (int x=0; x<vi.width; x++) {
        int Y = offset_y + (((int)m[0] * srcp[0] + (int)m[1] * srcp[1] + (int)m[2] * srcp[2] + 16383)>>15);
        *dstY++ = (BYTE)min(255,max(0,Y));  // All the safety we can wish for.
        srcp += pixel_step;
      }
      srcp -= src->GetPitch() + (vi.width * pixel_step);
      dstY += dst->GetPitch(PLANAR_Y) - vi.width;
    }
  }
  return dst;
}

AVSValue __cdecl ConvertToY8::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsY8())
    return clip;
  return new ConvertToY8(clip,getMatrix(args[1].AsString("rec601"), env), env);
}


/*****************************************************
 * ConvertRGBToYV24
 *
 * (c) Klaus Post, 2005
 ******************************************************/

ConvertRGBToYV24::ConvertRGBToYV24(PClip src, int in_matrix, IScriptEnvironment* env) : GenericVideoFilter(src)  {

  if (!vi.IsRGB())
    env->ThrowError("ConvertRGBToYV24: Only RGB data input accepted");

  pixel_step = vi.BytesFromPixels(1);
  vi.pixel_type = VideoInfo::CS_YV24;
  matrix = (signed short*)_aligned_malloc(sizeof(short)*12,64);

  
  if (in_matrix == Rec601) {
    /*
    Y'= 0.299*R' + 0.587*G' + 0.114*B'
    Cb=-0.169*R' - 0.331*G' + 0.500*B'
    Cr= 0.500*R' - 0.419*G' - 0.081*B' 
    */
    signed short* m = matrix;
    *m++ = (signed short)((219.0/255.0)*0.114*32768.0+0.5);  //B
    *m++ = (signed short)((219.0/255.0)*0.587*32768.0+0.5);  //G
    *m++ = (signed short)((219.0/255.0)*0.299*32768.0+0.5);  //R
    *m++ = 0;
    *m++ = (signed short)((219.0/255.0)*0.500*32768.0+0.5);
    *m++ = (signed short)((219.0/255.0)*-0.331*32768.0+0.5);
    *m++ = (signed short)((219.0/255.0)*-0.169*32768.0+0.5);
    *m++ = 0;
    *m++ = (signed short)((219.0/255.0)*-0.081*32768.0+0.5);
    *m++ = (signed short)((219.0/255.0)*-0.419*32768.0+0.5);
    *m++ = (signed short)((219.0/255.0)*0.5*32768.0+0.5);
    *m++ = 0;
    offset_y = 16;

  } else if (in_matrix == PC_601) {
    signed short* m = matrix;
    *m++ = (signed short)(0.114*32768.0+0.5);  //B
    *m++ = (signed short)(0.587*32768.0+0.5);  //G
    *m++ = (signed short)(0.299*32768.0+0.5);  //R
    *m++ = 0;
    *m++ = (signed short)(0.500*32768.0+0.5);
    *m++ = (signed short)(-0.331*32768.0+0.5);
    *m++ = (signed short)(-0.169*32768.0+0.5);
    *m++ = 0;
    *m++ = (signed short)(-0.081*32768.0+0.5);
    *m++ = (signed short)(-0.419*32768.0+0.5);
    *m++ = (signed short)(0.5*32768.0+0.5);
    *m++ = 0;
    offset_y = 0;
  } else if (in_matrix == Rec709) {
    /*
    Y'= 0.2215*R' + 0.7154*G' + 0.0721*B'
    Cb=-0.1145*R' - 0.3855*G' + 0.5000*B'
    Cr= 0.5016*R' - 0.4556*G' - 0.0459*B'
    */
    signed short* m = matrix;
    *m++ = (signed short)((219.0/255.0)*0.0721*32768.0+0.5);  //B
    *m++ = (signed short)((219.0/255.0)*0.7154*32768.0+0.5);  //G
    *m++ = (signed short)((219.0/255.0)*0.2215*32768.0+0.5);  //R
    *m++ = 0;
    *m++ = (signed short)((219.0/255.0)*0.5000*32768.0+0.5);
    *m++ = (signed short)((219.0/255.0)*-0.3855*32768.0+0.5);
    *m++ = (signed short)((219.0/255.0)*-0.1145*32768.0+0.5);
    *m++ = 0;
    *m++ = (signed short)((219.0/255.0)*-0.0459*32768.0+0.5);
    *m++ = (signed short)((219.0/255.0)*-0.4556*32768.0+0.5);
    *m++ = (signed short)((219.0/255.0)*0.5016*32768.0+0.5);
    *m++ = 0;
    offset_y = 16;
  } else if (in_matrix == PC_709) {
    signed short* m = matrix;
    *m++ = (signed short)(0.0721*32768.0+0.5);  //B
    *m++ = (signed short)(0.7154*32768.0+0.5);  //G
    *m++ = (signed short)(0.2215*32768.0+0.5);  //R
    *m++ = 0;
    *m++ = (signed short)(0.5000*32768.0+0.5);
    *m++ = (signed short)(-0.3855*32768.0+0.5);
    *m++ = (signed short)(-0.1145*32768.0+0.5);
    *m++ = 0;
    *m++ = (signed short)(-0.0459*32768.0+0.5);
    *m++ = (signed short)(-0.4556*32768.0+0.5);
    *m++ = (signed short)(0.5016*32768.0+0.5);
    *m++ = 0;
    offset_y = 0;
  } else {
    env->ThrowError("ConvertRGBToYV24: Unknown matrix.");
  }
  dyn_matrix = (BYTE*)matrix;
  dyn_dest = (BYTE*)_aligned_malloc(vi.width * 4, 64);
  this->src_pixel_step = pixel_step;
  this->post_add = (offset_y & 0xffff) | ((__int64)(127 & 0xffff)<<16) | ((__int64)(127 & 0xffff)<<32);
  this->GenerateAssembly(vi.width, 15, env);
}

PVideoFrame __stdcall ConvertRGBToYV24::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  const BYTE* srcp = src->GetReadPtr();
  BYTE* dstY = dst->GetWritePtr(PLANAR_Y);
  BYTE* dstU = dst->GetWritePtr(PLANAR_U);
  BYTE* dstV = dst->GetWritePtr(PLANAR_V);

  if (USE_DYNAMIC_COMPILER) {
    int* i_dyn_dest = (int*)dyn_dest;
    for (int y = 0; y < vi.height; y++) {
      dyn_src = (unsigned char*)&srcp[(vi.height-y-1)*src->GetPitch()];
      assembly.Call();
      for (int x = 0; x < vi.width; x++) {
        int p = i_dyn_dest[x];
        dstY[x] = p&0xff;
        dstU[x] = (p>>8)&0xff;
        dstV[x] = (p>>16)&0xff;
      }
      dstY += dst->GetPitch(PLANAR_Y);
      dstU += dst->GetPitch(PLANAR_U);
      dstV += dst->GetPitch(PLANAR_V);
    }
    return dst;
  }

  //Slow C-code.

  signed short* m = (signed short*)matrix;
  srcp += src->GetPitch() * (vi.height-1);  // We start at last line
  for (int y = 0; y < vi.height; y++) {
    for (int x = 0; x < vi.width; x++) {
      int b = srcp[0];
      int g = srcp[1];
      int r = srcp[2];
      int Y = offset_y + (((int)m[0] * b + (int)m[1] * g + (int)m[2] * r + 16383)>>15);
      int U = 127+(((int)m[4] * b + (int)m[5] * g + (int)m[6] * r + 16383)>>15);
      int V = 127+(((int)m[8] * b + (int)m[9] * g + (int)m[10] * r + 16383)>>15);
      *dstY++ = (BYTE)min(255,max(0,Y));  // All the safety we can wish for.
      *dstU++ = (BYTE)min(255,max(0,U));
      *dstV++ = (BYTE)min(255,max(0,V));
      srcp += pixel_step;
    }
    srcp -= src->GetPitch() + (vi.width * pixel_step);
    dstY += dst->GetPitch(PLANAR_Y) - vi.width;
    dstU += dst->GetPitch(PLANAR_U) - vi.width;
    dstV += dst->GetPitch(PLANAR_V) - vi.width;
  }
  return dst;
}

AVSValue __cdecl ConvertRGBToYV24::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsYV24())
    return clip;
  return new ConvertRGBToYV24(clip, getMatrix(args[1].AsString("rec601"), env), env);
}


/*****************************************************
 * ConvertYV24ToRGB
 *
 * (c) Klaus Post, 2005
 ******************************************************/

ConvertYV24ToRGB::ConvertYV24ToRGB(PClip src, int in_matrix, int _pixel_step, IScriptEnvironment* env) : GenericVideoFilter(src), pixel_step(_pixel_step) {

  if (!vi.IsYV24())
    env->ThrowError("ConvertYV24ToRGB: Only YV24 data input accepted");

  vi.pixel_type = (pixel_step == 3) ? VideoInfo::CS_BGR24 : VideoInfo::CS_BGR32;
  matrix = (signed short*)_aligned_malloc(sizeof(short)*12,64);

  const double mulfac = 8192.0;

  if (in_matrix == Rec601) {
    /*
    B'= Y' + 1.773*U' + 0.000*V'
    G'= Y' - 0.344*U' - 0.714*V'
    R'= Y' + 0.000*U' + 1.403*V'
    */
    signed short* m = matrix;
    *m++ = (signed short)((255.0/219.0)*1.000*mulfac+0.5);  //Y
    *m++ = (signed short)((255.0/219.0)*1.773*mulfac+0.5);  //U
    *m++ = (signed short)((255.0/219.0)*0.000*mulfac+0.5);  //V
    *m++ = 0;
    *m++ = (signed short)((255.0/219.0)*1.000*mulfac+0.5);  //Y
    *m++ = (signed short)((255.0/219.0)*-0.334*mulfac+0.5);
    *m++ = (signed short)((255.0/219.0)*-0.714*mulfac+0.5);
    *m++ = 0;
    *m++ = (signed short)((255.0/219.0)*1.000*mulfac+0.5);  //Y
    *m++ = (signed short)((255.0/219.0)*-0.000*mulfac+0.5);
    *m++ = (signed short)((255.0/219.0)*1.403*mulfac+0.5);
    *m++ = 0;
    offset_y = -16;
  } else if (in_matrix == PC_601) {
    signed short* m = matrix;
    *m++ = (signed short)(1.000*mulfac+0.5);  //Y
    *m++ = (signed short)(1.773*mulfac+0.5);  //U
    *m++ = (signed short)(0.000*mulfac+0.5);  //V
    *m++ = 0;
    *m++ = (signed short)(1.000*mulfac+0.5);  //Y
    *m++ = (signed short)(-0.334*mulfac+0.5);
    *m++ = (signed short)(-0.714*mulfac+0.5);
    *m++ = 0;
    *m++ = (signed short)(1.000*mulfac+0.5);  //Y
    *m++ = (signed short)(-0.000*mulfac+0.5);
    *m++ = (signed short)(1.403*mulfac+0.5);
    *m++ = 0;
    offset_y = 0;
  } else if (in_matrix == Rec709) {
    /*
    B'= Y' - 1.8556*Cb + 0.0000*Cr  // The colorfaq seems wrong here, "-" should be "+"
    G'= Y' - 0.1870*Cb - 0.4664*Cr
    R'= Y' + 0.0000*Cb + 1.5701*Cr
    */
    signed short* m = matrix;
    *m++ = (signed short)((255.0/219.0)*1.000*mulfac+0.5);  //Y
    *m++ = (signed short)((255.0/219.0)*1.8556*mulfac+0.5);  //U
    *m++ = (signed short)((255.0/219.0)*0.000*mulfac+0.5);  //V
    *m++ = 0;
    *m++ = (signed short)((255.0/219.0)*1.000*mulfac+0.5);  //Y
    *m++ = (signed short)((255.0/219.0)*-0.1870*mulfac+0.5);
    *m++ = (signed short)((255.0/219.0)*-0.4664*mulfac+0.5);
    *m++ = 0;
    *m++ = (signed short)((255.0/219.0)*1.000*mulfac+0.5);  //Y
    *m++ = (signed short)((255.0/219.0)*0.000*mulfac+0.5);
    *m++ = (signed short)((255.0/219.0)*1.5701*mulfac+0.5);
    *m++ = 0;
    offset_y = -16;
  } else if (in_matrix == PC_709) {
    signed short* m = matrix;
    *m++ = (signed short)(1.000*mulfac+0.5);  //Y
    *m++ = (signed short)(1.8556*mulfac+0.5);  //U
    *m++ = (signed short)(0.000*mulfac+0.5);  //V
    *m++ = 0;
    *m++ = (signed short)(1.000*mulfac+0.5);  //Y
    *m++ = (signed short)(-0.1870*mulfac+0.5);
    *m++ = (signed short)(-0.4664*mulfac+0.5);
    *m++ = 0;
    *m++ = (signed short)(1.000*mulfac+0.5);  //Y
    *m++ = (signed short)(0.000*mulfac+0.5);
    *m++ = (signed short)(1.5701*mulfac+0.5);
    *m++ = 0;
    offset_y = 0;
  } else {
    env->ThrowError("ConvertYV24ToRGB: Unknown matrix.");
  }
  dyn_matrix = (BYTE*)matrix;
  dyn_src = (BYTE*)_aligned_malloc(vi.width * 4, 64);
  this->dest_pixel_step = pixel_step;
  this->pre_add = (offset_y & 0xffff) | ((__int64)(-127 & 0xffff)<<16) | ((__int64)(-127 & 0xffff)<<32);
  this->GenerateAssembly(vi.width, 13, env);
}

ConvertYV24ToRGB::~ConvertYV24ToRGB() {
  if (dyn_src)
    _aligned_free(dyn_src);
  dyn_src = 0;
}

PVideoFrame __stdcall ConvertYV24ToRGB::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  const BYTE* srcY = src->GetReadPtr(PLANAR_Y);
  const BYTE* srcU = src->GetReadPtr(PLANAR_U);
  const BYTE* srcV = src->GetReadPtr(PLANAR_V);

  BYTE* dstp = dst->GetWritePtr();
  
  if (USE_DYNAMIC_COMPILER) {
    int* i_dyn_src = (int*)dyn_src;
    for (int y = 0; y < vi.height; y++) {
      for (int x = 0; x < vi.width; x++) {
        i_dyn_src[x] = srcY[x] | (srcU[x] << 8 ) | (srcV[x] << 16);
      }
      dyn_dest = &dstp[(vi.height-y-1)*dst->GetPitch()];
      assembly.Call();
      srcY += src->GetPitch(PLANAR_Y);
      srcU += src->GetPitch(PLANAR_U);
      srcV += src->GetPitch(PLANAR_V);
    }
    return dst;
  }

  //Slow C-code.

  signed short* m = (signed short*)matrix;
  dstp += dst->GetPitch() * (vi.height-1);  // We start at last line
  for (int y = 0; y < vi.height; y++) {
    for (int x = 0; x < vi.width; x++) {
      int Y = srcY[x] + offset_y;
      int U = srcU[x] - 127;
      int V = srcV[x] - 127;
      int b = (((int)m[0] * Y + (int)m[1] * U + (int)m[2] * V + 4096)>>13);
      int g = (((int)m[4] * Y + (int)m[5] * U + (int)m[6] * V + 4096)>>13);
      int r = (((int)m[8] * Y + (int)m[9] * U + (int)m[10] * V + 4096)>>13);
      dstp[0] = (BYTE)min(255,max(0,b));  // All the safety we can wish for.
      dstp[1] = (BYTE)min(255,max(0,g));  // Probably needed here.
      dstp[2] = (BYTE)min(255,max(0,r));
      dstp += pixel_step;
    }
    dstp -= dst->GetPitch() + (vi.width * pixel_step);
    srcY += src->GetPitch(PLANAR_Y);
    srcU += src->GetPitch(PLANAR_U);
    srcV += src->GetPitch(PLANAR_V);
  }
  return dst;
}

AVSValue __cdecl ConvertYV24ToRGB::Create32(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsRGB())
    return clip;
  return new ConvertYV24ToRGB(clip, getMatrix(args[1].AsString("rec601"), env), 4, env);
}

AVSValue __cdecl ConvertYV24ToRGB::Create24(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsRGB())
    return clip;
  return new ConvertYV24ToRGB(clip, getMatrix(args[1].AsString("rec601"), env), 3, env);
}

/************************************
 * YUY2 to YV16
 ************************************/

ConvertYUY2ToYV16::ConvertYUY2ToYV16(PClip src, IScriptEnvironment* env) : GenericVideoFilter(src) {

  if (!vi.IsYUY2())
    env->ThrowError("ConvertYUY2ToYV16: Only YUY2 is allowed as input");
  
  vi.pixel_type = VideoInfo::CS_YV16;
  
}

PVideoFrame __stdcall ConvertYUY2ToYV16::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  const BYTE* srcP = src->GetReadPtr();

  BYTE* dstY = dst->GetWritePtr(PLANAR_Y);
  BYTE* dstU = dst->GetWritePtr(PLANAR_U);
  BYTE* dstV = dst->GetWritePtr(PLANAR_V);

  int w = vi.width/2;
  
  for (int y=0; y<vi.height; y++) { // ASM will probably not be faster here.
    for (int x=0; x<w; x++) {
      int x2 = x<<1;
      int x4 = x<<2;
      dstY[x2] = srcP[x4];
      dstY[x2+1] = srcP[x4+2];
      dstU[x] = srcP[x4+1];
      dstV[x] = srcP[x4+3];
    }
    srcP += src->GetPitch();
    dstY += dst->GetPitch(PLANAR_Y);
    dstU += dst->GetPitch(PLANAR_U);
    dstV += dst->GetPitch(PLANAR_V);
  }
  return dst;
}

AVSValue __cdecl ConvertYUY2ToYV16::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsYV16())
    return clip;
  return new ConvertYUY2ToYV16(clip, env);
}

/************************************
 * YV16 to YUY2
 ************************************/

ConvertYV16ToYUY2::ConvertYV16ToYUY2(PClip src, IScriptEnvironment* env) : GenericVideoFilter(src) {

  if (!vi.IsYV16())
    env->ThrowError("ConvertYV16ToYUY2: Only YV16 is allowed as input");
  
  vi.pixel_type = VideoInfo::CS_YUY2;
  
}

PVideoFrame __stdcall ConvertYV16ToYUY2::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  const BYTE* srcY = src->GetReadPtr(PLANAR_Y);
  const BYTE* srcU = src->GetReadPtr(PLANAR_U);
  const BYTE* srcV = src->GetReadPtr(PLANAR_V);

  BYTE* dstp = dst->GetWritePtr();

  int w = vi.width/2;
  
  for (int y=0; y<vi.height; y++) { // ASM will probably not be faster here.
    for (int x=0; x<w; x++) {
      int x2 = x<<1;
      int x4 = x<<2;
      dstp[x4] = srcY[x2];
      dstp[x4+2] = srcY[x2+1];
      dstp[x4+1] = srcU[x];
      dstp[x4+3] = srcV[x];
    }
    srcY += src->GetPitch(PLANAR_Y);
    srcU += src->GetPitch(PLANAR_U);
    srcV += src->GetPitch(PLANAR_V);
    dstp += dst->GetPitch();
  }
  return dst;
}

AVSValue __cdecl ConvertYV16ToYUY2::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsYUY2())
    return clip;
  return new ConvertYV16ToYUY2(clip, env);
}

/**********************************************
 * Converter between arbitrary planar formats 
 *
 * This uses plane copy for luma, and the 
 * bicubic resizer for chroma (could be 
 * customizable later)
 *
 * (c) Klaus Post, 2005
 **********************************************/



ConvertToPlanarGeneric::ConvertToPlanarGeneric(PClip src, int dst_space, bool interlaced, AVSValue* UsubSampling, AVSValue* VsubSampling, IScriptEnvironment* env) : GenericVideoFilter(src) {
  Y8input = vi.IsY8();
  if (!Y8input) {
    Usource = new SwapUVToY(child, SwapUVToY::UToY8, env);  
    Vsource = new SwapUVToY(child, SwapUVToY::VToY8, env);
  }
  int uv_width = vi.width;
  int uv_height = vi.height;
  switch (dst_space) {
  case VideoInfo::CS_YV12:
  case VideoInfo::CS_I420:
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
  if (!Y8input) {
    MitchellNetravaliFilter filter(1./3., 1./3.);
    UsubSampling[2] = AVSValue(Usource->GetVideoInfo().width+UsubSampling[0].AsFloat(0.0));
    UsubSampling[3] = AVSValue(Usource->GetVideoInfo().height+UsubSampling[1].AsFloat(0.0));
    VsubSampling[2] = AVSValue(Vsource->GetVideoInfo().width+VsubSampling[0].AsFloat(0.0));
    VsubSampling[3] = AVSValue(Vsource->GetVideoInfo().height+VsubSampling[1].AsFloat(0.0));
    Usource = FilteredResize::CreateResize(Usource, uv_width, uv_height, UsubSampling, &filter, env);
    Vsource = FilteredResize::CreateResize(Vsource, uv_width, uv_height, VsubSampling, &filter, env);
  }
}

PVideoFrame __stdcall ConvertToPlanarGeneric::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y_ALIGNED), src->GetHeight(PLANAR_Y));
  if (!Y8input) {
    src = Usource->GetFrame(n, env);
    env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y_ALIGNED), src->GetHeight(PLANAR_Y));
    src = Vsource->GetFrame(n, env);
    env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y_ALIGNED), src->GetHeight(PLANAR_Y));
  } else {
    memset(dst->GetWritePtr(PLANAR_U), 127, dst->GetHeight(PLANAR_U)*dst->GetPitch(PLANAR_U));
    memset(dst->GetWritePtr(PLANAR_V), 127, dst->GetHeight(PLANAR_V)*dst->GetPitch(PLANAR_V));
  }
  return dst;
}

AVSValue __cdecl ConvertToPlanarGeneric::CreateYV12(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();

  if (clip->GetVideoInfo().IsYV12() )
    return clip;

  if (clip->GetVideoInfo().IsRGB()) 
    clip = new ConvertRGBToYV24(clip, getMatrix(args[2].AsString("rec601"), env), env);

  if (clip->GetVideoInfo().IsYUY2()) 
    clip = new ConvertYUY2ToYV16(clip,  env);

  if (!clip->GetVideoInfo().IsPlanar())
    env->ThrowError("ConvertToYV12: Can only convert from Planar YUV.");

  bool interlaced = args[1].AsBool(false);
  if (interlaced) clip = new SeparateFields(clip, env);
  AVSValue subs[4] = { 0.0, -0.5, 0.0, 0.0 }; 
  clip = new ConvertToPlanarGeneric(clip ,VideoInfo::CS_YV12,args[1].AsBool(false),subs, subs, env);
  if (interlaced) clip = new SelectEvery(new DoubleWeaveFields(clip), 2, 0);
  return clip;
}

AVSValue __cdecl ConvertToPlanarGeneric::CreateYV16(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();

  if (clip->GetVideoInfo().IsYV16())
    return clip;

  if (clip->GetVideoInfo().IsRGB()) 
    clip = new ConvertRGBToYV24(clip, getMatrix(args[2].AsString("rec601"), env), env);

  if (clip->GetVideoInfo().IsYUY2()) 
    return new ConvertYUY2ToYV16(clip,  env);

  if (!clip->GetVideoInfo().IsPlanar())
    env->ThrowError("ConvertToYV16: Can only convert from Planar YUV.");

  bool interlaced = args[1].AsBool(false) && clip->GetVideoInfo().IsYV12();
  if (interlaced) clip = new SeparateFields(clip, env);

  AVSValue subs[4] = { 0.0, 0.0, 0.0, 0.0 }; 
  clip = new ConvertToPlanarGeneric(clip ,VideoInfo::CS_YV16,args[1].AsBool(false),subs, subs, env);
  if (interlaced) clip = new SelectEvery(new DoubleWeaveFields(clip), 2, 0);
  return clip;
}

AVSValue __cdecl ConvertToPlanarGeneric::CreateYV24(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();

  if (clip->GetVideoInfo().IsYV24() )
    return clip;

  if (clip->GetVideoInfo().IsRGB()) 
    return new ConvertRGBToYV24(clip, getMatrix(args[2].AsString("rec601"), env), env);

  if (clip->GetVideoInfo().IsYUY2()) 
    clip = new ConvertYUY2ToYV16(clip,  env);

  if (!clip->GetVideoInfo().IsPlanar())
    env->ThrowError("ConvertToYV24: Can only convert from Planar YUV.");

  bool interlaced = args[1].AsBool(false) && clip->GetVideoInfo().IsYV12();
  if (interlaced) clip = new SeparateFields(clip, env);

  AVSValue subs[4] = { 0.0, 0.0, 0.0, 0.0 }; 
  clip = new ConvertToPlanarGeneric(clip ,VideoInfo::CS_YV24,args[1].AsBool(false),subs, subs, env);
  if (interlaced) clip = new SelectEvery(new DoubleWeaveFields(clip), 2, 0);
  return clip;
}

AVSValue __cdecl ConvertToPlanarGeneric::CreateYV411(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();

  if (clip->GetVideoInfo().IsYV411() )
    return clip;

  if (clip->GetVideoInfo().IsRGB()) 
    clip = new ConvertRGBToYV24(clip, getMatrix(args[2].AsString("rec601"), env), env);

  if (clip->GetVideoInfo().IsYUY2()) 
    clip = new ConvertYUY2ToYV16(clip,  env);

  if (!clip->GetVideoInfo().IsPlanar())
    env->ThrowError("ConvertToYV411: Can only convert from Planar YUV.");

  bool interlaced = args[1].AsBool(false) && clip->GetVideoInfo().IsYV12();
  if (interlaced) clip = new SeparateFields(clip, env);

  AVSValue subs[4] = { 0.0, 0.0, 0.0, 0.0 }; 
  clip = new ConvertToPlanarGeneric(clip ,VideoInfo::CS_YV411,args[1].AsBool(false),subs, subs, env);
  if (interlaced) clip = new SelectEvery(new DoubleWeaveFields(clip), 2, 0);
  return clip;
}

MatrixGenerator3x3::MatrixGenerator3x3() {
  pre_add = post_add = 0;
  src_pixel_step = 4;   // Change to 3 for RGB24 for instance.
  dest_pixel_step = 4;  // Note that 4 bytes will be written at the time, always.
}

MatrixGenerator3x3::~MatrixGenerator3x3() {
  assembly.Free();
}

/***
 *
 * (c) Klaus Post, 2005
 *
 * 4 bytes/loop
 * 1 pixels/loop
 * Following MUST be initialized prior to GenerateAssembly:
 * pre_add, post_add, src_pixel_step, dest_pixel_step
 * 
 * 100% faster than C-code.
 ***/

void MatrixGenerator3x3::GenerateAssembly(int width, int frac_bits, IScriptEnvironment* env) {
  __declspec(align(8))static const __int64 zeros = 0x0000000000000000;

  Assembler x86;   // This is the class that assembles the code.
  rounder = 0x0000000100000001 * (1<<(frac_bits-1)); 

  bool isse = !!(env->GetCPUFlags() & CPUF_INTEGER_SSE);
  bool unroll = false;   // Unrolled code ~30% slower on Athlon XP.

  int loops = width;

  if (pre_add && post_add)
    env->ThrowError("Internal error - only pre_add OR post_add can be used.");

  // Store registers
  x86.push(eax);
  x86.push(ebx);
  x86.push(ecx);
  x86.push(edx);
  x86.push(esi);
  x86.push(edi);
  x86.push(ebp);
    
  x86.mov(eax, (int)&dyn_src);
  x86.mov(ebx, (int)&dyn_matrix);  // Used in loop
  if (pre_add) {
    x86.mov(ecx, (int)&pre_add);  
    x86.movq(mm7, qword_ptr[ecx]);
  }
  if (post_add) {
    x86.mov(ecx, (int)&post_add);  
    x86.movq(mm7, qword_ptr[ecx]);
  }
  x86.mov(edx, (int)&dyn_dest);
  x86.mov(esi, dword_ptr [eax]); // eax no longer used
  x86.mov(ebx, dword_ptr [ebx]);
  x86.mov(edi, dword_ptr [edx]);  // edx no longer used
  if (!(pre_add || post_add)) {
    x86.pxor(mm7,mm7);  // Cleared mmx register - Not touched!
  } else {
    x86.mov(edx, (int)&zeros);
  }
  x86.mov(eax,(int)&rounder);
  x86.movq(mm6, qword_ptr[eax]);  // Ones in mmx reg, eax no longer used

  if (!unroll) {  // Should we create a loop instead of unrolling?
    x86.mov(ecx, loops);
    x86.label("loopback");
    loops = 1;
  }
  for (int i=0; i<loops; i++) {
    x86.movd(mm0, dword_ptr[esi]);
    if (!(pre_add || post_add)) {
      x86.punpcklbw(mm0, mm7);     // Unpack bytes -> words
    } else {
      x86.punpcklbw(mm0, qword_ptr[edx]);     // Unpack bytes -> words
    }
    if (pre_add) {
      x86.paddw(mm0,mm7);
    }
    x86.movq(mm1, mm0);
    x86.movq(mm2, mm0);
    x86.pmaddwd(mm0,qword_ptr [ebx]);     // Part 1/3
    x86.pmaddwd(mm1,qword_ptr [ebx+8]);                 // 2/3
    x86.pmaddwd(mm2,qword_ptr [ebx+16]);                 // 3/3
    
    if (!isse) {
      x86.movq(mm3,mm0);
      x86.movq(mm4,mm1);
      x86.movq(mm5,mm2);

      x86.psrlq(mm3, 32);
      x86.psrlq(mm4, 32);
      x86.psrlq(mm5, 32);
    } else {
      x86.pswapd(mm3,mm0);  // Swap upper and lower on move 
      x86.pswapd(mm4,mm1);  // Swap upper and lower on move
      x86.pswapd(mm5,mm2);  // Swap upper and lower on move
    }
    x86.paddd(mm0, mm3);
    x86.paddd(mm1, mm4);
    x86.paddd(mm2, mm5);

    x86.punpckldq(mm0,mm1);  // Move mm1 lower to mm0 upper

    x86.paddd(mm0,mm6);  // Add rounder
    x86.paddd(mm2,mm6);  // Add rounder

    x86.psrad(mm0, frac_bits);  // Shift down
    x86.psrad(mm2, frac_bits);  

    x86.packssdw(mm0, mm2);    // Pack results into words

    if (post_add) {
      x86.paddw(mm0,mm7);
    }

    x86.packuswb(mm0, mm0);    // Into bytes
    x86.movd(dword_ptr[edi], mm0);  // Store

    x86.add(esi, src_pixel_step);
    x86.add(edi, dest_pixel_step);
  }
  if (!unroll) {  // Loop on if not unrolling
    x86.dec(ecx);
    x86.jnz("loopback");
  }
  x86.emms();
    // Restore registers
   x86.pop(ebp);
   x86.pop(edi);
   x86.pop(esi);
   x86.pop(edx);
   x86.pop(ecx);
   x86.pop(ebx);
   x86.pop(eax);
   x86.ret();
  assembly = DynamicAssembledCode(x86, env, "ConvertMatrix: Dynamic MMX code could not be compiled.");
}

