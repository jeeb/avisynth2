// Avisynth v1.0 beta.  Copyright 2000 Ben Rudiak-Gould.
// http://www.math.berkeley.edu/~benrg/avisynth.html

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


#include "transform.h"





/********************************************************************
***** Declare index of new filters for Avisynth's filter engine *****
********************************************************************/

AVSFunction Transform_filters[] = {
  { "FlipVertical", "c", FlipVertical::Create },     
  { "Crop", "ciiii", Crop::Create },              // left, top, width, height *OR*
                                                  //  left, top, -right, -bottom (VDub style)
  { "CropBottom", "ci", Create_CropBottom },      // bottom amount
  { "AddBorders", "ciiii", AddBorders::Create },  // left, top, right, bottom
  { "Letterbox", "cii[x1]i[x2]i", Create_Letterbox },       // top, bottom, [left], [right]
  { 0 }
};










/********************************
 *******   Flip Vertical   ******
 ********************************/

PVideoFrame FlipVertical::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);
  const BYTE* srcp = src->GetReadPtr();
  BYTE* dstp = dst->GetWritePtr();
  int row_size = src->GetRowSize();
  int src_pitch = src->GetPitch();
  int dst_pitch = dst->GetPitch();
  BitBlt(dstp, dst_pitch, srcp + (vi.height-1) * src_pitch, -src_pitch, row_size, vi.height);
  return dst;
}


AVSValue __cdecl FlipVertical::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  return new FlipVertical(args[0].AsClip());
}











/******************************
 *******   Crop Filter   ******
 *****************************/

Crop::Crop(int _left, int _top, int _width, int _height, PClip _child, IScriptEnvironment* env)
 : GenericVideoFilter(_child)
{
  /* Negative values -> VDub-style syntax
     Namely, Crop(a, b, -c, -d) will crop c pixels from the right and d pixels from the bottom.  
     Flags on 0 values too since AFAICT it's much more useful to this syntax than the standard one. */
  if ( (_left<0) || (_top<0) )
    env->ThrowError("Crop: Top and Left must be more than 0");

  if (_width <= 0)
      _width = vi.width - _left + _width;
  if (_height <= 0)
      _height = vi.height - _top + _height;

  if (_width <=0)
    env->ThrowError("Crop: Destination width is 0 or less.");

  if (_height<=0)
    env->ThrowError("Crop: Destination height is 0 or less.");

  if (vi.IsYUV()) {
    // YUY2 can only crop to even pixel boundaries horizontally
    _left = _left & -2;
    _width = (_width+1) & -2;
    if (vi.IsYV12())  { //YV12 can only crop height to multiple of 2
      _height = _height & -2;
      _top = _top & -2;
    }
  } else {
    // RGB is upside-down
    _top = vi.height - _height - _top;
  }


  if (_left + _width > vi.width || _top + _height > vi.height)
    env->ThrowError("Crop: you cannot use crop to enlarge or 'shift' a clip");

  left_bytes = vi.BytesFromPixels(_left);
  top = _top;
  vi.width = _width;
  vi.height = _height;
  child->SetCacheHints(CACHE_NOTHING,0);
}


PVideoFrame Crop::GetFrame(int n, IScriptEnvironment* env) 
{
  PVideoFrame frame = child->GetFrame(n, env);
  if (!vi.IsPlanar())
    return frame->Subframe(top * frame->GetPitch() + left_bytes, frame->GetPitch(), vi.RowSize(), vi.height);
  else
    return frame->Subframe(top * frame->GetPitch() + left_bytes, frame->GetPitch(), vi.RowSize(), vi.height, (top/2) * frame->GetPitch(PLANAR_U) + (left_bytes/2), (top/2) * frame->GetPitch(PLANAR_V) + (left_bytes/2), frame->GetPitch(PLANAR_U));
  //                                                                                                                    U                                  V
}


AVSValue __cdecl Crop::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  return new Crop( args[1].AsInt(), args[2].AsInt(), args[3].AsInt(), args[4].AsInt(), 
                   args[0].AsClip(), env );
}





/******************************
 *******   Add Borders   ******
 *****************************/

AddBorders::AddBorders(int _left, int _top, int _right, int _bot, PClip _child)
 : GenericVideoFilter(_child), left(_left), top(_top), right(_right), bot(_bot), mybuffer(0)
{
  if (vi.IsYUV()) {
    // YUY2 can only add even amounts
    left = left & -2;
    right = (right+1) & -2;
    if (vi.IsYV12()) {
      top=top& -2;
      bot=bot+1& -2;
    }
  } else {
    // RGB is upside-down
    int t = top; top = bot; bot = t;
  }
  vi.width += left+right;
  vi.height += top+bot;
  child->SetCacheHints(CACHE_NOTHING,0);
}


PVideoFrame AddBorders::GetFrame(int n, IScriptEnvironment* env) 
{
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  const BYTE* srcp = src->GetReadPtr();
  BYTE* dstp = dst->GetWritePtr();
  const int src_pitch = src->GetPitch();
  const int dst_pitch = dst->GetPitch();
  const int src_row_size = src->GetRowSize();
  const int dst_row_size = dst->GetRowSize();
  const int src_height = src->GetHeight();

  const int initial_black = top * dst_pitch + vi.BytesFromPixels(left);
  const int middle_black = dst_pitch - src_row_size;
  const int final_black = bot * dst_pitch + vi.BytesFromPixels(right) 
                          + (dst_pitch - dst_row_size);

  const int initial_blackUV = (top/2) * dst->GetPitch(PLANAR_U) + left/2;
  const int middle_blackUV = dst->GetPitch(PLANAR_U) - src->GetRowSize(PLANAR_U);
  const int final_blackUV = (bot/2) * dst->GetPitch(PLANAR_U) + (right/2)
                          + (dst->GetPitch(PLANAR_U)- dst->GetRowSize(PLANAR_U));


  if (vi.IsYV12()) {
    BitBlt(dstp+initial_black, dst_pitch, srcp, src_pitch, src_row_size, src_height);
    BitBlt(dst->GetWritePtr(PLANAR_U)+initial_blackUV, dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U), src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
    BitBlt(dst->GetWritePtr(PLANAR_V)+initial_blackUV, dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
    short YBlack=10;
    unsigned char UVBlack=0x80;
    for (int a=0; a<initial_black; a += 2)
      *(unsigned short*)(dstp+a) = YBlack;
    dstp += initial_black + src_row_size;
    for (int y=src_height-1; y>0; --y) {
      for (int b=0; b<middle_black; b += 2)
        *(unsigned short*)(dstp+b) = YBlack;
      dstp += dst_pitch;
    }
    for (int c=0; c<final_black; c += 2)
      *(unsigned short*)(dstp+c) = YBlack;

    dstp = dst->GetWritePtr(PLANAR_U);
    for (a=0; a<initial_blackUV; a++)
      *(unsigned char*)(dstp+a) = UVBlack;
    dstp += initial_blackUV + src->GetRowSize(PLANAR_U);
    for (y=src->GetHeight(PLANAR_U)-1; y>0; --y) {
      for (int b=0; b<middle_blackUV; b++)
        *(unsigned char*)(dstp+b) = UVBlack;
      dstp += dst->GetPitch(PLANAR_U);
    }
    for (c=0; c<final_blackUV; c ++)
      *(unsigned char*)(dstp+c) = UVBlack;

    dstp = dst->GetWritePtr(PLANAR_V);
    for (a=0; a<initial_blackUV; a++)
      *(unsigned char*)(dstp+a) = UVBlack;
    dstp += initial_blackUV + src->GetRowSize(PLANAR_U);
    for (y=src->GetHeight(PLANAR_U)-1; y>0; --y) {
      for (int b=0; b<middle_blackUV; b++)
        *(unsigned char*)(dstp+b) = UVBlack;
      dstp += dst->GetPitch(PLANAR_U);
    }
    for (c=0; c<final_blackUV; c++)
      *(unsigned char*)(dstp+c) = UVBlack;



  } else if (vi.IsYUY2()) {
    BitBlt(dstp+initial_black, dst_pitch, srcp, src_pitch, src_row_size, src_height);
    const unsigned __int32 black = 0x80108010;
    for (int a=0; a<initial_black; a += 4)
      *(unsigned __int32*)(dstp+a) = black;
    dstp += initial_black + src_row_size;
    for (int y=src_height-1; y>0; --y) {
      for (int b=0; b<middle_black; b += 4)
        *(unsigned __int32*)(dstp+b) = black;
      dstp += dst_pitch;
    }
    for (int c=0; c<final_black; c += 4)
      *(unsigned __int32*)(dstp+c) = black;
  } else {
    BitBlt(dstp+initial_black, dst_pitch, srcp, src_pitch, src_row_size, src_height);
    memset(dstp, 0, initial_black);
    dstp += initial_black + src_row_size;
    for (int y=src_height-1; y>0; --y) {
      if (middle_black) {
        memset(dstp, 0, middle_black);
      }
      dstp += dst_pitch;
    }
    memset(dstp, 0, final_black);
  }

  return dst;
}


AVSValue __cdecl AddBorders::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  return new AddBorders( args[1].AsInt(), args[2].AsInt(), args[3].AsInt(), 
                         args[4].AsInt(), args[0].AsClip() );
}










/**********************************
 *******   Factory Methods   ******
 *********************************/

AVSValue __cdecl Create_Letterbox(AVSValue args, void*, IScriptEnvironment* env) 
{
  PClip clip = args[0].AsClip();
  int top = args[1].AsInt();
  int bot = args[2].AsInt();
  int left = args[3].AsInt(0); 
  int right = args[4].AsInt(0);
  const VideoInfo& vi = clip->GetVideoInfo();
  if ( (top<0) || (bot<0) || (left<0) || (right<0) ) 
    env->ThrowError("LetterBox: You cannot specify letterboxing less than 0.");
  if (top+bot>=vi.height) // Must be >= otherwise it is interpreted wrong by crop()
    env->ThrowError("LetterBox: You cannot specify letterboxing that is bigger than the picture (height).");  
  if (right+left>=vi.width) // Must be >= otherwise it is interpreted wrong by crop()
    env->ThrowError("LetterBox: You cannot specify letterboxing that is bigger than the picture (width).");
  if (vi.IsYUY2() && (left&1))
    env->ThrowError("LetterBox: Width must be divideable with 2 (Left side)");
  if (vi.IsYUY2() && (right&1))
    env->ThrowError("LetterBox: Width must be divideable with 2 (Right side)");
  return new AddBorders(left, top, right, bot, new Crop(left, top, vi.width-left-right, vi.height-top-bot, clip, env));
}


AVSValue __cdecl Create_CropBottom(AVSValue args, void*, IScriptEnvironment* env) 
{
  PClip clip = args[0].AsClip();
  const VideoInfo& vi = clip->GetVideoInfo();
  return new Crop(0, 0, vi.width, vi.height - args[1].AsInt(), clip, env);
}
