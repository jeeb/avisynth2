// Avisynth v2.5 alpha.  Copyright 2002 Ben Rudiak-Gould et al.
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


#include "videoframe.h"
#include <functional>   //for unary_function 
#include <algorithm>    //for remove_if()  
#pragma warning(disable:4786)



/**********************************************************************************************/
/*************************************** VideoFrame *******************************************/
/**********************************************************************************************/



virtual dimension VideoFrame::HeightCheck(dimension height) const
{
  static AvisynthError FB_AND_ODD_HT("Attempting to create a field based VideoFrame with an odd height");
  if (IsFiledBased() && (height & 1))
    throw FB_AND_ODD_HT;
  return height;
}


const string VideoFrame::UNMATCHING_CLR_SPACE("ColorSpace don't match");

const string FB_DONT_MATCH = "Cannot stack an field based frame with a plain one";

void VideoFrame::StackHorizontal(CPVideoFrame other)
{  
  static const string STACK_HZ_ERR = "StackHorizontal Error: ";

  if (GetColorSpace() != other->GetColorSpace())
    throw AvisynthError(STACK_HZ_ERR + UNMATCHING_CLR_SPACE);  
  if (GetVideoHeight() != other->GetVideoHeight())
    throw AvisynthError(STACK_HZ_ERR + "Height don't match");
  if (IsFieldBased() != other-> IsFieldBased())
    throw AvisynthError(STACK_HZ_ERR + FB_DONT_MATCH);
  //now everything is ok
  
  dimension original_width = GetVideoWidth(); 
  SizeChange(0, -other->GetVideoWidth(), 0, 0);
  Copy(other, original_width, 0);
}

void VideoFrame::StackVertical(CPVideoFrame other)
{
  static const string STACK_VT_ERR = "StackVertical Error: ";

  if (GetColorSpace() != other->GetColorSpace())
    throw AvisynthError(STACK_VT_ERR + UNMATCHING_CLR_SPACE);  
  if (GetVideoWidth() != other->GetVideoWidth())
    throw AvisynthError(STACK_VT_ERR + "Width don't match");
  if (IsFieldBased() != other-> IsFieldBased())
    throw AvisynthError(STACK_VT_ERR + FB_DONT_MATCH);
  //now everything is ok

  dimension original_height = GetVideoHeight();
  SizeChange(0, 0, 0, -other->GetVideoHeight());
  Copy(other, 0, original_height);
}


/**********************************************************************************************/
/************************************ VideoFrameBuffer ****************************************/
/**********************************************************************************************/


VideoFrameBuffer::VideoFrameBuffer(dimension row_size, dimension height, Align _align)
: align( (_align < 0)? -_align : max(_align, FRAME_ALIGN) ),
  pitch(Aligned(row_size)), size( height * pitch + align*4 ) 
{
  AllocationVector::iterator it = alloc.begin();
  while( it != alloc.end() && it->first != size ) { ++it; }
  if (it == alloc.end() )
    data = new BYTE[size];
  else {
    data = it->second;
    alloc.erase(it);
  }
}




/**********************************************************************************************/
/************************************** BufferWindow ******************************************/
/**********************************************************************************************/


BYTE* BufferWindow::GetWritePtr() { 
  if (vfb->isShared()) {
    VideoFrameBuffer * new_vfb = new VideoFrameBuffer(row_size, height, -vfb->GetAlign());
    int new_offset = new_vfb->FirstOffset();
    IScriptEnvironment::BitBlt(new_vfb->GetDataPtr() + new_offset, new_vfb->GetPitch(), vfb->GetDataPtr() + offset, vfb->GetPitch(), row_size, height);
    vfb = new_vfb;
    offset = new_offset;
  }
  return vfb->GetDataPtr() + offset;
} 


BufferWindow::BufferWindow(const BufferWindow& other, dimension left, dimension right, dimension top, dimension bottom)
  : row_size(RowSizeCheck(other.row_size - left - right)), height(HeightCheck(other.height - top - bottom)),
    offset(other.offset + left + top*other.GetPitch()), vfb(other.vfb)
{
  //three conditions where needed to reallocate the Buffer
  if (row_size > GetPitch() || offset < 0 || offset + height * GetPitch() >= vfb->GetSize())
  {
    vfb = new VideoFrameBuffer(row_size, height, -vfb->GetAlign());
    offset = vfb->FirstOffset();
    Copy(other, -left, -top);
  }
}

void BufferWindow::Copy(const BufferWindow& other, dimension left, dimension top)
{
  _ASSERTE(this != &other);
  BYTE * dst = GetWritePtr();  //has to be done first since it can change the pitch of self
  const BYTE * src = other.GetReadPtr();
  int copy_offset = 0, copy_offset_other = 0;
  dimension pitch = GetPitch();
  dimension pitch_other = other.GetPitch();
  dimension copy_row_size, copy_height;
  if (left < 0) {
    copy_row_size = min(row_size, other.row_size + left);
    copy_offset_other -= left;
  } else {
    copy_row_size = min(row_size - left, other.row_size);
    copy_offset += left;
  }
  if (top < 0) {
    copy_height = min(height, other.height + top);
    copy_offset_other -= top*pitch_other;
  } else {
    copy_height = min(height - top, other.height);
    copy_offset += top*pitch;
  }
  if (copy_height > 0 && copy_row_size > 0) //these checks should be moved in BitBlt
    IScriptEnvironment::BitBlt(dst + copy_offset, pitch, src + copy_offset_other, pitch_other, copy_row_size, copy_height);                                          
}

void BufferWindow::Blend(const BufferWindow& other, float factor)
{
  static const AvisynthError UNMATCHING_SIZE("Blend Error: Height or Width don't match");
  if (row_size != other.row_size || height != other.height)
    throw UNMATCHING_SIZE;
  if (factor <= 0) 
    return;         //no change
  if (factor >= 1)
    *this = other;  //we replace this window by the other one
  else {
    //TODO: Code ME !!
  }
}


/**********************************************************************************************/
/************************************** PropertyList ******************************************/
/**********************************************************************************************/


PropertyList::CPProperty PropertyList::GetProperty(const PropertyName& name) const
{
  static CPProperty nullProp;
  PropertyVector::const_iterator it = props.begin();
  while (it != props.end() && ! (*it)->IsType(name)) { ++it; }
  return it != props.end()? *it : nullProp;
}

PropertyList * PropertyList::SetProperty(CPProperty prop)
{
  PropertyVector::iterator it = props.begin();
  while( it != props.end() && ! (*it)->SameTypeAs(*prop) ) { ++it; }
  if (it != props.end())
    *it = prop;
  else
    props.push_back(prop);
  return this;
}

PropertyList * PropertyList::RemoveProperty(const PropertyName& name)
{
  PropertyVector::iterator it = props.begin();
  while (it != props.end() && ! (*it)->IsType(name)) { ++it; }
  if (it != props.end())
    props.erase(it);
  return this;
}


struct IsPropVolatile : public unary_function<PropertyList::CPProperty, bool> {
  bool operator() (const VideoFrame::CPProperty& prop) const { return prop->IsVolatile(); }
};

PropertyList * PropertyList::RemoveVolatileProperties()
{  
  props.erase(remove_if(props.begin(), props.end(), IsPropVolatile()), props.end());
  return this;
}


/**********************************************************************************************/
/************************************ NormalVideoFrame ****************************************/
/**********************************************************************************************/


void NormalVideoFrame::Copy(CPVideoFrame other, dimension left, dimension top)
{
  if (GetColorSpace() != other->GetColorSpace())
    throw AvisynthError("Copy Error: " + UNMATCHING_CLR_SPACE);
  left = WidthToRowSize(left);
  top = HeightCheck(top);          
  main.Copy( ((CPNormalVideoFrame)other)->main, left, top);
}

/**********************************************************************************************/
/************************************* RGBVideoFrame ******************************************/
/**********************************************************************************************/


void RGBVideoFrame::FlipHorizontal()
{  
  dimension row_size = main.GetRowSize();
  dimension height   = main.GetHeight();
  BufferWindow dst(row_size, height, -main.GetAlign());
  
  BYTE * dstp = dst.GetWritePtr();  
  const BYTE * srcp = main.GetReadPtr();
  dimension src_pitch = main.GetPitch();
  dimension dst_pitch = dst.GetPitch();

  int step = WidthToRowSize(1);
  dimension width = row_size/step;
  srcp += row_size - step;
  for(int h = height; h--> 0; )
  {
    for(int x = width; x--> 0; ) 
      for(int i = step; i-->0; )
        dstp[step*x+i] = srcp[-step*x+i];
    srcp += src_pitch;
    dstp += dst_pitch;
  }
  main = dst;  //replace window by the flipped one  
}


/**********************************************************************************************/
/************************************ RGB32VideoFrame *****************************************/
/**********************************************************************************************/

CPVideoFrame RGB32VideoFrame::ConvertTo(ColorSpace space) const
{
  switch(space)
  {
    case VideoInfo::CS_BGR32: return this;
    case VideoInfo::CS_BGR24: return new RGB24VideoFrame(*this);
    case VideoInfo::CS_YUY2:  return new YUY2VideoFrame(*this);
    case VideoInfo::CS_YV12:  return new PlanarVideoFrame(*this);
  }
  throw AvisynthError(CONVERTTO_ERR);
}



/**********************************************************************************************/
/************************************ RGB24VideoFrame *****************************************/
/**********************************************************************************************/

CPVideoFrame RGB24VideoFrame::ConvertTo(ColorSpace space) const
{
  switch(space)
  {
    case VideoInfo::CS_BGR32: return new RGB32VideoFrame(*this);
    case VideoInfo::CS_BGR24: return this;
    case VideoInfo::CS_YUY2:  return new YUY2VideoFrame(*this);
    case VideoInfo::CS_YV12:  return new PlanarVideoFrame(*this);
  }
  throw AvisynthError(CONVERTTO_ERR);
}


/**********************************************************************************************/
/************************************* YUY2VideoFrame *****************************************/
/**********************************************************************************************/

YUY2VideoFrame::YUY2VideoFrame(const RGB24VideoFrame& other)
 : 



CPVideoFrame YUY2VideoFrame::ConvertTo(ColorSpace space) const
{
  switch(space)
  {
    case VideoInfo::CS_BGR32: return new RGB32VideoFrame(*this);
    case VideoInfo::CS_BGR24: return new RGB24VideoFrame(*this);
    case VideoInfo::CS_YUY2:  return this;
    case VideoInfo::CS_YV12:  return new PlanarVideoFrame(*this);
  }
  throw AvisynthError(CONVERTTO_ERR);
}

void YUY2VideoFrame::FlipHorizontal()
{  
  dimension row_size = main.GetRowSize();
  dimension height   = main.GetHeight();
  BufferWindow dst(row_size, height, -main.GetAlign());
  
  BYTE * dstp = dst.GetWritePtr();  
  const BYTE * srcp = main.GetReadPtr();
  dimension src_pitch = main.GetPitch();
  dimension dst_pitch = dst.GetPitch();

  srcp += row_size - 4;
  for(int h = height; h--> 0; )
  {
    for(int x = row_size>>2; x--> 0; ) {
      dstp[4*x]   = srcp[-4*x+2];
      dstp[4*x+1] = srcp[-4*x+1];
      dstp[4*x+2] = srcp[-4*x];
      dstp[4*x+3] = srcp[-4*x+3];
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
  main = dst;  //replace window by the flipped one  
}

/**********************************************************************************************/
/************************************ PlanarVideoFrame ****************************************/
/**********************************************************************************************/


PlanarVideoFrame::PlanarVideoFrame(const RGB24VideoFrame& other)
: VideoFrameAncestor(other.IsFieldBased()), y(WidthToRowSize(other.GetVideoWidth()), HeightCheck(otherGetVideoHeight()), other.GetAlign()),
  u(other.GetVideoWidth()>>1, other.GetVideoHeight()>>1, other.GetAlign()), v(other.GetVideoWidth()>>1, other.GetVideoHeight()>>1, other.GetAlign())
{
  //TODO: Code ME !!
}
 
PlanarVideoFrame::PlanarVideoFrame(const RGB32VideoFrame& other)
: VideoFrameAncestor(other.IsFieldBased()), y(WidthToRowSize(other.GetVideoWidth()), HeightCheck(otherGetVideoHeight()), other.GetAlign()),
  u(other.GetVideoWidth()>>1, other.GetVideoHeight()>>1, other.GetAlign()), v(other.GetVideoWidth()>>1, other.GetVideoHeight()>>1, other.GetAlign())
{
  //TODO: Code ME !!  
}
 
PlanarVideoFrame::PlanarVideoFrame(const YUY2VideoFrame& other)
: VideoFrameAncestor(other.IsFieldBased()), y(WidthToRowSize(other.GetVideoWidth()), HeightCheck(otherGetVideoHeight()), other.GetAlign()),
  u(other.GetVideoWidth()>>1, other.GetVideoHeight()>>1, other.GetAlign()), v(other.GetVideoWidth()>>1, other.GetVideoHeight()>>1, other.GetAlign())
{
  //TODO: Code ME !!  
}


CPVideoFrame PlanarVideoFrame::ConvertTo(ColorSpace space) const
{
  switch(space)
  {
    case VideoInfo::CS_BGR32: return new RGB32VideoFrame(*this);
    case VideoInfo::CS_BGR24: return new RGB24VideoFrame(*this);
    case VideoInfo::CS_YUY2:  return new YUY2VideoFrame(*this);
    case VideoInfo::CS_YV12:  return this;
  }
  throw AvisynthError(CONVERTTO_ERR);
}

void PlanarVideoFrame::SizeChange(dimension left, dimension right, dimension top, dimension bottom)
{
  left = WidthToRowSize(left);   //those four perform the args check
  right = WidthToRowSize(right);
  HeightCheck(top);
  HeightCheck(bottom);
  y = BufferWindow(y, left, right, top, bottom);   //width and height > 0 checked here
  u = BufferWindow(u, left>>1, right>>1, top>>1, bottom>>1);
  v = BufferWindow(v, left>>1, right>>1, top>>1, bottom>>1);
  //that's all
}


void PlanarVideoFrame::FlipVertical()
{
  for(int i = 3; i--> 0; ) //reverse loop  
    GetWindow(IndexToYUVPlane(i)).FlipVertical();
}

void PlanarVideoFrame::FlipHorizontal()
{
  for(int i = 3; i--> 0; ) //reverse loop
  {
    BufferWindow& src = GetWindow(IndexToYUVPlane(i));
    dimension row_size = src.GetRowSize();
    dimension height   = src.GetHeight();
    BufferWindow dst(row_size, height, -src.GetAlign());
    BYTE* dstp = dst.GetWritePtr();  
    const BYTE* srcp = src.GetReadPtr();
    dimension src_pitch = src.GetPitch();
    dimension dst_pitch = dst.GetPitch();

    srcp +=row_size-1;
    for(int h = height; h--> 0; )
    {
      for(int x = row_size; x--> 0; )
        dstp[x] = srcp[-x];
      srcp += src_pitch;
      dstp += dst_pitch;
    }
    src = dst;  //replace window by the flipped one
  }
}

void PlanarVideoFrame::Blend(CPVideoFrame other, float factor)
{
  if (other->GetColorSpace() != VideoInfo::CS_YV12)
    throw AvisynthError("Blend Error: " + UNMATCHING_CLR_SPACE); 
  CPPlanarVideoFrame planar_other = other;

  for(int i = 3; i--> 0; ) { //reverse loop
    Plane p = IndexToYUVPlane(i);
    GetWindow(p).Blend(planar_other->GetWindow(p), factor);  //width and height check done here
  }
}