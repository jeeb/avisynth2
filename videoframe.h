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


#ifndef __VIDEOFRAME_H__
#define __VIDEOFRAME_H__

#include "internal.h" //I have strange errors when directly including avisynth.h ...
#include <utility>    //for pair
//include <memory>      //for auto_ptr
#include <vector> 
#include <functional> //for unary_function 
#include <algorithm>  //for remove_if()  
using namespace std;


//simpler replacement for auto_ptr (saves a bool)
//and with a operator=(T*)  (replacement for reset(T*) missing in VC6 STL)
template <class T> class simple_auto_ptr {

  T * ptr;

public:
  simple_auto_ptr() : ptr(NULL) { }
  simple_auto_ptr(T * _ptr) : ptr(_ptr) { }
  simple_auto_ptr(const simple_auto_ptr<T>& other) : ptr(other.release()) { }
  ~simple_auto_ptr() { if (ptr) delete ptr; }

  simple_auto_ptr<T>& operator=(T * _ptr) { if (ptr && ptr != _ptr) delete ptr; ptr = _ptr; return *this; }
  simple_auto_ptr<T>& operator=(const simple_auto_ptr<T>& other) {
    if (this != &other) {
      if (ptr) delete ptr;
      ptr = other.release();
    }
    return *this;
  }
  operator bool() const { return ptr != NULL; }

  T * get() const { return ptr; }  
  T * release() const { T * result = ptr; ((simple_auto_ptr<T>*)this)->ptr = NULL; return result; }
  void swap(simple_auto_ptr<T>&) { std::swap<T*>(ptr, other.ptr); }

}; 


// VideoFrameBuffer holds information about a memory block which is used
// for video data.  

class VideoFrameBuffer : public RefCounted {  

  typedef VideoFrame::dimension dimension;
  typedef VideoFrame::Align Align;
  typedef simple_auto_ptr<BYTE> Buffer;  //auto_ptr<BYTE> Buffer;
  typedef pair<unsigned, Buffer> MemoryPiece;
  typedef vector<MemoryPiece> AllocationVector;

  static AllocationVector alloc;

  Buffer data;
  unsigned size;
  dimension pitch;
  Align align;

  VideoFrameBuffer(const VideoFrameBuffer& other, unsigned rel_offset, dimension row_size, dimension height);

public:
  VideoFrameBuffer(dimension row_size, dimension height, Align _align);
  ~VideoFrameBuffer() { alloc.push_back(MemoryPiece(size, data)); }

  const BYTE * GetDataPtr() const { return data.get(); }
  BYTE * GetDataPtr() { return data.get(); }
  dimension GetPitch() const { return pitch; }
  
  unsigned FirstOffset() const { return unsigned(data.get()) % align; }

  VideoFrameBuffer * SmartCopy(unsigned rel_offset, dimension row_size, dimension height) { return new VideoFrameBuffer(*this, rel_offset, row_size, height); }

  dimension Aligned(dimension dim) { return (dim + align - 1) / align * align; }
};


//BufferWindow holds a window in a possibly shared VideoFrameBuffer
//when asked for a write ptr, it will do a (intelligent) copy of the buffer if shared
class BufferWindow {

  typedef smart_ptr<VideoFrameBuffer> PVideoFrameBuffer;  
  typedef VideoFrame::dimension dimension;
  typedef VideoFrame::Align Align;  

  PVideoFrameBuffer vfb;
  unsigned offset;
  dimension row_size, height;
  
public:
  BufferWindow(dimension _row_size, dimension _height, Align align)
    : row_size(_row_size), height(_height), vfb(new VideoFrameBuffer(row_size, height, align)), offset(vfb->FirstOffset()) { }
  BufferWindow(const BufferWindow& other) : row_size(other.row_size), height(other.height), vfb(other.vfb), offset(other.offset) { }

  dimension GetPitch()   const { return vfb->GetPitch(); }
  dimension GetRowSize() const { return row_size; }
  dimension GetAlignedRowSize() const { dimension r = vfb->Aligned(row_size); return r <= vfb->GetPitch()? r : row_size; }
  dimension GetHeight()  const { return height; }
  
  const BYTE* GetReadPtr() const { return vfb->GetDataPtr() + offset; }
  BYTE* GetWritePtr() { 
    if (vfb->isShared()) {
      vfb = vfb->SmartCopy(offset, row_size, height);
      offset = vfb->FirstOffset();
    }
    return vfb->GetDataPtr() + offset;
  } 
  
};

class PropertyList;

typedef smart_ptr_to_cst<PropertyList> CPPropertyList;
typedef smart_ptr<PropertyList> PPropertyList;


class PropertyList {

  typedef VideoFrame::CPProperty CPProperty;
  typedef VideoFrame::PropertyName PropertyName;

  typedef vector<CPProperty> PropertyVector;
  
  PropertyVector props;

public:
  CPProperty GetProperty(const PropertyName& name) const;
  void SetProperty(const CPProperty& prop);
  void RemoveProperty(const PropertyName& name);

protected:
  virtual void clean();

};





class VideoFrameAncestor : public VideoFrame {

  PPropertyList propList;

  static CPPropertyList emptyList;

protected:
  VideoFrameAncestor() : propList(emptyList) { }
  VideoFrameAncestor(const VideoFrameAncestor& other) : propList(other.propList) { }

  virtual void clean() { propList = CPPropertyList(propList); }
  //clone if shared and perform clean 
    
public:
  virtual const Property * GetProperty(const PropertyName& name) const ;
  virtual void SetProperty(const CPProperty& prop) { propList->SetProperty(prop); }    
  virtual void RemoveProperty(const PropertyName& name) { propList->RemoveProperty(name); }

};



//by opposition to PlanarVideoFrame...
class NormalVideoFrame : public VideoFrameAncestor {

  BufferWindow main;

  void CheckPlane(Plane plane) const { if (plane != NOT_PLANAR) throw NoSuchPlane(); }

protected:  

  NormalVideoFrame(const NormalVideoFrame& other) : VideoFrameAncestor(other), main(other.main) { }
  NormalVideoFrame(dimension row_size, dimension height, Align align) : main(row_size, height, align) { }

public:
  virtual dimension GetPitch(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetPitch(); }
  virtual dimension GetRowSize(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetRowSize(); }
  virtual dimension GetAlignedRowSize(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetAlignedRowSize(); }
  virtual dimension GetHeight(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetHeight(); }

  virtual const BYTE * GetReadPtr(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetReadPtr(); }
  virtual BYTE * GetWritePtr(Plane plane) throw(NoSuchPlane) { CheckPlane(plane); return main.GetWritePtr(); }

public:
  virtual void DownSize(dimension left, dimension right, dimension top, dimension bottom);


};

class RGB24VideoFrame : public NormalVideoFrame {

public:
  RGB24VideoFrame(const RGB24VideoFrame& other) : NormalVideoFrame(other) { } 
  RGB24VideoFrame(dimension width, dimension height, Align align) : NormalVideoFrame(3*width, height, align) { }

  virtual VideoInfo::ColorSpace GetColorSpace() const { return VideoInfo::CS_BGR24; }


protected:
  virtual RefCounted * clone() const { return new RGB24VideoFrame(*this); }

};

class RGB32VideoFrame : public NormalVideoFrame {

public:
  RGB32VideoFrame(const RGB32VideoFrame& other) : NormalVideoFrame(other) { } 
  RGB32VideoFrame(dimension width, dimension height, Align align) : NormalVideoFrame(4*width, height, align) { }

  virtual VideoInfo::ColorSpace GetColorSpace() const { return VideoInfo::CS_BGR32; }

  virtual RefCounted * clone() const { return new RGB32VideoFrame(*this); }
};

class YUY2VideoFrame : public NormalVideoFrame {

public:
  YUY2VideoFrame(const YUY2VideoFrame& other) : NormalVideoFrame(other) { } 
  YUY2VideoFrame(dimension width, dimension height, Align align) : NormalVideoFrame(2*width, height, align) { }

  virtual VideoInfo::ColorSpace GetColorSpace() const { return VideoInfo::CS_YUY2; }


protected:
  virtual RefCounted * clone() const { return new YUY2VideoFrame(*this); }

};



class PlanarVideoFrame : public VideoFrameAncestor {

  BufferWindow y, u ,v;

  const BufferWindow& GetPlane(Plane plane) const {
    switch(plane) {
      case PLANAR_Y: return y;
      case PLANAR_U: return u;
      case PLANAR_V: return v;
    }
    throw NoSuchPlane();
  }

  BufferWindow& GetPlane(Plane plane) {
    switch(plane) {
      case PLANAR_Y: return y;
      case PLANAR_U: return u;
      case PLANAR_V: return v;
    }
    throw NoSuchPlane();
  }

public:
  PlanarVideoFrame(const PlanarVideoFrame& other) : VideoFrameAncestor(other), y(other.y), u(other.u), v(other.v) { }
  PlanarVideoFrame(dimension width, dimension height, Align align) 
    : y(width, height, align), u(width>>1, height>>1, align), v(width>>1, height>>1, align) { }

  virtual dimension GetPitch(Plane plane) const throw(NoSuchPlane) { return GetPlane(plane).GetPitch(); }
  virtual dimension GetRowSize(Plane plane) const throw(NoSuchPlane) { return GetPlane(plane).GetRowSize(); }
  virtual dimension GetAlignedRowSize(Plane plane) const throw(NoSuchPlane) { return GetPlane(plane).GetAlignedRowSize(); }
  virtual dimension GetHeight(Plane plane) const throw(NoSuchPlane) { return GetPlane(plane).GetHeight(); }

  virtual const BYTE * GetReadPtr(Plane plane) const throw(NoSuchPlane) { return GetPlane(plane).GetReadPtr(); }
  virtual BYTE * GetWritePtr(Plane plane) throw(NoSuchPlane) { return GetPlane(plane).GetWritePtr(); }

  virtual VideoInfo::ColorSpace GetColorSpace() const { return VideoInfo::CS_YV12; }

  virtual void DownSize(dimension left, dimension right, dimension top, dimension bottom);


protected:
  virtual RefCounted * clone() const { return new PlanarVideoFrame(*this); }

}; 














#endif  //ifndef __VIDEOFRAME_H__