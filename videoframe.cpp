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
    Copy(other, left, top);
  }
}

BufferWindow::Copy(const BufferWindow& other, dimension left, dimension top)
{
  _ASSERTE(this != &other);
  BYTE * dst = GetWritePtr();  //has to be done first since it can change the offset & pitch of the self
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


//naive implementation: I convert other to the same colorspace and then copy..
void NormalVideoFrame::Copy(CPVideoFrame other, dimension left, dimension top)
{
  //bettter do these conversions/checks before colorspace conversions
  left = WidthToRowSize(left);
  top = HeightCheck(top);          
  main.Copy(( (CPNormalVideoFrame)other->ConvertTo(GetColorSpace()) )->main, left, top);
}
