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

enum { AVISYNTH_INTERFACE_VERSION = 2 };


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

// The VideoInfo struct holds global information about a clip (i.e.
// information that does not depend on the frame number).  The GetVideoInfo
// method in IClip returns this struct.

// Audio Sample information
typedef float SFLOAT;

enum {SAMPLE_INT8  = 1<<0,
        SAMPLE_INT16 = 1<<1, 
        SAMPLE_INT24 = 1<<2,    // Int24 is a very stupid thing to code, but it's supported by some hardware.
        SAMPLE_INT32 = 1<<3,
        SAMPLE_FLOAT = 1<<4};

enum {
   PLANAR_Y=1<<0,
   PLANAR_U=1<<1,
   PLANAR_V=1<<2,
   PLANAR_ALIGNED=1<<3,
   PLANAR_Y_ALIGNED=PLANAR_Y|PLANAR_ALIGNED,
   PLANAR_U_ALIGNED=PLANAR_U|PLANAR_ALIGNED,
   PLANAR_V_ALIGNED=PLANAR_V|PLANAR_ALIGNED,
  };

struct VideoInfo {
  int width, height;    // width=0 means no video
  unsigned fps_numerator, fps_denominator;
  int num_frames;
  // This is more extensible than previous versions. More properties can be added seeminglesly.

  // Colorspace properties.
  enum {
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
  int pixel_type;                // changed to int as of 2.5
  

  int audio_samples_per_second;   // 0 means no audio
  int sample_type;                // as of 2.5
  __int64 num_audio_samples;      // changed as of 2.5
  int nchannels;                  // as of 2.5

  // Imagetype properties

  int image_type;

  enum {
    IT_BFF = 1<<0,
    IT_TFF = 1<<1,
    IT_FIELDBASED = 1<<2
  };

  // useful functions of the above
  bool HasVideo() const { return (width!=0); }
  bool HasAudio() const { return (audio_samples_per_second!=0); }
  bool IsRGB() const { return !!(pixel_type&CS_BGR); }
  bool IsRGB24() const { return (pixel_type&CS_BGR24)==CS_BGR24; } // Clear out additional properties
  bool IsRGB32() const { return (pixel_type & CS_BGR32) == CS_BGR32 ; }
  bool IsYUV() const { return !!(pixel_type&CS_YUV ); }
  bool IsYUY2() const { return (pixel_type & CS_YUY2) == CS_YUY2; }  
  bool IsYV12() const { return ((pixel_type & CS_YV12) == CS_YV12)||((pixel_type & CS_I420) == CS_I420); }
  bool IsColorSpace(int c_space) const { return ((pixel_type & c_space) == c_space); }
  bool Is(int property) const { return ((pixel_type & property)==property ); }
  bool IsPlanar() const { return !!(pixel_type & CS_PLANAR); }
  bool IsFieldBased() const { return !!(image_type & IT_FIELDBASED); }
  bool IsParityKnown() const { return ((image_type & IT_FIELDBASED)&&(image_type & (IT_BFF||IT_TFF))); }
  bool IsBFF() const { return !!(pixel_type & IT_BFF); }
  bool IsTFF() const { return !!(pixel_type & IT_TFF); }
  
  bool IsVPlaneFirst() const {return ((pixel_type & CS_YV12) == CS_YV12); }  // Don't use this 
  int BytesFromPixels(int pixels) const { return pixels * (BitsPerPixel()>>3); }   // Will not work on planar images, but will return only luma planes
  int RowSize() const { return BytesFromPixels(width); }  // Also only returns first plane on planar images
  int BMPSize() const { if (IsPlanar()) {int p = height * ((RowSize()+3) & ~3); p+=p>>1; return p;  } return height * ((RowSize()+3) & ~3); }
  __int64 AudioSamplesFromFrames(__int64 frames) const { return (__int64(frames) * audio_samples_per_second * fps_denominator / fps_numerator); }
  int FramesFromAudioSamples(__int64 samples) const { return (int)(samples * (__int64)fps_numerator / (__int64)fps_denominator / (__int64)audio_samples_per_second); }
  __int64 AudioSamplesFromBytes(__int64 bytes) const { return bytes / BytesPerAudioSample(); }
  __int64 BytesFromAudioSamples(__int64 samples) const { return samples * BytesPerAudioSample(); }
  int AudioChannels() const { return nchannels; }
  int SampleType() const{ return sample_type;}
  int SamplesPerSecond() const { return audio_samples_per_second; }
  int BytesPerAudioSample() const { return nchannels*BytesPerChannelSample();}
  void SetFieldBased(bool isfieldbased)  { if (isfieldbased) image_type|=IT_FIELDBASED; else  image_type&=~IT_FIELDBASED; }
  void Set(int property)  { image_type|=property; }
  void Clear(int property)  { image_type&=~property; }

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
  int BytesPerChannelSample() const { 
    switch (sample_type) {
    case SAMPLE_INT8:
      return sizeof(signed char);
    case SAMPLE_INT16:
      return sizeof(signed short);
    case SAMPLE_INT24:
      return 3;
    case SAMPLE_INT32:
      return sizeof(signed int);
    case SAMPLE_FLOAT:
      return sizeof(SFLOAT);
    default:
      _ASSERTE("Sample type not recognized!");
      return 0;
    }
  }

  // useful mutator
  void SetFPS(unsigned numerator, unsigned denominator) {
    unsigned x=numerator, y=denominator;
    while (y) {   // find gcd
      unsigned t = x%y; x = y; y = t;
    }
    fps_numerator = numerator/x;
    fps_denominator = denominator/x;
  }
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

  void InitAddRef() { refcount += (refcount == -1)? 2 : 1; }
  void AddRef() { ++refcount; }
  void Release() { if (--refcount == 0) recycle(); }
  
  friend class smart_ptr_base;

protected:
  //refcount starts at -1 to prevent confustion between new instances and unreferrenced ones 
  //will help achieve thread-safety in instance recycling
  RefCounted() : refcount(-1) { } 

  //method to put oneself in a recyclable state, default implementation = suicide 
  //subclass who recycle instance must redefine it
  virtual void recycle() { delete this; } 

  //clean volatile data (if there is any), performed when going from smart_ptr_to_cst to a smart_ptr
  virtual void clean() { }   //default implementation : doing nothing

  //clone method, used by the conversion between smart_ptr and smart_ptr_to_cst
  virtual RefCounted * clone() const { throw NotClonable(); }
  
  virtual ~RefCounted() { }  

public:
  bool isFree() { return refcount == 0; }   //useful for instance recycling
  bool isShared() { return refcount > 1; }  //used to decide whether clone or steal by smart pointers

};


/****************************************************************************************
 * smart pointers classes (refcounting pointers)                                        *
 ****************************************************************************************
  method of conversion between 'smart const T *' and 'smart T *' are provided
  they copy the pointed object if its shared with others, otherwise steal it
*/

//base class for the smart pointers classes
class smart_ptr_base {

  void Release() { if (ptr) ptr->Release(); }
  void AddRef() { if (ptr) ptr->AddRef(); }
  void InitAddRef() { if (ptr) ptr->InitAddRef(); }

protected:
  RefCounted * ptr;

  smart_ptr_base() : ptr(NULL) { }
  smart_ptr_base(RefCounted * _ptr) : ptr(_ptr) { InitAddRef(); }
  smart_ptr_base(const smart_ptr_base& other) : ptr(other.ptr) { AddRef(); }  

  void Set(RefCounted * newPtr)
  { 
    if (ptr != newPtr) { 
      Release(); 
      ptr = newPtr; 
      InitAddRef(); 
    } 
  } 

  void Copy(const smart_ptr_base& other)
  { 
    if (ptr != other.ptr) {
      Release(); 
      ptr = other.ptr;
      AddRef();
    }  
  }

  void Clone(const smart_ptr_base& other) { Set( (other.ptr)? other.ptr->clone() : NULL); }

  void StealOrClone(smart_ptr_base& other)
  {    
    if (other.ptr && other.ptr->isShared())  //if shared
      Set(other.ptr->clone()); //we make a copy for ourselves (and let the old one live)
    else {
      Release();
      ptr = other.ptr;          //we steal ptr
      other.ptr = NULL;
    }
  } 

  void Clean() { if (ptr) ptr->clean(); }

public:
  void release() { Set(NULL); }  //smart ptr voids itself
  
  operator bool() const { return ptr != NULL; }  //useful in boolean expressions

  //probably useless, allow logically illegal comparisons but that's not really a problem
  bool operator ==(const smart_ptr_base& other) const { return ptr == other.ptr; }
  bool operator < (const smart_ptr_base& other) const { return ptr < other.ptr; }

  ~smart_ptr_base() { Release(); }

};

template <class T> class smart_ptr;

//smart const T * 
template <class T> class smart_ptr_to_cst : public smart_ptr_base {

public:
  smart_ptr_to_cst() { }
  smart_ptr_to_cst(T* _ptr) : smart_ptr_base<T>(_ptr) { }
  smart_ptr_to_cst(const smart_ptr_to_cst<T>& other) : smart_ptr_base(other) { }
  smart_ptr_to_cst(smart_ptr<T>& other);
  smart_ptr_to_cst(const smart_ptr<T>& other);


  void swap(smart_ptr_to_cst<T>& other) { std::swap(ptr, other.ptr); }

  const smart_ptr_to_cst<T>& operator =(T* newPtr) { Set(newPtr); return *this; }
  const smart_ptr_to_cst<T>& operator =(const smart_ptr_to_cst<T>& other) { Copy(other); return *this; }
  const smart_ptr_to_cst<T>& operator =(smart_ptr<T>& other);
  const smart_ptr_to_cst<T>& operator =(const smart_ptr<T>& other);
  
  const T * const operator ->() const { return (T*)ptr; }
  const T& operator *() const { return *((T*)ptr); }
};


//smart T * 
//converting a smart const T * to a smart T * (constructor or operator =) will perform cleanup 
template <class T> class smart_ptr : public smart_ptr_base {

public:
  smart_ptr() { }
  smart_ptr(T* _ptr) : smart_ptr_base(_ptr) { }
  smart_ptr(const smart_ptr<T>& other) : smart_ptr_base(other) { }  
  smart_ptr(smart_ptr_to_cst<T>& other) { StealOrClone(other); Clean(); }
  smart_ptr(const smart_ptr_to_cst<T>& other) { Clone(other); Clean(); }

  void swap(smart_ptr<T>& other) { std::swap(ptr, other.ptr); }
  
  const smart_ptr<T>& operator =(T* newPtr) { Set(newPtr); return *this; }
  const smart_ptr<T>& operator =(const smart_ptr<T>& other) { Copy(other); return *this; }
  const smart_ptr<T>& operator =(smart_ptr_to_cst<T>& other) { StealOrClone(other); Clean(); return *this; }
  const smart_ptr<T>& operator =(const smart_ptr_to_cst<T>& other) { Clone(other); Clean(); return *this; }

  T * const operator ->() const { return (T*)ptr; }
  T& operator *() const { return *((T*)ptr); }

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

// polymorphic VideoFrame class

class VideoFrame : public RefCounted {

public:

  typedef unsigned short dimension;  //in Bytes when horizontal, not pixels
  typedef signed char Align;

  enum Plane {
    NOT_PLANAR,
    PLANAR_Y,
    PLANAR_U,
    PLANAR_V
  };

  //method to convert index in Plane enum, USE it !!!
  static Plane IndexToYUVPlane(int i) {
    static Plane planes[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
    _ASSERTE( i>=0 && i < 3);
    return planes[i];
  }

  class NoSuchPlane { }; //exception for inadequate plane requests
  class IllegalDimension { };

  //some legacy methods
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


  virtual VideoInfo::ColorSpace GetColorSpace() const = 0;
  bool IsPlanar() const { return GetColorSpace() & VideoInfo::CS_PLANAR != 0; }
  dimension GetVideoHeight() const { return GetHeight( IsPlanar()? PLANAR_Y : NOT_PLANAR ): }
  //dimension GetVideoWidth() const { return GetRowSize( IsPlanar()? PLANAR_Y : NOT_PLANAR ) /

  //construction method
  PVideoFrame NewVideoFrame(const VideoInfo& vi, Align align);


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
  class Property : public RefCounted {
    const PropertyName * const name;

  protected:
    Property(const Property& other) : name(other.name) { } 
    virtual RefCounted * clone() const { return new Property(*this); } //probably useless, but...

  public:
    Property(const PropertyName& _name) : name(&_name) { }  //convenience constructor

    bool IsType(const PropertyName& _name) const { return name == &_name; } //convenience version of IsType
    bool SameTypeAs(const Property& other) const { return name == other.name; }

    virtual bool operator==(const Property& other) const { return SameTypeAs(other); }
  };
   
  typedef smart_ptr_to_cst<Property> CPProperty;
  typedef smart_ptr<Property> PProperty;  //you are not really supposed to use it, but you can

  virtual const Property * GetProperty(const PropertyName& name) const = 0;
  virtual void SetProperty(const CPProperty& prop) = 0;
  void SetProperty(const PProperty& prop) { SetProperty(CPProperty(prop)); }  //exists so outside PProperty won't be casted to CPProperty by the call of the above and loses its ref
  virtual void RemoveProperty(const PropertyName& name) = 0;


};





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
  
  virtual PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) = 0;
  virtual bool __stdcall GetParity(int n) = 0;  // return field parity if field_based, else parity of first field in frame
  virtual void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) = 0;  // start and count are in samples
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
  short array_size;  
  union {
    bool boolean;
    int integer;
    float floating_pt;
    const char* str;
    const AVSValue* array;
    //__int64 longlong;
  };
  PClip clip;

  /*enum { DataSize = max( sizeof(string), max(sizeof(PClip), sizeof(float)) ) }; //..sizeof(__int64)

  BYTE data[DataSize];

  void destroy() { 
    if (type == CLIP_T)
      ((PClip*)data)->~PClip();
    else
      if (type == STRING_T)
        ((string*)data)->~string();    
  }

  void construct() {
    if (type == CLIP_T)
      new(data) PClip();
    else
      if (type == STRING_T)
        new(data) string();    
  }*/

  void Assign(const AVSValue& other) {
    //destroy();
    type = other.type;
    //construct();
    if (defined = other.defined) {
      array_size = other.array_size;
      floating_pt = other.floating_pt;
  	  clip = other.clip;
    }
  }

public:

  AVSValue() : defined(false), type(VOID_T) { }
  //AVSValue(AVSType _type) : defined(false), type(_type) { }
  AVSValue(IClip* c) : defined(true), type(CLIP_T), clip(c) { }
  AVSValue(const PClip& c) : defined(true), type(CLIP_T), clip(c) { }
  AVSValue(bool b) : defined(true), type(BOOL_T), boolean(b) { }
  AVSValue(int i) : defined(true), type(INT_T), integer(i) { }
  //AVSValue(__int64 l), defined(true), type(LONG_T), longlong(l) { }
  AVSValue(float f) : defined(true), type(FLOAT_T), floating_pt(f) { }
  AVSValue(double f) : defined(true), type(FLOAT_T), floating_pt(f) { }
  AVSValue(const char* s) : defined(true), type(STRING_T), str(s) { }
  AVSValue(const AVSValue* a, int size) : defined(true), type(ARRAY_T), array(a), array_size(size) { }
  AVSValue(const AVSValue& other) { Assign(other); }

  ~AVSValue() { }
  const AVSValue& operator=(const AVSValue& other) { Assign(other); return *this; }

  // Note that we transparently allow 'int' to be treated as 'float'.
  // There are no int<->bool conversions, though.

  bool Defined() const { return defined; }
  bool IsClip() const { return type == CLIP_T; }
  bool IsBool() const { return type == BOOL_T; }
  bool IsInt() const { return type == INT_T; }
//  bool IsLong() const { return (type == LONG_T || type == INT_T); }
  bool IsFloat() const { return type == FLOAT_T || type == INT_T; }
  bool IsString() const { return type == STRING_T; }
  bool IsArray() const { return type == ARRAY_T; }

  PClip AsClip() const { _ASSERTE(IsClip()); return IsClip()?clip:0; }
  bool AsBool() const { _ASSERTE(IsBool()); return boolean; }
  int AsInt() const { _ASSERTE(IsInt()); return integer; }   
//  int AsLong() const { _ASSERTE(IsLong()); return longlong; } 
  const char* AsString() const { _ASSERTE(IsString()); return IsString()?str:0; }
  double AsFloat() const { _ASSERTE(IsFloat()); return IsInt()?integer:floating_pt; }

  bool AsBool(bool def) const { _ASSERTE(IsBool()||!Defined()); return IsBool() ? boolean : def; }
  int AsInt(int def) const { _ASSERTE(IsInt()||!Defined()); return IsInt() ? integer : def; }
  double AsFloat(double def) const { _ASSERTE(IsFloat()||!Defined()); return IsInt() ? integer : type==FLOAT_T ? floating_pt : def; }
  const char* AsString(const char* def) const { _ASSERTE(IsString()||!Defined()); return IsString() ? str : def; }

  int ArraySize() const { _ASSERTE(IsArray()); return IsArray()?array_size:1; }
  const AVSValue& operator[](int index) const {
    _ASSERTE(IsArray() && index>=0 && index<array_size);
    return (IsArray() && index>=0 && index<array_size) ? array[index] : *this;
  }

};


// instantiable null filter
class GenericVideoFilter : public IClip {
protected:
  PClip child;
  VideoInfo vi;
public:
  GenericVideoFilter(PClip _child) : child(_child) { vi = child->GetVideoInfo(); }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) { return child->GetFrame(n, env); }
  void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) { child->GetAudio(buf, start, count, env); }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  bool __stdcall GetParity(int n) { return child->GetParity(n); }
  void __stdcall SetCacheHints(int cachehints,int frame_range) { } ;  // We do not pass cache requests upwards, only to the next filter.
};


class AvisynthError /* exception */ {
public:
  const char* const msg;
  AvisynthError(const char* _msg) : msg(_msg) {}
};




/* Helper classes useful to plugin authors */

class AlignPlanar : public GenericVideoFilter 
{
public:
  AlignPlanar(PClip _clip);
  static PClip Create(PClip clip);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};



class FillBorder : public GenericVideoFilter 
{
public:
  FillBorder(PClip _clip);
  static PClip Create(PClip clip);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};



class ConvertAudio : public GenericVideoFilter 
/**
  * Helper class to convert audio to any format
 **/
{
public:
  ConvertAudio(PClip _clip, int prefered_format);
  void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env);

  static PClip Create(PClip clip, int sample_type, int prefered_type);
  static AVSValue __cdecl Create_float(AVSValue args, void*, IScriptEnvironment*);
  static AVSValue __cdecl Create_32bit(AVSValue args, void*, IScriptEnvironment*);
  static AVSValue __cdecl Create_16bit(AVSValue args, void*, IScriptEnvironment*);
  static AVSValue __cdecl Create_8bit(AVSValue args, void*, IScriptEnvironment*);
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

  virtual char* __stdcall SaveString(const char* s, int length = -1) = 0;
  virtual char* __stdcall Sprintf(const char* fmt, ...) = 0;
  // note: val is really a va_list; I hope everyone typedefs va_list to a pointer
  virtual char* __stdcall VSprintf(const char* fmt, void* val) = 0;

  __declspec(noreturn) virtual void __stdcall ThrowError(const char* fmt, ...) = 0;

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

  // align should be 4 or 8
  virtual PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, int align=FRAME_ALIGN) = 0;

  virtual bool __stdcall MakeWritable(PVideoFrame* pvf) = 0;

  static void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height);

  typedef void (__cdecl *ShutdownFunc)(void* user_data, IScriptEnvironment* env);
  virtual void __stdcall AtExit(ShutdownFunc function, void* user_data) = 0;

  virtual void __stdcall CheckVersion(int version = AVISYNTH_INTERFACE_VERSION) = 0;

  virtual PVideoFrame __stdcall Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height) = 0;

	virtual int __stdcall SetMemoryMax(int mem) = 0;

  virtual int __stdcall SetWorkingDir(const char * newdir) = 0;

};


// avisynth.dll exports this; it's a way to use it as a library, without
// writing an AVS script or without going through AVIFile.
IScriptEnvironment* __stdcall CreateScriptEnvironment(int version = AVISYNTH_INTERFACE_VERSION);


#pragma pack(pop)

#endif //__AVISYNTH_H__
