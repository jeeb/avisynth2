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
#include <string> 
using namespace std;


//simpler replacement for auto_ptr (saves a bool)
//and with a operator=(T*)  (replacement for reset(T*) missing in VC6 STL)
template <class T> class simple_auto_ptr {

  mutable T * ptr;

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
  T * release() const { T * result = ptr; ptr = NULL; return result; }
  void swap(simple_auto_ptr<T>&) { std::swap<T*>(ptr, other.ptr); }

}; 


// VideoFrameBuffer holds information about a memory block which is used
// for video data.  

class VideoFrameBuffer : public RefCounted {  

  typedef VideoFrame::dimension dimension;
  typedef VideoFrame::Align Align;
  typedef simple_auto_ptr<BYTE> Buffer;  //auto_ptr<BYTE> Buffer;
  typedef pair<int, Buffer> MemoryPiece;
  typedef vector<MemoryPiece> AllocationVector;

  static AllocationVector alloc;

  Buffer data;
  int size;  
  dimension pitch;
  Align align;

public:

  VideoFrameBuffer(dimension row_size, dimension height, Align _align);
  ~VideoFrameBuffer() { alloc.push_back(MemoryPiece(size, data)); }

  const BYTE * GetDataPtr() const { return data.get(); }
  BYTE * GetDataPtr() { return data.get(); }
  dimension GetPitch() const { return pitch; }
  int GetSize() const { return size; }
  Align GetAlign() const { return align; }
  
  int FirstOffset() const { return int(data.get()) % align; }

  dimension Aligned(dimension dim) { return (dim + align - 1) / align * align; }
};


//BufferWindow holds a window in a possibly shared VideoFrameBuffer
//when asked for a write ptr, it will do a (intelligent) copy of the buffer if shared
class BufferWindow {

  typedef smart_ptr<VideoFrameBuffer> PVideoFrameBuffer;  
  typedef VideoFrame::dimension dimension;
  typedef VideoFrame::Align Align;  

  PVideoFrameBuffer vfb;
  int offset;
  dimension row_size, height;
  
  inline static dimension RowSizeCheck(dimension row_size) {
    static AvisynthError NEG_WIDTH_ERR("Filter Error: Attempting to request a VideoFrame with a negative or zero width");
    if (row_size <= 0)
      throw NEG_WIDTH_ERR;
    return row_size;
  }
  inline static dimension HeightCheck(dimension height) {
    static AvisynthError NEG_HEIGHT_ERR("Filter Error: Attempting to request a VideoFrame with a negative or zero height");
    if (height <= 0)
      throw NEG_HEIGHT_ERR;
    return height;
  }

public:
  BufferWindow(dimension _row_size, dimension _height, Align align)
    : row_size(RowSizeCheck(_row_size)), height(HeightCheck(_height)), vfb(new VideoFrameBuffer(row_size, height, align)), offset(vfb->FirstOffset()) { }
  //copy constructor
  BufferWindow(const BufferWindow& other) : row_size(other.row_size), height(other.height), vfb(other.vfb), offset(other.offset) { }
  //redimensioning constructor, will try reusing buffer if the new window can fit in 
  BufferWindow(const BufferWindow& other, dimension left, dimension right, dimension top, dimension bottom);

  dimension GetPitch()   const { return vfb->GetPitch(); }
  dimension GetRowSize() const { return row_size; }
  dimension GetAlignedRowSize() const { dimension r = vfb->Aligned(row_size); return r <= vfb->GetPitch()? r : row_size; }
  dimension GetHeight()  const { return height; }
  Align GetAlign() const { return vfb->GetAlign(); }
  
  const BYTE* GetReadPtr() const { return vfb->GetDataPtr() + offset; }
  BYTE* GetWritePtr();  

  //copy other into self at the given coords (which can be negatives)
  //only overlap is copied, no effect if there is none
  void Copy(const BufferWindow& other, dimension left, dimension top); 
  
  void Blend(const BufferWindow& other, float factor);

  void FlipVertical() {
    BufferWindow result(row_size, height, -vfb->GetAlign());
    IScriptEnvironment::BitBlt(result.GetWritePtr(), result.GetPitch(), 
      GetReadPtr() + (height - 1)*GetPitch(), -GetPitch(), row_size, height);
    *this = result;
  }

};







class PropertyList;

typedef smart_ptr_to_cst<PropertyList> CPPropertyList;
typedef smart_ptr<PropertyList> PPropertyList;


class PropertyList : public RefCounted {

  typedef VideoFrame::CPProperty CPProperty;
  typedef VideoFrame::PropertyName PropertyName;

  typedef vector<CPProperty> PropertyVector;
  
  PropertyVector props;

public:
  CPProperty GetProperty(const PropertyName& name) const;
  PropertyList * SetProperty(CPProperty prop);
  PropertyList * RemoveProperty(const PropertyName& name);
 
  PropertyList * RemoveVolatileProperties();  

};


class VideoFrameAncestor : public VideoFrame {

  CPPropertyList propList;

  static CPPropertyList emptyList;  //empty list of properties
  static CPPropertyList fieldBasedList;  //list containing the single property fieldBased


protected:
  VideoFrameAncestor(bool fieldBased) : propList(fieldBased? fieldBasedList : emptyList) { }
  VideoFrameAncestor(const VideoFrameAncestor& other) : propList(other.propList) { }

  virtual void clean() { propList = PPropertyList(propList)->RemoveVolatileProperties(); }
  //clone if shared and perform clean on the property list

  

public:
  virtual CPProperty GetProperty(const PropertyName& name) const;
  virtual void SetProperty(CPProperty prop) { propList = PPropertyList(propList)->SetProperty(prop); }    
  virtual void RemoveProperty(const PropertyName& name) { propList = PPropertyList(propList)->RemoveProperty(name); }

  virtual PVideoFrame NewVideoFrame(const VideoInfo& vi, Align align) const;

  
protected:
  //error msgs
  static const string CONVERTTO_ERR; // ="ConvertTo Error: Cannot convert to requested ColorSpace"

};



//by opposition to PlanarVideoFrame...
class NormalVideoFrame : public VideoFrameAncestor {

  typedef smart_ptr_to_cst<NormalVideoFrame> CPNormalVideoFrame;

  inline void CheckPlane(Plane plane) const { if (plane != NOT_PLANAR) throw NoSuchPlane(); }

protected:
  
  BufferWindow main;

  NormalVideoFrame(const NormalVideoFrame& other) : VideoFrameAncestor(other), main(other.main) { }
  NormalVideoFrame(dimension width, dimension height, Align align, bool fieldBased)
    : VideoFrameAncestor(fieldBased), main(WidthToRowSize(width), HeightCheck(height), align) { }

  

public:
  virtual dimension GetPitch(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetPitch(); }
  virtual dimension GetRowSize(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetRowSize(); }
  virtual dimension GetAlignedRowSize(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetAlignedRowSize(); }
  virtual dimension GetHeight(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetHeight(); }

  virtual const BYTE * GetReadPtr(Plane plane) const throw(NoSuchPlane) { CheckPlane(plane); return main.GetReadPtr(); }
  virtual BYTE * GetWritePtr(Plane plane) throw(NoSuchPlane) { CheckPlane(plane); return main.GetWritePtr(); }


  //here we are still in pixels
  virtual void SizeChange(dimension left, dimension right, dimension top, dimension bottom)
  { 
    //now in bytes
    main = BufferWindow(main, WidthToRowSize(left), WidthToRowSize(right), HeightCheck(top), HeightCheck(bottom));
  }
  
  virtual void Copy(CPVideoFrame other, dimension left, dimension top);

  virtual void FlipVertical() { main.FlipVertical(); }

  virtual void Blend(CPVideoFrame other, float factor);
};


class RGBVideoFrame : public NormalVideoFrame {

public:
  RGBVideoFrame(const RGBVideoFrame& other) : NormalVideoFrame(other) { }
  RGBVideoFrame(dimension width, dimension height, Align align, bool fieldBased)
    : NormalVideoFrame(width, height, align, fieldBased) { }


  virtual void FlipHorizontal();

  virtual void TurnLeft(); 
  virtual void TurnRight();

};

class RGB24VideoFrame : public RGBVideoFrame {

protected:
  virtual RefCounted * clone() const { return new RGB24VideoFrame(*this); }  


public:
  RGB24VideoFrame(const RGB32VideoFrame& other);  //converting constrcutors
  RGB24VideoFrame(const YUY2VideoFrame& other);
  RGB24VideoFrame(const PlanarVideoFrame& other);
  RGB24VideoFrame(const RGB24VideoFrame& other) : RGBVideoFrame(other) { } 
  RGB24VideoFrame(dimension width, dimension height, Align align, bool fieldBased)
    : RGBVideoFrame(width, height, align, fieldBased) { }

  virtual ColorSpace GetColorSpace() const { return VideoInfo::CS_BGR24; }

  virtual dimension WidthToRowSize(dimension width) const { return 3 * width; }

  virtual CPVideoFrame ConvertTo(ColorSpace space) const;

};

class RGB32VideoFrame : public RGBVideoFrame {

protected:
  virtual RefCounted * clone() const { return new RGB32VideoFrame(*this); }

public:
  RGB32VideoFrame(const RGB24VideoFrame& other);  //converting constrcutors
  RGB32VideoFrame(const YUY2VideoFrame& other);
  RGB32VideoFrame(const PlanarVideoFrame& other);
  RGB32VideoFrame(const RGB32VideoFrame& other) : RGBVideoFrame(other) { } 
  RGB32VideoFrame(dimension width, dimension height, Align align, bool fieldBased)
    : RGBVideoFrame(width, height, align, fieldBased) { }

  virtual ColorSpace GetColorSpace() const { return VideoInfo::CS_BGR32; }

  virtual dimension WidthToRowSize(dimension width) const { return 4 * width; }

  virtual CPVideoFrame ConvertTo(ColorSpace space) const;

};


class YUY2VideoFrame : public NormalVideoFrame {

  static const string ODD_WIDTH;   //error msg when odd width in YUY2                 

protected:
  virtual RefCounted * clone() const { return new YUY2VideoFrame(*this); }

  
public:
  YUY2VideoFrame(const RGB32VideoFrame& other);  //converting constructors
  YUY2VideoFrame(const RGB24VideoFrame& other);
  YUY2VideoFrame(const PlanarVideoFrame& other);
  YUY2VideoFrame(const YUY2VideoFrame& other) : NormalVideoFrame(other) { } //spec of the above
  YUY2VideoFrame(dimension width, dimension height, Align align, bool fieldBased)
    : NormalVideoFrame(width, height, align, fieldBased)  { }    

  virtual ColorSpace GetColorSpace() const { return VideoInfo::CS_YUY2; }

  virtual CPVideoFrame ConvertTo(ColorSpace space) const;

  virtual void FlipHorizontal();

  virtual void TurnLeft(); 
  virtual void TurnRight();

};


/**********************************************************************************************/
/************************************ PlanarVideoFrame ****************************************/
/**********************************************************************************************/

class PlanarVideoFrame : public VideoFrameAncestor {

  typedef smart_ptr_to_cst<PlanarVideoFrame> CPPlanarVideoFrame;

  BufferWindow y, u ,v;

  const BufferWindow& GetWindow(Plane plane) const {
    switch(plane) {
      case PLANAR_Y: return y;
      case PLANAR_U: return u;
      case PLANAR_V: return v;
    }
    throw NoSuchPlane();
  }

  BufferWindow& GetWindow(Plane plane) {
    switch(plane) {
      case PLANAR_Y: return y;
      case PLANAR_U: return u;
      case PLANAR_V: return v;
    }
    throw NoSuchPlane();
  }

protected:
  virtual RefCounted * clone() const { return new PlanarVideoFrame(*this); }

public:
  PlanarVideoFrame(const RGB32VideoFrame& other);  //converting constructors
  PlanarVideoFrame(const RGB24VideoFrame& other);
  PlanarVideoFrame(const YUY2VideoFrame& other);
  PlanarVideoFrame(const PlanarVideoFrame& other) : VideoFrameAncestor(other), y(other.y), u(other.u), v(other.v) { }
  PlanarVideoFrame(dimension width, dimension height, Align align, bool fieldBased) 
    : VideoFrameAncestor(fieldBased), y(WidthToRowSize(width), HeightCheck(height), align),
       u(width>>1, height>>1, align), v(width>>1, height>>1, align) { }
  
  virtual dimension GetPitch(Plane plane) const throw(NoSuchPlane) { return GetWindow(plane).GetPitch(); }
  virtual dimension GetRowSize(Plane plane) const throw(NoSuchPlane) { return GetWindow(plane).GetRowSize(); }
  virtual dimension GetAlignedRowSize(Plane plane) const throw(NoSuchPlane) { return GetWindow(plane).GetAlignedRowSize(); }
  virtual dimension GetHeight(Plane plane) const throw(NoSuchPlane) { return GetWindow(plane).GetHeight(); }

  virtual const BYTE * GetReadPtr(Plane plane) const throw(NoSuchPlane) { return GetWindow(plane).GetReadPtr(); }
  virtual BYTE * GetWritePtr(Plane plane) throw(NoSuchPlane) { return GetWindow(plane).GetWritePtr(); }

  virtual ColorSpace GetColorSpace() const { return VideoInfo::CS_YV12; }
  
  virtual dimension GetVideoHeight() const { return y.GetHeight(); }  
  virtual dimension GetVideoWidth() const { return y.GetWidth(); }

  virtual dimension 


  virtual CPVideoFrame ConvertTo(ColorSpace space) const;

  virtual void SizeChange(dimension left, dimension right, dimension top, dimension bottom);
  virtual void Copy(CPVideoFrame other, dimension left, dimension top);

  virtual void FlipHorizontal();
  virtual void FlipVertical();

  virtual void TurnLeft(); 
  virtual void TurnRight();

  virtual void Blend(CPVideoFrame other, float factor);

  
}; 














#endif  //ifndef __VIDEOFRAME_H__