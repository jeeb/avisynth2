// Avisynth v3.0 alpha.  Copyright 2002 Ben Rudiak-Gould et al.
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




#ifndef __AVISYNTH_H__
#define __AVISYNTH_H__

enum { AVISYNTH_INTERFACE_VERSION = 3 };


/* Define all types necessary for interfacing with avisynth.dll
   Moved from internal.h */

// Win32 API macros, notably the types BYTE, DWORD, ULONG, etc. 
#include <windef.h>  

// COM interface macros
#include <objbase.h>

// Raster types used by VirtualDub & Avisynth
#define in64 (__int64)(unsigned short)
typedef unsigned long	Pixel;    // this will break on 64-bit machines!
typedef unsigned long	Pixel32;
typedef unsigned char Pixel8;
typedef long			PixCoord;
typedef	long			PixDim;
typedef	long			PixOffset;

//STL relatives include
#include <string>
using namespace std;

/* Compiler-specific crap */

// Tell MSVC to stop precompiling here
#ifdef _MSC_VER
  #pragma hdrstop
#endif

// Set up debugging macros for MS compilers; for others, step down to the
// standard <assert.h> interface
#ifdef _MSC_VER
  #include <crtdbg.h>
#else
  #define _RPT0(a,b) ((void)0)
  #define _RPT1(a,b,c) ((void)0)
  #define _RPT2(a,b,c,d) ((void)0)
  #define _RPT3(a,b,c,d,e) ((void)0)
  #define _RPT4(a,b,c,d,e,f) ((void)0)
  
  #define _ASSERTE(x) assert(x)
  #include <assert.h>
#endif



// I had problems with Premiere wanting 1-byte alignment for its structures,
// so I now set the Avisynth struct alignment explicitly here.
#pragma pack(push,8)

#define FRAME_ALIGN 16 
// Default frame alignment is 16 bytes, to help P4, when using SSE2




//exception class
class AvisynthError  {
public:
  const string err_msg;
  AvisynthError(const string& _err_msg) : err_msg(_err_msg) { }
};

//exception class for internal errors
class InternalError : public AvisynthError {
public: 
  InternalError(const string& _err_msg) : AvisynthError("Internal Error: " + _err_msg) { }
};

//exception class for errors in parsed scripts
class ScriptError : public AvisynthError {
public: 
  ScriptError(const string& _err_msg) : AvisynthError("Script Error: " + _err_msg) { }
};

// The VideoInfo struct holds global information about a clip (i.e.
// information that does not depend on the frame number).  The GetVideoInfo
// method in IClip returns this struct.

// Audio Sample information
typedef float SFLOAT;

class VideoInfo {

public:

  typedef short dimension;
  
  //some enumerations
  enum SampleType {
    SAMPLE_INT8,
    SAMPLE_INT16, 
    SAMPLE_INT24,    // Int24 is a very stupid thing to code, but it's supported by some hardware.
    SAMPLE_INT32,
    SAMPLE_FLOAT
  };

  // Colorspace properties.
  enum CS_Property {
    CS_BGR = 1<<28,  
    CS_YUV = 1<<29,
    CS_INTERLEAVED = 1<<30,
    CS_PLANAR = 1<<31
  };

  // Specific colorformats
  enum ColorSpace { 
    CS_UNKNOWN = 0,
    CS_BGR24 = 1<<0 | CS_BGR | CS_INTERLEAVED,
    CS_BGR32 = 1<<1 | CS_BGR | CS_INTERLEAVED,
    CS_YUY2 = 1<<2 | CS_YUV | CS_INTERLEAVED,
    CS_YV12 = 1<<3 | CS_YUV | CS_PLANAR,  // y-v-u, planar
    CS_I420 = 1<<4 | CS_YUV | CS_PLANAR,  // y-u-v, planar
    CS_IYUV = 1<<4 | CS_YUV | CS_PLANAR  // same as above
  };


  //methods used to check that clip dimensions respect ColorSpace contraints  
  //for convenience reasons, they return their argument  
  static inline dimension WidthCheckYUY2(dimension width) {
    static const string YUY2_ODD_WD = "Filter Error: Attempted to request a YUY2 VideoFrame with an odd width";
    if ( width & 1)
      throw AvisynthError(YUY2_ODD_WD);
    return width;
  }
  static inline dimension WidthCheckYV12(dimension width) {
    static const string YV12_ODD_WD = "Filter Error: Attempted to request a YV12 VideoFrame with an odd width";
    if ( width & 1)
      throw AvisynthError(YV12_ODD_WD);
    return width;
  }
  static inline dimension WidthCheck(dimension width, ColorSpace space) {
    switch(space)
    {
      case CS_YUY2:
        return WidthCheckYUY2(width);
      case CS_YV12:
      case CS_I420:
        return WidthCheckYV12(width);
    }
    return width;
  }

  static inline dimension HeightFBCheck(dimension height, bool fieldBased) {
    static const string FB_AND_ODD_HT = "Filter Error: Attempted to request a field based VideoFrame with an odd height";
    if ( height & 1 )
      throw AvisynthError(FB_AND_ODD_HT);
    return height;
  }
  static inline dimension HeightFBCheckYV12(dimension height, bool fieldBased) {
    static const string YV12FB_AND_HT_MOD4 = "Filter Error: Attempted to request a field based YV12 VideoFrame with a not mod 4 height";
    if ( height & 3)
      throw AvisynthError(YV12FB_AND_HT_MOD4);
    return height;
  }
  static inline dimension HeightCheck(dimension height, bool fieldBased, ColorSpace space) {
    return (space == CS_YV12 || space == CS_I420) ? HeightFBCheckYV12(height, fieldBased) 
                                                  : HeightFBCheck(height, fieldBased);
  }

  //default constructor : no video, no audio
  VideoInfo() : width(0), audio_samples_per_second(0) { }



  /*
   * Video stuff
   */
private:

  ColorSpace pixel_type;  
  dimension width, height;    // width=0 means no video
  unsigned fps_numerator, fps_denominator;
  int num_frames;

public:

  // useful functions of the above

  bool HasVideo() const { return width != 0; }
  void RemoveVideo() { width = 0; }

  dimension GetWidth() const { return width; }
  dimension GetHeight() const { return height; }
  ColorSpace GetColorSpace() const { return pixel_type; }
  int GetFrameCount() const { return num_frames; }

  void SetVideo(dimension _width, dimension _height, ColorSpace space, int frame_count) {
    if (_width <= 0 || _height <= 0 || frame_count <= 0)
      throw AvisynthError("Filter Error: Requested an illegal VideoInfo");
    pixel_type = space;
    width = WidthCheck(_width, space);
    height = HeightCheck(_height, IsFieldBased(), space);
    num_frames = frame_count;
  }

  bool IsColorSpace(ColorSpace space) const { return (pixel_type & space) == space; } // Clear out additional properties
  bool HasProperty(CS_Property property) const { return (pixel_type & property) == property ; }

  bool IsPlanar() const { return HasProperty(CS_PLANAR); }
 
  bool IsRGB() const { return HasProperty(CS_BGR); }      
  bool IsRGB24() const { return IsColorSpace(CS_BGR24); } 
  bool IsRGB32() const { return IsColorSpace(CS_BGR32); }

  bool IsYUV() const { return HasProperty(CS_YUV); }
  bool IsYUY2() const { return IsColorSpace(CS_YUY2); }  
  bool IsYV12() const { return IsColorSpace(CS_YV12) || IsColorSpace(CS_I420); }

  bool IsVPlaneFirst() const { return IsYV12(); }  // Don't use this 
  //defining all these with the first two, will help if we have to change how ColorSpaces are handled

  unsigned GetFPSNumerator() const { return fps_numerator; }
  unsigned GetFPSDenominator() const { return fps_denominator; }

  double GetFPS() const { return fps_numerator/fps_denominator; }

  void SetFPS(unsigned numerator, unsigned denominator) {
    unsigned x = numerator, y = denominator;
    while (y) {   // find gcd
      unsigned t = x%y; x = y; y = t;
    }
    fps_numerator = numerator/x;
    fps_denominator = denominator/x;
  }

  int BytesFromPixels(int pixels) const { return pixels * (BitsPerPixel()>>3); }   // Will not work on planar images, but will return only luma planes
  int RowSize() const { return BytesFromPixels(width); }  // Also only returns first plane on planar images
  int BMPSize() const { if (IsPlanar()) {int p = height * ((RowSize()+3) & ~3); p+=p>>1; return p;  } return height * ((RowSize()+3) & ~3); }

  int BitsPerPixel() const { 
    switch (pixel_type) {
      case CS_BGR24:
        return 24;
      case CS_BGR32:
        return 32;
      case CS_YUY2:
        return 16;
      case CS_YV12:
      case CS_I420:
        return 12;
      default:
        return 0;
    }
  }


  /*
   * Audio stuff
   */
private:

  SampleType sample_type;           
  int audio_samples_per_second;   // 0 means no audio
  __int64 num_audio_samples;      // changed as of 2.5
  int nchannels;                  // as of 2.5

public:

  bool HasAudio() const { return audio_samples_per_second != 0; }
  void RemoveAudio() { audio_samples_per_second = 0; }

  SampleType GetSampleType() const { return sample_type; }
  int GetChannelCount() const { return nchannels; }
  int GetSamplesPerSecond() const { return audio_samples_per_second; }
  __int64 GetSamplesCount() const { return num_audio_samples; }

  void SetAudio(SampleType _sample_type, int _nchannels, int samples_per_sec, __int64 samples_count) {
    if (_nchannels <= 0 || samples_per_sec < 0 || samples_count < 0)
      throw AvisynthError("Filter Error: Requested an illegal VideoInfo");
    sample_type = _sample_type;
    nchannels = nchannels;
    audio_samples_per_second = samples_per_sec;
    num_audio_samples = samples_count;
  }

  int FramesFromAudioSamples(__int64 samples) const { return (int)(samples * (__int64)fps_numerator / (__int64)fps_denominator / (__int64)audio_samples_per_second); }
  __int64 AudioSamplesFromFrames(__int64 frames) const { return (__int64(frames) * audio_samples_per_second * fps_denominator / fps_numerator); }
  __int64 AudioSamplesFromBytes(__int64 bytes) const { return bytes / BytesPerAudioSample(); }
  __int64 BytesFromAudioSamples(__int64 samples) const { return samples * BytesPerAudioSample(); }

  int BytesPerAudioSample() const { return nchannels * BytesPerChannelSample();}
  int BytesPerChannelSample() const { 
    static int bytes[] = { sizeof(signed char), sizeof(signed short), 3, sizeof(signed int), sizeof(SFLOAT) };
    return bytes[sample_type];
  }


 
  /*
   * Field based stuff
   */
private:

  int image_type;

  enum {
    IT_BFF = 1<<0,
    IT_TFF = 1<<1,
    IT_FIELDBASED = 1<<2
  };

public:
  bool IsFieldBased() const { return image_type & IT_FIELDBASED != 0; }
  bool IsBFF() const { return image_type & IT_BFF != 0; }
  bool IsTFF() const { return image_type & IT_TFF != 0; }
  bool IsParityKnown() const { return IsFieldBased() && (IsBFF() || IsTFF()); }
  
  void SetFieldBased(bool isfieldbased)  { 
    if (isfieldbased) {
      HeightCheck(height, true, pixel_type);
      image_type |= IT_FIELDBASED;
    } else 
        image_type &= ~IT_FIELDBASED;
  }
  void SetBFF() { image_type |= (IT_FIELDBASED | IT_BFF); image_type &= ~IT_TFF; }
  void SetTFF() { image_type |= (IT_FIELDBASED | IT_TFF); image_type &= ~IT_BFF; }

};

enum {
  FILTER_TYPE=1,
  FILTER_INPUT_COLORSPACE=2,
  FILTER_OUTPUT_TYPE=9,
  FILTER_NAME=4,
  FILTER_AUTHOR=5,
  FILTER_VERSION=6,
  FILTER_ARGS=7,
  FILTER_ARGS_INFO=8,
  FILTER_ARGS_DESCRIPTION=10,
  FILTER_DESCRIPTION=11,
};
enum {  //SUBTYPES
  FILTER_TYPE_AUDIO=1,
  FILTER_TYPE_VIDEO=2,
  FILTER_OUTPUT_TYPE_SAME=3,
  FILTER_OUTPUT_TYPE_DIFFERENT=4,
};


/**************************************************************************************
 *  RefCounted class                                                                  *
 **************************************************************************************
  base class for all the refcounted object (IClip, VideoFrame, VideoFrameBuffer...)
  refcount access are limited here (so multithread changes are limited here too)
  in multithread, all methods involving refcount should be made atomic
*/

class smart_ptr_base;

class NotClonable { };  //exception for illegal use of clone

class RefCounted {

  int refcount;

  void InitAddRef() { refcount += (refcount == -1)? 2 : 1; } //used by smart ptrs when getting a ptr
  void AddRef() { ++refcount; }
  void Release() { if (--refcount == 0) delete this; }
  
  friend class smart_ptr_base;

protected:
  //refcount starts at -1 to prevent confusion between new instances and unreferrenced ones 
  //will help achieve thread-safety in instance recycling
  RefCounted() : refcount(-1) { } 

  //clone method, used by the conversion between smart_ptr and smart_ptr_to_cst
  //if your subclass represent constant objects, you can even make it return this (by casting away constness)
  virtual RefCounted * clone() const throw(NotClonable) { throw NotClonable(); }
  
  virtual ~RefCounted() { }  //virtual destructor 

public:
  bool isShared() { return refcount > 1; }  //used to decide whether clone or steal by smart pointers

};


/****************************************************************************************
 * smart pointers classes (refcounting pointers)                                        *
 ****************************************************************************************
  method of conversion between 'smart const T *' and 'smart T *' are provided
  they copy the pointed object if its shared with others, otherwise steal it
*/

//base class for the smart pointers classes
//(would have been a template if (stupid) VC6 understood template friend declarations)
class smart_ptr_base {

protected:
  RefCounted * ptr;

  smart_ptr_base() : ptr(NULL) { }
  smart_ptr_base(RefCounted * _ptr) : ptr(_ptr) { if (ptr) ptr->InitAddRef(); }
  smart_ptr_base(const smart_ptr_base& other) : ptr(other.ptr) { if (ptr) ptr->AddRef(); }  

  inline void Set(RefCounted * newPtr)
  { 
    if (ptr != newPtr) { 
      if (ptr) ptr->Release(); 
      ptr = newPtr; 
      if (ptr) ptr->InitAddRef(); 
    } 
  } 

  inline void Copy(const smart_ptr_base& other)
  { 
    if (ptr != other.ptr) {
      if (ptr) ptr->Release();      
      ptr = other.ptr;
      if (ptr) ptr->AddRef();
    }  
  }

  inline void Clone(const smart_ptr_base& other) { 
    _ASSERTE(this != &other);
    if (ptr) ptr->Release();
    if (other.ptr) {
      ptr = other.ptr->clone();
      ptr->InitAddRef();
    }
    else ptr = NULL;    
  }

  inline void StealOrClone(smart_ptr_base& other)
  {    
    _ASSERTE(this != &other);
    if (ptr) ptr->Release();
    if (other.ptr && other.ptr->isShared()) { //if shared
      ptr = other.ptr->clone();
      ptr->InitAddRef();          //we make a copy for ourselves (and let the old one live)
    } else {
      ptr = other.ptr;            //we steal ptr
      other.ptr = NULL;
    }
  } 

  inline void SelfClone()
  {
    if (ptr) {
      RefCounted * newPtr = ptr->clone();
      newPtr->InitAddRef();   //must be done first, clone is allowed to return this...
      ptr->Release();         //otherwise we might end up destroying the object
      ptr = newPtr;
    }
  }

public:
  void release() { if (ptr) { ptr->Release(); ptr = NULL; } } //smart ptr voids itself
  
  operator bool() const { return ptr != NULL; }  //useful in boolean expressions

  ~smart_ptr_base() { if (ptr) ptr->Release(); }

};

template <class T> class smart_ptr;

//smart const T * 
template <class T> class smart_ptr_to_cst : public smart_ptr_base {

public:
  smart_ptr_to_cst() { }
  smart_ptr_to_cst(T* _ptr) : smart_ptr_base(_ptr) { }
  smart_ptr_to_cst(const T *_ptr) : smart_ptr_base(const_cast<T*>(_ptr)) { }
  //casting constructor 
  template <class Y> smart_ptr_to_cst(const smart_ptr_to_cst<Y>& other) : smart_ptr_base(other) { }
  smart_ptr_to_cst(smart_ptr<T>& other);
  smart_ptr_to_cst(const smart_ptr<T>& other);


  void swap(smart_ptr_to_cst<T>& other) { std::swap(ptr, other.ptr); }

  const smart_ptr_to_cst<T>& operator =(T* newPtr) { Set(newPtr); return *this; }
  const smart_ptr_to_cst<T>& operator =(const T* newPtr) { Set(const_cast<T*>(newPtr)); return *this; }
  const smart_ptr_to_cst<T>& operator =(const smart_ptr_to_cst<T>& other) { Copy(other); return *this; }
  const smart_ptr_to_cst<T>& operator =(smart_ptr<T>& other);
  const smart_ptr_to_cst<T>& operator =(const smart_ptr<T>& other);
  
  const T * const operator ->() const { return (T*)ptr; }
  const T& operator *() const { return *((T*)ptr); }

  //comparison operators, operate on the pointed object
  bool operator ==(const smart_ptr_to_cst<T>& other) const { return *((T*)ptr) == *((T*)other.ptr); }
  bool operator < (const smart_ptr_to_cst<T>& other) const { return *((T*)ptr) < *((T*)other.ptr); }

};


//smart T * 
//converting a smart const T * to a smart T * (constructor or operator =) will perform cleanup 
template <class T> class smart_ptr : public smart_ptr_base {

public:
  smart_ptr() { }
  smart_ptr(T* _ptr) : smart_ptr_base(_ptr) { }
  //casting constructor 
  template <class Y> smart_ptr(const smart_ptr<Y>& other) : smart_ptr_base(other) { }  
  smart_ptr(smart_ptr_to_cst<T>& other) { StealOrClone(other); }
  smart_ptr(const smart_ptr_to_cst<T>& other) { Clone(other); }

  void swap(smart_ptr<T>& other) { std::swap(ptr, other.ptr); }
  
  const smart_ptr<T>& operator =(T* newPtr) { Set(newPtr); return *this; }
  const smart_ptr<T>& operator =(const smart_ptr<T>& other) { Copy(other); return *this; }
  const smart_ptr<T>& operator =(smart_ptr_to_cst<T>& other) { StealOrClone(other); return *this; }
  const smart_ptr<T>& operator =(const smart_ptr_to_cst<T>& other) { Clone(other);  return *this; }

  T * const operator ->() const { return (T*)ptr; }
  T& operator *() const { return *((T*)ptr); }

  //comparison operators, operate on the pointed object
  bool operator ==(const smart_ptr<T>& other) const { return *((T*)ptr) == *((T*)other.ptr); }
  bool operator < (const smart_ptr<T>& other) const { return *((T*)ptr) < *((T*)other.ptr); }

  //clone the pointed object, ensuring you don't modify something shared by another smart T *  
  void clone() { SelfClone(); }
};


template <class T> smart_ptr_to_cst<T>::smart_ptr_to_cst(smart_ptr<T>& other) { StealOrClone(other); }
template <class T> smart_ptr_to_cst<T>::smart_ptr_to_cst(const smart_ptr<T>& other) { Clone(other); }

template <class T> const smart_ptr_to_cst<T>& smart_ptr_to_cst<T>::operator =(smart_ptr<T>& other) { StealOrClone(other); return *this; }
template <class T> const smart_ptr_to_cst<T>& smart_ptr_to_cst<T>::operator =(const smart_ptr<T>& other) { Clone(other); return *this; }



/********************************************************************************************
 * Polymorphic VideoFrame class                                                             *
 ********************************************************************************************
  VideoFrameBuffer no longer defined in avisynth.h (videoframe.h)
  VideoFrame still use them, but they are hidden by the VideoFrame API
  due to architecture changes (3 buffers when planar), you must no longer assume pitchU = pitchV
  it won't happen often but it can
  MakeWritable is no longer required : CPVideoFrame are never Writable, nor mutable in any sort
  PVideoframe are always writable/mutable, and will copy buffers before returning Write Ptr if they are shared
*/

class VideoFrame;
typedef smart_ptr<VideoFrame> PVideoFrame;
typedef smart_ptr_to_cst<VideoFrame> CPVideoFrame;

enum Plane {
  NOT_PLANAR,
  PLANAR_Y,
  PLANAR_U,
  PLANAR_V
};

class VideoFrame : public RefCounted {

public:

  //some typedefs
  typedef short dimension;  
  typedef signed char Align;
  typedef VideoInfo::ColorSpace ColorSpace;


  //static method to convert index in Plane enum, USE it !!!
  inline static Plane IndexToYUVPlane(int i) {
    static Plane planes[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
    _ASSERTE( i>=0 && i < 3);
    return planes[i];
  }

  class NoSuchPlane { }; //exception for inadequate plane requests 

  //some legacy methods, no longer get plane Y when planar
  dimension GetPitch() const throw(NoSuchPlane) { return GetPitch(NOT_PLANAR); }
  dimension GetRowSize() const throw(NoSuchPlane) { return GetRowSize(NOT_PLANAR); }
  dimension GetAlignedRowSize() const throw(NoSuchPlane) { return GetAlignedRowSize(NOT_PLANAR); }
  dimension GetHeight() const throw(NoSuchPlane) { return GetHeight(NOT_PLANAR); }

  const BYTE * GetReadPtr() const throw(NoSuchPlane) { return GetReadPtr(NOT_PLANAR); }
  BYTE * GetWritePtr() throw(NoSuchPlane) { return GetWritePtr(NOT_PLANAR); }
  
  //new virtual ones
  virtual dimension GetPitch(Plane plane) const throw(NoSuchPlane) = 0;
  virtual dimension GetRowSize(Plane plane) const throw(NoSuchPlane) = 0;
  virtual dimension GetAlignedRowSize(Plane plane) const throw(NoSuchPlane) = 0;
  virtual dimension GetHeight(Plane plane) const throw(NoSuchPlane) = 0;

  virtual const BYTE * GetReadPtr(Plane plane) const throw(NoSuchPlane) = 0;
  virtual BYTE * GetWritePtr(Plane plane) throw(NoSuchPlane) = 0;
  
  //methods to get some other general infos
  virtual ColorSpace GetColorSpace() const = 0;
  bool IsPlanar() const { return GetColorSpace() & VideoInfo::CS_PLANAR != 0; }  
  virtual Align GetAlign() const = 0;

  //frames now know about their fieldbased state
  bool IsFieldBased() const;
  void SetFieldBased(bool fieldBased);

  //method to get Video width and height, default implementation: not planar case
  virtual dimension GetVideoHeight() const = 0; 
  virtual dimension GetVideoWidth() const = 0; 

  //methods used to check that pixels args match Colorspace restrictions
  //they throw the appropriate error msg when arg are illegal
  //you are not supposed to use them, all calls are done internally where needed
  //btw, they don't check sign, just parity and such (sign check are done at a lower level)
  virtual dimension WidthToRowSize(dimension width) const = 0;
  virtual dimension HeightCheck(dimension height) const = 0;

  //construction method, moved from IScriptEnvironment
  static PVideoFrame NewVideoFrame(const VideoInfo& vi, Align align = FRAME_ALIGN);
  //same as the above but with inheritance of the non volatiles properties
  virtual PVideoFrame NewVideoFrame(const VideoInfo& vi, Align align = FRAME_ALIGN) const = 0; 


  /************************************************************************************
   *  various ToolBox methods now provided by the VideoFrame API                       *
   ************************************************************************************/

  //ColorSpace conversion method
  virtual CPVideoFrame ConvertTo(ColorSpace space) const = 0;

  //Please Note that those dimensions are in PIXELS, and can be negative
  //positive values crop, negatives increase size, buffers won't be reallocated if possible
  //if reallocation has to be done, original data is copied at the right place
  virtual void SizeChange(dimension left, dimension right, dimension top, dimension bottom) = 0;

  //Copy other into self, the coords left and top can be negative
  //Only the overlapping part is copied
  virtual void Copy(CPVideoFrame other, dimension left, dimension top) = 0;

  void StackHorizontal(CPVideoFrame other); //implemented using SizeChange and Copy 
  void StackVertical(CPVideoFrame other);   
  
  virtual void FlipVertical() = 0;
  virtual void FlipHorizontal() = 0;

  virtual void TurnLeft() = 0;  //how do we turn a YUY2 frame !?...
  virtual void TurnRight() = 0;

  virtual void Blend(CPVideoFrame other, float factor) = 0;

  //what else ???

  /************************************************************************************
   *  Custom Properties System                                                        *
   ************************************************************************************/

  //class used as a name/type for properties
  //use like this: static PropertyName nameOfMyProperty;
  class PropertyName {
    char dummy;  //to ensure sizeof > 0, so consecutive instances won't fuck up
  public:
    PropertyName() { }
  };

  //Property class, you can subclass it to add parameters you need
  //subclass should implement an accurate operator == taking new parameters into account
  //if your subclass is not made of constant objects, reimplement clone too
  class Property : public RefCounted {
    const PropertyName * name;
    
  protected:
    Property(const Property& other) : name(other.name) { }
    //Property is constant here, so I can return safely this as a clone
    //REIMPLEMENT if your subclass is mutable
    virtual RefCounted * clone() const { return const_cast<Property*>(this); } 

  public:
    Property(const PropertyName& _name) : name(&_name) { }  

    bool IsType(const PropertyName& _name) const { return name == &_name; } //convenience version of IsType
    bool SameTypeAs(const Property& other) const { return name == other.name; }

    virtual bool IsVolatile() const { return false; }   //properties are sticky by default
    
    //virtual == operator, should be reimplemented to take new members into account if subclassed
    virtual bool operator==(const Property& other) const { return SameTypeAs(other); }
    //does not yield a total order when subclassing but since multiple instance of a subclass
    //are not supposed to cohabit it pose no problem.
    bool operator <(const Property& other) const { return name < other.name; }
  };
   
  typedef smart_ptr_to_cst<Property> CPProperty;  //smart const Property *
  typedef smart_ptr<Property> PProperty;  //you are not really supposed to use it, but you can

  virtual CPProperty GetProperty(const PropertyName& name) const = 0;
  virtual void SetProperty(CPProperty prop) = 0;
  void SetProperty(PProperty prop) { SetProperty(CPProperty(prop)); }  //exists so outside PProperty won't be casted to CPProperty by the call of the above and loses its ref
  virtual void RemoveProperty(const PropertyName& name) = 0;

protected:
  //cleanup of all volatiles properties
  virtual void CleanProperties() = 0;
  friend class smart_ptr<VideoFrame>; //so it can clean

};

//template specialisations, so properties will be get cleaned correctly
template <> smart_ptr<VideoFrame>::smart_ptr(smart_ptr_to_cst<VideoFrame>& other) { StealOrClone(other); if (ptr) ((VideoFrame*)ptr)->CleanProperties(); }
template <> smart_ptr<VideoFrame>::smart_ptr(const smart_ptr_to_cst<T>& other) { Clone(other); if (ptr) ((VideoFrame*)ptr)->CleanProperties(); }

template <> const smart_ptr<VideoFrame>& smart_ptr<VideoFrame>::operator =(smart_ptr_to_cst<VideoFrame>& other) { StealOrClone(other); if (ptr) ((VideoFrame*)ptr)->CleanProperties(); return *this; }
template <> const smart_ptr<VideoFrame>& smart_ptr<VideoFrame>::operator =(const smart_ptr_to_cst<VideoFrame>& other) { Clone(other);  if (ptr) ((VideoFrame*)ptr)->CleanProperties(); return *this; }


class IScriptEnvironment;

// Base class for all filters.
class IClip : public RefCounted {

public:

  enum CachePolicy {
    CACHE_NOTHING,
    CACHE_RANGE,
    CACHE_LAST
  };

  IClip() {}

  virtual int __stdcall GetVersion() { return AVISYNTH_INTERFACE_VERSION; }
  
  virtual CPVideoFrame __stdcall GetFrame(int n) = 0;
  virtual bool __stdcall GetParity(int n) = 0;  // return field parity if field_based, else parity of first field in frame
  virtual void __stdcall GetAudio(void* buf, __int64 start, __int64 count) = 0;  // start and count are in samples
  virtual void __stdcall SetCacheHints(CachePolicy policy, int size) { }  // We do not pass cache requests upwards, only to the next filter.
  virtual const VideoInfo& __stdcall GetVideoInfo() = 0;

};

typedef smart_ptr<IClip> PClip;  // smart pointer to IClip




enum AVSType {
  VOID_T,
  CLIP_T,
  BOOL_T,
  INT_T,
  //LONG_T,
  FLOAT_T,
  STRING_T,
  ARRAY_T
};

/*template <class T> class avs { static AVSType type; };

AVSType avs<void>::type  = VOID_T;
AVSType avs<PClip>::type = CLIP_T;
AVSType avs<bool>::type  = BOOL_T;
AVSType avs<int>::type   = INT_T;
//AVSType avs<__int64>::type = LONG_T;
AVSType avs<float>::type = FLOAT_T;
AVSType avs<string>::type = STRING_T;  */

class AVSValue {

  AVSType type;
  bool defined;

  void * data;
  
  void * buildValue() {
    switch(type)
    {      
      case CLIP_T:   return new PClip();
      case INT_T:    return new int;
      case BOOL_T:   return new bool;
      case FLOAT_T:  return new float;
      case STRING_T: return new string();
    }
    return NULL;  
  }
  void build() { data = buildValue(); }

  void destroy()
  {
    switch(type)
    {
      case CLIP_T:   delete (PClip *)data; break;
      case INT_T:    delete (int *)data; break;
      case BOOL_T:   delete (bool *)data; break;
      case FLOAT_T:  delete (float *)data; break;
      case STRING_T: delete (string *)data; break;
    }
  }

  void Assign(const AVSValue& other) {
    destroy();
    type = other.type;
    build();
    switch(type)
    {
      case CLIP_T:   *((PClip *)data) = *((PClip *)other.data); break;
      case INT_T:    *((int *)data) = *((int *)other.data); break;
      case BOOL_T:   *((bool *)data) = *((bool *)other.data); break;
      case FLOAT_T:  *((float *)data) = *((float *)other.data); break;
      case STRING_T: *((string *)data) = *((string *)other.data); break;
    }    
  }

  //three methods repeatly used in operators
  inline TypeCheck(AVSType expected) const {
    if (expected != type)
      throw InternalError("improper use of an AVSValue");
  }
  inline BeforeAffectCheck(AVSType expected)
  {
    TypeCheck(expected);
    defined = true;
  }
  inline BeforeCastCheck(AVSType expected) const
  {
    TypeCheck(expected);
    if (! defined)
      throw AvisynthError("use of an AVSValue who was not initialised");
  }

public:

  AVSValue() : defined(false), type(VOID_T) { }
  AVSValue(const AVSValue& other) : type(VOID_T) { Assign(other); }

  ~AVSValue() { destroy(); }

  //constructor by type, used by the parser
  AVSValue(AVSType _type) : defined(false), type(_type) { build(); }

  AVSValue(PClip clip)      : defined(true), type(CLIP_T)   { build(); *((PClip *)data) = clip; }
  AVSValue(int b)          : defined(true), type(BOOL_T)   { build(); *((int *)data) = b; }
  AVSValue(bool i)           : defined(true), type(INT_T)    { build(); *((bool *)data) = i; }
  AVSValue(float f)         : defined(true), type(FLOAT_T)  { build(); *((float *)data) = f; }
  AVSValue(const string& s) : defined(true), type(STRING_T) { build(); *((string *)data) = s; }


  const AVSValue& operator=(const AVSValue& other) { Assign(other); return *this; }

  // Note that we transparently allow 'int' to be treated as 'float'.
  // There are no int<->bool conversions, though..

  const AVSValue& operator=(PClip value)  { BeforeAffectCheck(CLIP_T);   *((PClip *)data)  = value; return *this; }
  const AVSValue& operator=(int value)    { 
    if (type == FLOAT_T) 
      return operator=( float(value) ); //allows float avsvalues to receive int values
    BeforeAffectCheck(INT_T);    *((int *)data)    = value; return *this;
  }
  const AVSValue& operator=(bool value)   { BeforeAffectCheck(BOOL_T);   *((bool *)data)   = value; return *this; }
  const AVSValue& operator=(float value)  { BeforeAffectCheck(FLOAT_T);  *((float *)data)  = value; return *this; }
  const AVSValue& operator=(string value) { BeforeAffectCheck(STRING_T); *((string *)data) = value; return *this; }

  operator PClip() const  { BeforeCastCheck(CLIP_T); return *((PClip *)data); }
  operator int()   const  { BeforeCastCheck(INT_T);  return *((int *)data); }
  operator bool()  const  { BeforeCastCheck(BOOL_T); return *((bool *)data); }
  operator float() const  { 
    if (type == INT_T)
      return operator int();  //alows int avsvalues to be treated as float
    BeforeCastCheck(FLOAT_T); return *((float *)data);
  }
  operator string() const { BeforeCastCheck(STRING_T); return *((string *)data); }

  //may come in handy in some overload resolution
  operator double() const { return operator float(); }

  //test if initialised
  bool Defined() const { return defined; }
  AVSType GetType() const { return type; }

  bool Is(AVSType _type) const { return type == _type; }

  bool IsClip()   const { return Is(CLIP_T); }
  bool IsBool()   const { return Is(BOOL_T); }
  bool IsInt()    const { return Is(INT_T); }
  bool IsFloat()  const { return Is(FLOAT_T) || Is(INT_T); }
  bool IsString() const { return Is(STRING_T); }

};

typedef vector<AVSValue> ArgVector;


// instanciable null filter that forwards all requests to child
// use for filter who don't change VideoInfo
class StableVideoFilter : public IClip {

protected:
  PClip child;

  //protected constructor
  StableVideoFilter(PClip _child) : child(_child) { }

public:
  CPVideoFrame __stdcall GetFrame(int n) { return child->GetFrame(n); }
  void __stdcall GetAudio(void* buf, __int64 start, __int64 count) { child->GetAudio(buf, start, count); }
  const VideoInfo& __stdcall GetVideoInfo() { return child->GetVideoInfo(); }
  bool __stdcall GetParity(int n) { return child->GetParity(n); }
};


// instance null filter
// use when VideoInfo is changed
class GenericVideoFilter : public StableVideoFilter {

protected:
  VideoInfo vi;

  //protected constructor
  GenericVideoFilter(PClip _child, const VideoInfo& _vi) : StableVideoFilter(_child), vi(_vi) { }

    
public:
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }

};








/* Helper classes useful to plugin authors */


class ConvertAudio : public GenericVideoFilter 
/**
  * Helper class to convert audio to any format
 **/
{
public:
  ConvertAudio(PClip _clip, int prefered_format);
  void __stdcall GetAudio(void* buf, __int64 start, __int64 count);

  static PClip Create(PClip clip, int sample_type, int prefered_type);
  static AVSValue __cdecl Create_float(AVSValue args, void*);
  static AVSValue __cdecl Create_32bit(AVSValue args, void*);
  static AVSValue __cdecl Create_16bit(AVSValue args, void*);
  static AVSValue __cdecl Create_8bit(AVSValue args, void*);
  virtual ~ConvertAudio()
  {if (tempbuffer_size) {delete[] tempbuffer;tempbuffer_size=0;}}
private:
  void convertToFloat(char* inbuf, float* outbuf, char sample_type, int count);
  void convertFromFloat(float* inbuf, void* outbuf, char sample_type, int count);

  __inline int Saturate_int8(float n);
  __inline short Saturate_int16(float n);
  __inline int Saturate_int24(float n);
  __inline int Saturate_int32(float n);

  char src_format;
  char dst_format;
  int src_bps;
  char *tempbuffer;
  SFLOAT *floatbuffer;
  int tempbuffer_size;
};


// For GetCPUFlags.  These are backwards-compatible with those in VirtualDub.
enum {                    
                    /* slowest CPU to support extension */
  CPUF_FORCE			  = 0x01,   // N/A
  CPUF_FPU			    = 0x02,   // 386/486DX
  CPUF_MMX			    = 0x04,   // P55C, K6, PII
  CPUF_INTEGER_SSE	= 0x08,		// PIII, Athlon
  CPUF_SSE			    = 0x10,		// PIII, Athlon XP/MP
  CPUF_SSE2			    = 0x20,		// PIV, Hammer
  CPUF_3DNOW			  = 0x40,   // K6-2
  CPUF_3DNOW_EXT		= 0x80,		// Athlon
  CPUF_X86_64       = 0xA0,   // Hammer (note: equiv. to 3DNow + SSE2, which only Hammer
                              //         will have anyway)
};
#define MAX_INT 0x7fffffff
#define MIN_INT 0x80000000



class IScriptEnvironment {
public:
  virtual __stdcall ~IScriptEnvironment() {}

  virtual /*static*/ long __stdcall GetCPUFlags() = 0;


  __declspec(noreturn) void __stdcall ThrowError(const string& err_msg) { throw AvisynthError(err_msg); }

  class NotFound /*exception*/ {};  // thrown by Invoke and GetVar

  typedef AVSValue (__cdecl *ApplyFunc)(AVSValue args, void* user_data, IScriptEnvironment* env);

  virtual void __stdcall AddFunction(const char* name, const char* params, ApplyFunc apply, void* user_data) = 0;
  virtual bool __stdcall FunctionExists(const char* name) = 0;
  virtual AVSValue __stdcall Invoke(const char* name, const AVSValue args, const char** arg_names=0) = 0;

  virtual AVSValue __stdcall GetVar(const char* name) = 0;
  virtual bool __stdcall SetVar(const char* name, const AVSValue& val) = 0;
  virtual bool __stdcall SetGlobalVar(const char* name, const AVSValue& val) = 0;

  virtual void __stdcall PushContext(int level=0) = 0;
  virtual void __stdcall PopContext() = 0;


  static void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height);

  typedef void (__cdecl *ShutdownFunc)(void* user_data, IScriptEnvironment* env);
  virtual void __stdcall AtExit(ShutdownFunc function, void* user_data) = 0;

  virtual void __stdcall CheckVersion(int version = AVISYNTH_INTERFACE_VERSION) = 0;

	virtual int __stdcall SetMemoryMax(int mem) = 0;

  virtual int __stdcall SetWorkingDir(const char * newdir) = 0;

};


// avisynth.dll exports this; it's a way to use it as a library, without
// writing an AVS script or without going through AVIFile.
IScriptEnvironment* __stdcall CreateScriptEnvironment(int version = AVISYNTH_INTERFACE_VERSION);


#pragma pack(pop)

#endif //__AVISYNTH_H__
