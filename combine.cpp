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

#include "combine.h"





/********************************************************************
***** Declare index of new filters for Avisynth's filter engine *****
********************************************************************/

AVSFunction Combine_filters[] = {
  { "StackVertical", "cc+", StackVertical::Create },
  { "StackHorizontal", "cc+", StackHorizontal::Create },
  { "ShowFiveVersions", "ccccc", ShowFiveVersions::Create },
  { "Animate", "iis.*", Animate::Create },  // start frame, end frame, filter, start-args, end-args
  { "Animate", "ciis.*", Animate::Create }, 
  { 0 }
};






/********************************
 *******   StackVertical   ******
 ********************************/

StackVertical::StackVertical(PClip _child1, PClip _child2, IScriptEnvironment* env)
{
  // swap the order of the parameters in RGB mode because it's upside-down
  if (_child1->GetVideoInfo().IsYUY2()) {
    child1 = _child1; child2 = _child2;
  } else {
    child1 = _child2; child2 = _child1;
  }

  const VideoInfo& vi1 = child1->GetVideoInfo();
  const VideoInfo& vi2 = child2->GetVideoInfo();

  if (vi1.width != vi2.width)
    env->ThrowError("StackVertical: image widths don't match");
  if (vi1.pixel_type != vi2.pixel_type)
    env->ThrowError("StackVertical: image formats don't match");

  vi = vi1;
  vi.height += vi2.height;
  vi.num_frames = max(vi1.num_frames, vi2.num_frames);
  vi.num_audio_samples = max(vi1.num_audio_samples, vi2.num_audio_samples);
}


PVideoFrame __stdcall StackVertical::GetFrame(int n, IScriptEnvironment* env) 
{
  PVideoFrame src1 = child1->GetFrame(n, env);
  PVideoFrame src2 = child2->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);
  const BYTE* src1p = src1->GetReadPtr();
  const BYTE* src1pU = src1->GetReadPtr(PLANAR_U);
  const BYTE* src1pV = src1->GetReadPtr(PLANAR_V);
  const BYTE* src2p = src2->GetReadPtr();
  const BYTE* src2pU = src2->GetReadPtr(PLANAR_U);
  const BYTE* src2pV = src2->GetReadPtr(PLANAR_V);
  BYTE* dstp = dst->GetWritePtr();
  BYTE* dstpU = dst->GetWritePtr(PLANAR_U);
  BYTE* dstpV = dst->GetWritePtr(PLANAR_V);
  const int src1_pitch = src1->GetPitch();
  const int src1_pitchUV = src1->GetPitch(PLANAR_U);
  const int src2_pitch = src2->GetPitch();
  const int src2_pitchUV = src2->GetPitch(PLANAR_U);
  const int dst_pitch = dst->GetPitch();
  const int dst_pitchUV = dst->GetPitch(PLANAR_V);
  const int src1_height = src1->GetHeight();
  const int src1_heightUV = src1->GetHeight(PLANAR_U);
  const int src2_height = src2->GetHeight();
  const int src2_heightUV = src2->GetHeight(PLANAR_U);
  const int row_size = dst->GetRowSize();
  const int row_sizeUV = dst->GetRowSize(PLANAR_U);

  BYTE* dstp2 = dstp + dst_pitch * src1->GetHeight();
  BYTE* dstp2U = dstpU + dst_pitchUV * src1->GetHeight(PLANAR_U);
  BYTE* dstp2V = dstpV + dst_pitchUV * src1->GetHeight(PLANAR_V);

  if (vi.IsYV12())
  {
    // Copy YV12 upside-down
    BitBlt(dstp, dst_pitch, src2p, src1_pitch, row_size, src1_height);
    BitBlt(dstp2, dst_pitch, src1p, src2_pitch, row_size, src2_height);

    BitBlt(dstpU, dst_pitchUV, src2pU, src1_pitchUV, row_sizeUV, src1_heightUV);
    BitBlt(dstp2U, dst_pitchUV, src1pU, src2_pitchUV, row_sizeUV, src2_heightUV);

    BitBlt(dstpV, dst_pitchUV, src2pV, src1_pitchUV, row_sizeUV, src1_heightUV);
    BitBlt(dstp2V, dst_pitchUV, src1pV, src2_pitchUV, row_sizeUV, src2_heightUV);
  } else {
    // I'll leave the planar BitBlts (no-ops) in for compatibility with future planar formats
    BitBlt(dstp, dst_pitch, src1p, src1_pitch, row_size, src1_height);
    BitBlt(dstp2, dst_pitch, src2p, src2_pitch, row_size, src2_height);

    BitBlt(dstpU, dst_pitchUV, src1pU, src1_pitchUV, row_sizeUV, src1_heightUV);
    BitBlt(dstp2U, dst_pitchUV, src2pU, src2_pitchUV, row_sizeUV, src2_heightUV);

    BitBlt(dstpV, dst_pitchUV, src1pV, src1_pitchUV, row_sizeUV, src1_heightUV);
    BitBlt(dstp2V, dst_pitchUV, src2pV, src2_pitchUV, row_sizeUV, src2_heightUV);
  }
  return dst;
}


AVSValue __cdecl StackVertical::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  PClip result = args[0].AsClip();
  for (int i=0; i<args[1].ArraySize(); ++i)
    result = new StackVertical(result, args[1][i].AsClip(), env);
  return result;
}









/**********************************
 *******   StackHorizontal   ******
 **********************************/

StackHorizontal::StackHorizontal(PClip _child1, PClip _child2, IScriptEnvironment* env)
   : child1(_child1), child2(_child2)
{
  const VideoInfo& vi1 = child1->GetVideoInfo();
  const VideoInfo& vi2 = child2->GetVideoInfo();

  if (vi1.height != vi2.height)
    env->ThrowError("StackHorizontal: image heights don't match");
  if (vi1.pixel_type != vi2.pixel_type)
    env->ThrowError("StackHorizontal: image formats don't match");

  vi = vi1;
  vi.width += vi2.width;
  vi.num_frames = max(vi1.num_frames, vi2.num_frames);
  vi.num_audio_samples = max(vi1.num_audio_samples, vi2.num_audio_samples);
}

PVideoFrame __stdcall StackHorizontal::GetFrame(int n, IScriptEnvironment* env) 
{
  PVideoFrame src1 = child1->GetFrame(n, env);
  PVideoFrame src2 = child2->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  const BYTE* src1p = src1->GetReadPtr();
  const BYTE* src1pU = src1->GetReadPtr(PLANAR_U);
  const BYTE* src1pV = src1->GetReadPtr(PLANAR_V);

  const BYTE* src2p = src2->GetReadPtr();
  const BYTE* src2pU = src2->GetReadPtr(PLANAR_U);
  const BYTE* src2pV = src2->GetReadPtr(PLANAR_V);

  BYTE* dstp = dst->GetWritePtr();
  BYTE* dstpU = dst->GetWritePtr(PLANAR_U);
  BYTE* dstpV = dst->GetWritePtr(PLANAR_V);

  const int src1_pitch = src1->GetPitch();
  const int src2_pitch = src2->GetPitch();
  const int src1_pitchUV = src1->GetPitch(PLANAR_U);
  const int src2_pitchUV = src2->GetPitch(PLANAR_U);

  const int dst_pitch = dst->GetPitch();
  const int dst_pitchUV = dst->GetPitch(PLANAR_U);

  const int src1_row_size = src1->GetRowSize();
  const int src2_row_size = src2->GetRowSize();
  const int src1_row_sizeUV = src1->GetRowSize(PLANAR_U);
  const int src2_row_sizeUV = src2->GetRowSize(PLANAR_V);

  BYTE* dstp2 = dstp + src1_row_size;
  BYTE* dstp2U = dstpU + src1_row_sizeUV;
  BYTE* dstp2V = dstpV + src1_row_sizeUV;
  const int dst_heightUV = dst->GetHeight(PLANAR_U);

  BitBlt(dstp, dst_pitch, src1p, src1_pitch, src1_row_size, vi.height);
  BitBlt(dstp2, dst_pitch, src2p, src2_pitch, src2_row_size, vi.height);

  BitBlt(dstpU, dst_pitchUV, src1pU, src1_pitchUV, src1_row_sizeUV, dst_heightUV);
  BitBlt(dstp2U, dst_pitchUV, src2pU, src2_pitchUV, src2_row_sizeUV, dst_heightUV);

  BitBlt(dstpV, dst_pitchUV, src1pV, src1_pitchUV, src1_row_sizeUV, dst_heightUV);
  BitBlt(dstp2V, dst_pitchUV, src2pV, src2_pitchUV, src2_row_sizeUV, dst_heightUV);

  return dst;
}


AVSValue __cdecl StackHorizontal::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  PClip result = args[0].AsClip();
  for (int i=0; i<args[1].ArraySize(); ++i)
    result = new StackHorizontal(result, args[1][i].AsClip(), env);
  return result;
}







/********************************
 *******   Five Versions   ******
 ********************************/

ShowFiveVersions::ShowFiveVersions(PClip* children, IScriptEnvironment* env)
{
  for (int b=0; b<5; ++b)
    child[b] = children[b];

  vi = child[0]->GetVideoInfo();

  for (int c=1; c<5; ++c)
  {
    const VideoInfo& viprime = child[c]->GetVideoInfo();
    vi.num_frames = max(vi.num_frames, viprime.num_frames);
    if (vi.width != viprime.width || vi.height != viprime.height || vi.pixel_type != viprime.pixel_type)
      env->ThrowError("ShowFiveVersions: video attributes of all clips must match");
  }

  vi.width *= 3;
  vi.height *= 2;
}

PVideoFrame __stdcall ShowFiveVersions::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame dst = env->NewVideoFrame(vi);
  BYTE* dstp = dst->GetWritePtr();
  const int dst_pitch = dst->GetPitch();
  const int height = dst->GetHeight()/2;

  for (int c=0; c<5; ++c) 
  {
    PVideoFrame src = child[c]->GetFrame(n, env);
    const BYTE* srcp = src->GetReadPtr();
    const int src_pitch = src->GetPitch();
    const int src_row_size = src->GetRowSize();

    // staggered arrangement
    BYTE* dstp2 = dstp + (c>>1) * src_row_size;
    if ((c&1)^vi.IsRGB())
      dstp2 += (height * dst_pitch);
    if (c&1)
      dstp2 += vi.BytesFromPixels(vi.width/6);

    BitBlt(dstp2, dst_pitch, srcp, src_pitch, src_row_size, height);
  }

  return dst;
}


AVSValue __cdecl ShowFiveVersions::Create(AVSValue args, void*, IScriptEnvironment* env)
{
  PClip children[5];
  for (int i=0; i<5; ++i)
    children[i] = args[i].AsClip();
  return new ShowFiveVersions(children, env);
}









/**************************************
 *******   Animate (Recursive)   ******
 **************************************/

Animate::Animate( PClip context, int _first, int _last, const char* _name, const AVSValue* _args_before, 
                  const AVSValue* _args_after, int _num_args, IScriptEnvironment* env )
   : first(_first), last(_last), name(_name), num_args(_num_args)
{
  if (first >= last)
    env->ThrowError("Animate: final frame number must be greater than initial");

  // check that argument types match
  for (int arg=0; arg<num_args; ++arg) {
    const AVSValue& a = _args_before[arg];
    const AVSValue& b = _args_after[arg];
    if (a.IsString() && b.IsString()) {
      if (lstrcmp(a.AsString(), b.AsString()))
        env->ThrowError("Animate: string arguments must match before and after");
    }
    else if (a.IsBool() && b.IsBool()) {
      if (a.AsBool() != b.AsBool())
        env->ThrowError("Animate: boolean arguments must match before and after");
    }
    else if (a.IsFloat() && b.IsFloat()) {
      // ok; also catches other numeric types
    }
    else if (a.IsClip() && b.IsClip()) {
      // ok
    }
    else {
      env->ThrowError("Animate: must have two argument lists with matching types");
    }
  }

  // copy args, and add initial clip arg for OOP notation

  if (context)
    num_args++;

  args_before = new AVSValue[num_args*3];
  args_after = args_before + num_args;
  args_now = args_after + num_args;

  if (context) {
    args_after[0] = args_before[0] = context;
    for (int i=1; i<num_args; ++i) {
      args_before[i] = _args_before[i-1];
      args_after[i] = _args_after[i-1];
    }
  }
  else {
    for (int i=0; i<num_args; ++i) {
      args_before[i] = _args_before[i];
      args_after[i] = _args_after[i];
    }
  }

  memset(cache_stage, -1, sizeof(cache_stage));
  cache[0] = env->Invoke(name, AVSValue(args_before, num_args)).AsClip();
  cache_stage[0] = 0;
  cache[1] = env->Invoke(name, AVSValue(args_after, num_args)).AsClip();
  cache_stage[1] = last-first;
  const VideoInfo& vi1 = cache[0]->GetVideoInfo();
  const VideoInfo& vi2 = cache[1]->GetVideoInfo();
  if (vi1.width != vi2.width || vi1.height != vi2.height)
    env->ThrowError("Animate: initial and final video frame sizes must match");
}


PVideoFrame __stdcall Animate::GetFrame(int n, IScriptEnvironment* env) 
{
  int stage = min(max(n, first), last) - first;
  for (int i=0; i<cache_size; ++i)
    if (cache_stage[i] == stage)
      return cache[i]->GetFrame(n, env);

  // filter not found in cache--create it
  int furthest = 0;
  for (int j=1; j<cache_size; ++j)
    if (abs(stage-cache_stage[j]) > abs(stage-cache_stage[furthest]))
      furthest = j;

  int scale = last-first;
  for (int a=0; a<num_args; ++a) {
    if (args_before[a].IsInt() && args_after[a].IsInt()) {
      args_now[a] = int(((double)args_before[a].AsInt()*(scale-stage) + (double)args_after[a].AsInt()*stage) / scale + 0.5);
    }
    else if (args_before[a].IsFloat() && args_after[a].IsFloat()) {
      args_now[a] = float(((double)args_before[a].AsFloat()*(scale-stage) + (double)args_after[a].AsFloat()*stage) / scale);
    }
    else {
      args_now[a] = args_before[a];
    }
  }
  cache_stage[furthest] = stage;
  cache[furthest] = env->Invoke(name, AVSValue(args_now, num_args)).AsClip();
  return cache[furthest]->GetFrame(n, env);
}

  

AVSValue __cdecl Animate::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  PClip context;
  if (args[0].IsClip()) {
    context = args[0].AsClip();
    args = AVSValue(&args[1], 4);
  }
  const int first = args[0].AsInt();
  const int last = args[1].AsInt();
  const char* const name = args[2].AsString();
  int n = args[3].ArraySize();
  if (n&1)
    env->ThrowError("Animate: must have two argument lists of the same length");
  return new Animate(context, first, last, name, &args[3][0], &args[3][n>>1], n>>1, env);
}
