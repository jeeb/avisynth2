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
#pragma warning(disable:4786)

VideoFrameBuffer::VideoFrameBuffer(dimension row_size, dimension height, Align _align)
: align( (_align < 0)? -_align : max(_align, FRAME_ALIGN) ),
  pitch(Aligned(row_size)), size( height * pitch + align ) 
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


VideoFrameBuffer::VideoFrameBuffer(const VideoFrameBuffer& other, unsigned rel_offset, dimension row_size, dimension height)
: align(other.align), pitch(Aligned(row_size)), size( height * pitch + align * 4 )
{
  _ASSERTE( row_size <= other.pitch && rel_offset + other.pitch * (height - 1) + row_size < other.size);
  IScriptEnvironment::BitBlt(data.get() + FirstOffset(), pitch, other.data.get() + rel_offset, other.pitch, row_size, height);
}


PropertyList::CPProperty PropertyList::GetProperty(const PropertyName& name) const
{
  static CPProperty nullProp;
  PropertyVector::const_iterator it = props.begin();
  while (it != props.end() && ! (*it)->IsType(name)) { ++it; }
  return it != props.end()? *it : nullProp;
}

void PropertyList::SetProperty(const CPProperty& prop)
{
  PropertyVector::iterator it = props.begin();
  while( it != props.end() && ! (*it)->SameTypeAs(*prop) ) { ++it; }
  if (it != props.end())
    *it = prop;
  else
    props.push_back(prop);
}

void PropertyList::RemoveProperty(const PropertyName& name)
{
  PropertyVector::iterator it = props.begin();
  while (it != props.end() && ! (*it)->IsType(name)) { ++it; }
  if (it != props.end())
    props.erase(it);
}


struct IsPropVolatile : public unary_function<PropertyList::CPProperty, bool> {
  bool operator() (const VideoFrame::CPProperty& prop) const { return prop->IsVolatile(); }
};

void PropertyList::clean()
{
  props.erase(remove_if(props.begin(), props.end(), IsPropVolatile()), props.end());
}