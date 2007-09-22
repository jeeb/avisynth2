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

// ConvertAudio classes
// Copyright (c) Klaus Post 2001 - 2004
// Copyright (c) Ian Brabham 2005


#include "soundout.h"

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif

#  pragma warning(disable: 4244 4311 4554 4311) 
// There are two type parameters. Acceptable sample types and a prefered sample type.
// If the current clip is already one of the defined types in sampletype, this will be returned.
// If not, the current clip will be converted to the prefered type.
PClip ConvertAudio::Create(PClip clip, int sample_type, int prefered_type) 
{
  if ((!clip->GetVideoInfo().HasAudio()) || clip->GetVideoInfo().SampleType()&(sample_type|prefered_type)) {  // Sample type is already ok!
    return clip;
  }
  else 
    return new ConvertAudio(clip,prefered_type);
}


void __stdcall ConvertAudio::SetCacheHints(int cachehints,int frame_range)
{   // We do pass cache requests upwards, to the next filter.
  child->SetCacheHints(cachehints, frame_range);
}


/*******************************************
 *******   Convert Audio -> Arbitrary ******
 ******************************************/

// Optme: Could be made onepass, but that would make it immensely complex
ConvertAudio::ConvertAudio(PClip _clip, int _sample_type) 
  : GenericVideoFilter(_clip)
{
  dst_format=_sample_type;
  src_format=vi.SampleType();
  // Set up convertion matrix
  src_bps=vi.BytesPerChannelSample();  // Store old size
  vi.sample_type=dst_format;
  tempbuffer_size=0;
}

  ConvertAudio::~ConvertAudio() {
    if (tempbuffer_size) {
      _aligned_free(tempbuffer); 
      _aligned_free(floatbuffer); 
      tempbuffer_size=0;
    }
  }

void __stdcall ConvertAudio::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) 
{
  int channels=vi.AudioChannels();
  if (tempbuffer_size) {
    if (tempbuffer_size<count) {
      _aligned_free(tempbuffer);
      _aligned_free(floatbuffer);
      tempbuffer = (char *) _aligned_malloc(count*src_bps*channels, 16);
      floatbuffer = (SFLOAT*) _aligned_malloc(count*channels*sizeof(SFLOAT), 16);
      tempbuffer_size=count;
    }
  } else {
    tempbuffer = (char *) _aligned_malloc(count*src_bps*channels, 16);
    floatbuffer = (SFLOAT*)_aligned_malloc(count*channels*sizeof(SFLOAT),16);
    tempbuffer_size=count;
  }

  child->GetAudio(tempbuffer, start, count, env);

  float* tmp_fb;
  if (dst_format == SAMPLE_FLOAT)  // Skip final copy, if samples are to be float
	tmp_fb = (float*)buf;
  else
	tmp_fb = floatbuffer;

  if (src_format != SAMPLE_FLOAT) {  // Skip initial copy, if samples are already float
// Someone with an AMD beast decide which code runs better SSE2 or 3DNow   :: FIXME
    if ((env->GetCPUFlags() & CPUF_3DNOW_EXT)) {
      convertToFloat_3DN(tempbuffer, tmp_fb, src_format, count*channels);
    } else if (((int)tmp_fb & 3 == 0) && (env->GetCPUFlags() & CPUF_SSE2)) {
      convertToFloat_SSE2(tempbuffer, tmp_fb, src_format, count*channels);
    } else if ((env->GetCPUFlags() & CPUF_SSE)) {
      convertToFloat_SSE(tempbuffer, tmp_fb, src_format, count*channels);
    } else {
      convertToFloat(tempbuffer, tmp_fb, src_format, count*channels);
    }
  } else {
    tmp_fb = (float*)tempbuffer;
  }

  if (dst_format != SAMPLE_FLOAT) {  // Skip final copy, if samples are to be float
// Someone with an AMD beast decide which code runs better SSE2 or 3DNow   :: FIXME
	if ((env->GetCPUFlags() & CPUF_3DNOW_EXT)) {
	  convertFromFloat_3DN(tmp_fb, buf, dst_format, count*channels);
	} else if ((env->GetCPUFlags() & CPUF_SSE2)) {
	  convertFromFloat_SSE2(tmp_fb, buf, dst_format, count*channels);
	} else if ((env->GetCPUFlags() & CPUF_SSE)) {
	  convertFromFloat_SSE(tmp_fb, buf, dst_format, count*channels);
	} else {
	  convertFromFloat(tmp_fb, buf, dst_format, count*channels);
	}
  }
}

/* SAMPLE_INT16 <-> SAMPLE_INT32

 * S32 = S16 << 16
 *
 * short *d=dest, *s=src;
 *
 *   *d++ = 0;
 *   *d++ = *s++;
 *
 * for (i=0; i< count*ch; i++) {
 *   d[i*2] = 0;
 *   d[i*2+1] = s[i];
 * }

 *   movq       mm0,[s]
 *    pxor      mm1,mm1
 *   pxor       mm2,mm2
 *    punpcklwd mm1,mm0
 *   d+=16
 *    punpckhwd mm2,mm0
 *   movq       [d-16],mm1
 *    s+=8
 *   movq       [d-8],mm2
 *   
 
 * S16 = (S32 + 0x8000) >> 16
 *
 * short *d=dest;
 * int   *s=src;
 * 
 * for (i=0; i< count*ch; i++) {
 *   d[i] = (s[i]+0x00008000) >> 16;
 * }

 *   movq    mm7,[0x0000800000008000]
 * 
 *   movq     mm0,[s]
 *    movq    mm1,[s+8]
 *   paddd    mm0,mm7
 *    paddd   mm1,mm7
 *   psrad    mm0,16
 *    psrad   mm1,16
 *   d+=8
 *    packsdw mm0,mm1
 *   s+=16
 *    movq    [d-8],mm0

*/

//================
// convertToFloat
//================

void ConvertAudio::convertToFloat(char* inbuf, float* outbuf, char sample_type, int count) {
  int i;
  switch (sample_type) {
    case SAMPLE_INT8: {
      unsigned char* samples = (unsigned char*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=((float)samples[i]-128.0f) / 128.0f;
      break;
      }
    case SAMPLE_INT16: {
      signed short* samples = (signed short*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=(float)samples[i] / 32768.0f;
      break;
      }

    case SAMPLE_INT32: {
      signed int* samples = (signed int*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=(float)samples[i] / (float)(MAX_INT);
      break;     
    }
    case SAMPLE_FLOAT: {
      SFLOAT* samples = (SFLOAT*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=samples[i];
      break;     
    }
    case SAMPLE_INT24: {
      unsigned char* samples = (unsigned char*)inbuf;
      for (i=0;i<count;i++) {
        signed int tval = (samples[i*3]<<8) | (samples[i*3+1] << 16) | (samples[i*3+2] << 24); 
        outbuf[i] = (float)(tval / 256) / (float)(1<<23);
      }
      break;
    }
    default: { 
      for (i=0;i<count;i++) 
        outbuf[i]=0.0f;
      break;     
    }
  }
}

void ConvertAudio::convertToFloat_SSE(char* inbuf, float* outbuf, char sample_type, int count) {
  int i;
  switch (sample_type) {
    case SAMPLE_INT8: {
      unsigned char* samples = (unsigned char*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=((float)samples[i]-128.0f) / 128.0f;
      break;
      }
    case SAMPLE_INT16: {
      const float divisor = 1.0 / 32768.0;
      signed short* samples = (signed short*)inbuf;
      int c_miss = count & 3;
      int c_loop = (count - c_miss);  // Number of samples.

      if (c_loop) {        
        __asm {
          xor       eax, eax                  // count
          mov       edx, [c_loop]
          mov       esi, [samples];
          shl       edx, 1                    // Number of input bytes (*2)
          movss     xmm7, [divisor]
          mov       edi, [outbuf];
          shufps    xmm7, xmm7, 00000000b
          align     16
c16_loop:
          movq      mm1, [esi+eax]            //  d c | b a
          add       eax,8
          punpcklwd mm0, mm1                  //  b x | a x
          punpckhwd mm1, mm1                  //  d d | c c
          psrad     mm0, 16                   //  sign extend
          psrad     mm1, 16                   //  sign extend
          cvtpi2ps  xmm0, mm0                 //  - b | - a -> float
          cvtpi2ps  xmm1, mm1                 //  - d | - c -> float
		  movlhps   xmm0, xmm1                //  xd  xc || xb  xa
          mulps     xmm0, xmm7                //  *=1/MAX_SHORT
          cmp       eax, edx
          movups    [edi+eax*2-16], xmm0      //  store xd | xc | xb | xa
          jne       c16_loop
          emms
        }
      }
      for (i=0; i<c_miss; i++) {
        outbuf[i+c_loop]=(float)samples[i+c_loop] * divisor;
      }
      break;
    }
    case SAMPLE_INT32: {
      const float divisor = float(1.0 / MAX_INT);
      signed int* samples = (signed int*)inbuf;
      int c_miss = count & 3;
      int c_loop = count-c_miss; // in samples

      if (c_loop) {        
        __asm {
          xor      eax, eax               // count
          mov      edx, [c_loop]
          mov      esi, [samples];
          shl      edx, 2                 // in input bytes (*4)
          movss    xmm7, [divisor]
          mov      edi, [outbuf];
          shufps   xmm7, xmm7, 00000000b
          align 16
c32_loop:
          movq     mm0, [esi+eax]        //  b b | a a
          movq     mm1, [esi+eax+8]      //  d d | c c
          cvtpi2ps xmm0, mm0             //  b b | a a -> float
          cvtpi2ps xmm1, mm1             //  d d | c c -> float
		  movlhps  xmm0, xmm1            //  xd  xc || xb  xa
          add      eax, 16
          mulps    xmm0, xmm7             //  *=1/MAX_INT
          cmp      eax, edx
          movups   [edi+eax-16], xmm0     //  store xd | xc | xb | xa
          jne      c32_loop
          emms
        }
      }
      for (i=0; i<c_miss; i++) {
        outbuf[i+c_loop]=(float)samples[i+c_loop] * divisor;
      }
      break;
    }

    case SAMPLE_FLOAT: {
      SFLOAT* samples = (SFLOAT*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=samples[i];
      break;     
    }

    case SAMPLE_INT24: {
      unsigned char* samples = (unsigned char*)inbuf;
      for (i=0;i<count;i++) {
        signed int tval = (samples[i*3]<<8) | (samples[i*3+1] << 16) | (samples[i*3+2] << 24); 
        outbuf[i] = (float)(tval / 256) / (float)(1<<23);
      }
      break;
    }

    default: { 
      for (i=0;i<count;i++) 
        outbuf[i]=0.0f;
      break;     
    }
  }
}


void ConvertAudio::convertToFloat_SSE2(char* inbuf, float* outbuf, char sample_type, int count) {
  int i;
  switch (sample_type) {
    case SAMPLE_INT8: {
      unsigned char* samples = (unsigned char*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=((float)samples[i]-128.0f) / 128.0f;
      break;
      }
    case SAMPLE_INT16: {
      const float divisor = 1.0 / 32768.0;
      signed short* samples = (signed short*)inbuf;

      while (((int)outbuf & 15) && count) { // dqword align outbuf
        *outbuf++ = divisor * (float)*samples++;
		count-=1;
      }
	  
      int c_miss = count & 7;
      int c_loop = (count - c_miss);  // Number of samples.

      if (c_loop) {        
        __asm {
          xor       eax, eax                  // count
          mov       edx, [c_loop]
          mov       esi, [samples];
          shl       edx, 1                    // Number of input bytes (*2)
          movss     xmm7, [divisor]
          mov       edi, [outbuf];
          shufps    xmm7, xmm7, 00000000b
          align     16
c16_loop:
          movdqu    xmm1, [esi+eax]           //  h g | f e | d c | b a
          add       eax,16
          punpcklwd xmm0, xmm1                //  d x | c x | b x | a x
          punpckhwd xmm1, xmm1                //  h h | g g | f f | e e
          psrad     xmm0, 16                  //  sign extend
          psrad     xmm1, 16                  //  sign extend
          cvtdq2ps  xmm2, xmm0                //  - d | - c | - b | - a -> float
          cvtdq2ps  xmm3, xmm1                //  - h | - g | - f | - e -> float
          mulps     xmm2, xmm7                //  *=1/MAX_SHORT
          mulps     xmm3, xmm7                //  *=1/MAX_SHORT
          movaps    [edi+eax*2-32], xmm2      //  store xd | xc | xb | xa
          cmp       eax, edx
          movaps    [edi+eax*2-16], xmm3      //  store xh | xg | xf | xe
          jne       c16_loop
          emms
        }
      }
      for (i=0; i<c_miss; i++) {
        outbuf[i+c_loop]=(float)samples[i+c_loop] * divisor;
      }
      break;
    }
    case SAMPLE_INT32: {
      const float divisor = float(1.0 / MAX_INT);
      signed int* samples = (signed int*)inbuf;

      while (((int)outbuf & 15) && count) { // dqword align outbuf
        *outbuf++ = divisor * (float)*samples++;
		count-=1;
      }
	  
      int c_miss = count & 7;
      int c_loop = count-c_miss; // in samples

      if (c_loop) {        
        __asm {
          xor      eax, eax               // count
          mov      edx, [c_loop]
          mov      esi, [samples];
          shl      edx, 2                 // in input bytes (*4)
          movss    xmm7, [divisor]
          mov      edi, [outbuf];
          shufps   xmm7, xmm7, 00000000b
          align 16
c32_loop:
          movdqu   xmm0, [esi+eax]        //  dd | cc | bb | aa
          movdqu   xmm1, [esi+eax+16]     //  hh | gg | ff | ee
          cvtdq2ps xmm2, xmm0             //  xd | xc | xb | xa
          cvtdq2ps xmm3, xmm1             //  xh | xg | xf | xe
          mulps    xmm2, xmm7             //  *=1/MAX_INT
          add      eax,32
          mulps    xmm3, xmm7             //  *=1/MAX_INT
          movaps   [edi+eax-32], xmm2     //  store xd | xc | xb | xa
          cmp      eax, edx
          movaps   [edi+eax-16], xmm3     //  store xh | xg | xf | xe
          jne      c32_loop
          emms
        }
      }
      for (i=0; i<c_miss; i++) {
        outbuf[i+c_loop]=(float)samples[i+c_loop] * divisor;
      }
      break;
    }

    case SAMPLE_FLOAT: {
      SFLOAT* samples = (SFLOAT*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=samples[i];
      break;     
    }

    case SAMPLE_INT24: {
      unsigned char* samples = (unsigned char*)inbuf;
      for (i=0;i<count;i++) {
        signed int tval = (samples[i*3]<<8) | (samples[i*3+1] << 16) | (samples[i*3+2] << 24); 
        outbuf[i] = (float)(tval / 256) / (float)(1<<23);
      }
      break;
    }

    default: { 
      for (i=0;i<count;i++) 
        outbuf[i]=0.0f;
      break;     
    }
  }
}


void ConvertAudio::convertToFloat_3DN(char* inbuf, float* outbuf, char sample_type, int count) {
  int i;
  switch (sample_type) {
    case SAMPLE_INT8: {
      unsigned char* samples = (unsigned char*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=((float)samples[i]-128.0f) / 128.0f;
      break;
      }
    case SAMPLE_INT16: {
      float divisor = 1.0 / 32768.0;
      signed short* samples = (signed short*)inbuf;
      int c_miss = count & 3;
      int c_loop = (count - c_miss);  // Number of samples.
      if (c_loop) {        
        __asm {
          xor eax,eax                   // count
          mov edx, [c_loop]
          shl edx, 1  // Number of input bytes.
          mov esi, [samples];
          mov edi, [outbuf];
          movd mm7,[divisor]
          pshufw mm7,mm7, 01000100b
          pxor mm6,mm6
          align 16
c16_loop:
          movq mm0, [esi+eax]          //  d c | b a
          movq mm1, mm0
          punpcklwd mm0, mm6             //  b b | a a
          punpckhwd mm1, mm6             //  d d | c c
          pi2fw mm0, mm0                 //  xb=float(b) | xa=float(a)
          pi2fw mm1, mm1                 //  xb=float(d) | xa=float(c)
          pfmul mm0,mm7                  // x / 32768.0
          pfmul mm1,mm7                  // x / 32768.0
          movq [edi+eax*2], mm0          //  store xb | xa
          movq [edi+eax*2+8], mm1        //  store xd | xc
          add eax,8
          cmp eax, edx
          jne c16_loop
          emms
        }
      }
      for (i=0; i<c_miss; i++) {
        outbuf[i+c_loop]=(float)samples[i+c_loop] * divisor;
      }
      break;
    }
    case SAMPLE_INT32: {
      float divisor = (float)1.0 / (float)MAX_INT;
      signed int* samples = (signed int*)inbuf;
      int c_miss = count & 3;
      int c_loop = count-c_miss; // in samples
      if (c_loop) {        
        __asm {
          xor eax,eax                   // count
          mov edx, [c_loop]
          shl edx, 2                     // in input bytes (*4)
          mov esi, [samples];
          mov edi, [outbuf];
          movd mm7,[divisor]
            pshufw mm7,mm7, 01000100b
            align 16
c32_loop:
          movq mm1, [esi+eax]            //  b b | a a
          movq mm2, [esi+eax+8]          //  d d | c c
          pi2fd mm1, mm1                 //  xb=float(b) | xa=float(a)
          pi2fd mm2, mm2                 //  xb=float(d) | xa=float(c)
          pfmul mm1,mm7                  // x / MaxInt.0
          pfmul mm2,mm7                  // x / MaxInt.0
          movq [edi+eax], mm1            //  store xb | xa
          movq [edi+eax+8], mm2          //  store xd | xc
          add eax,16
          cmp eax, edx
          jne c32_loop
          emms
        }
      }
      for (i=0; i<c_miss; i++) {
        outbuf[i+c_loop]=(float)samples[i+c_loop] * divisor;
      }
      break;
    }

    case SAMPLE_FLOAT: {
      SFLOAT* samples = (SFLOAT*)inbuf;
      for (i=0;i<count;i++) 
        outbuf[i]=samples[i];
      break;     
    }

    case SAMPLE_INT24: {
      unsigned char* samples = (unsigned char*)inbuf;
      for (i=0;i<count;i++) {
        signed int tval = (samples[i*3]<<8) | (samples[i*3+1] << 16) | (samples[i*3+2] << 24); 
        outbuf[i] = (float)(tval / 256) / (float)(1<<23);
      }
      break;
    }

    default: { 
      for (i=0;i<count;i++) 
        outbuf[i]=0.0f;
      break;     
    }
  }
}

//==================
// convertFromFloat
//==================

void ConvertAudio::convertFromFloat_3DN(float* inbuf,void* outbuf, char sample_type, int count) {
  int i;
  switch (sample_type) {
    case SAMPLE_INT8: {
      unsigned char* samples = (unsigned char*)outbuf;
      for (i=0;i<count;i++) 
        samples[i]=(unsigned char)Saturate_int8(inbuf[i] * 128.0f)+128;
      break;
      }
    case SAMPLE_INT16: {
      float multiplier = 32768.0;
      signed short* samples = (signed short*)outbuf;
      int c_miss = count & 3;
      int c_loop = count-c_miss; // in samples
      if (c_loop) {        
        __asm {
          xor eax,eax                   // count
            mov edx, [c_loop]
            shl edx, 1                     // in output bytes (*2)
            mov esi, [inbuf];
          mov edi, [outbuf];
          movd mm7,[multiplier]
            pshufw mm7,mm7, 01000100b
            align 16
c16f_loop:
          movq mm1, [esi+eax*2]            //  b b | a a
            movq mm2, [esi+eax*2+8]          //  d d | c c
            pfmul mm1,mm7                  // x * 32 bit
            pfmul mm2,mm7                  // x * 32 bit

// These may work better with the packssdw on a K6
//          pf2id mm1, mm1                 //  xb=int(b) | xa=int(a)
//          pf2id mm2, mm2                 //  xb=int(d) | xa=int(c)

// K6 etc may get this wrong - wait for test results
            pf2iw mm1, mm1                 //  xb=int(b) | xa=int(a)
            pf2iw mm2, mm2                 //  xb=int(d) | xa=int(c)
            packssdw mm1,mm2
            movq [edi+eax], mm1            //  store xb | xa
            add eax,8
            cmp eax, edx
            jne c16f_loop
            emms
        }
      }
      for (i=0; i<c_miss; i++) {
        samples[i+c_loop]=Saturate_int16(inbuf[i+c_loop] * 32768.0f);
      }
      break;
    }

    case SAMPLE_INT32: {
      float multiplier = (float)MAX_INT;
      signed int* samples = (signed int*)outbuf;
      int c_miss = count & 3;
      int c_loop = count-c_miss; // in samples
      if (c_loop) {        
        __asm {
          xor eax,eax                   // count
            mov edx, [c_loop]
            shl edx, 2                     // in output bytes (*4)
            mov esi, [inbuf];
          mov edi, [outbuf];
          movd mm7,[multiplier]
            pshufw mm7,mm7, 01000100b
            align 16
c32f_loop:
          movq mm1, [esi+eax]            //  b b | a a
            movq mm2, [esi+eax+8]          //  d d | c c
            pfmul mm1,mm7                  // x * 32 bit
            pfmul mm2,mm7                  // x * 32 bit
            pf2id mm1, mm1                 //  xb=int(b) | xa=int(a)
            pf2id mm2, mm2                 //  xb=int(d) | xa=int(c)
            movq [edi+eax], mm1            //  store xb | xa
            movq [edi+eax+8], mm2          //  store xd | xc
            add eax,16
            cmp eax, edx
            jne c32f_loop
            emms
        }
      }
      for (i=0; i<c_miss; i++) {
        samples[i+c_loop]=Saturate_int32(inbuf[i+c_loop] * (float)(MAX_INT));
      }
      break;
    }
    case SAMPLE_INT24: {
      float multiplier = (float)(1<<23);
      float maxF = (float)((1<<23) - 1);
      float minF = -(float)((1<<23) -1);
      unsigned char* samples = (unsigned char*)outbuf;
      int c_loop = (count-8) - ((count-8)&3); // in samples
      int c_miss = count-c_loop;
      if (c_loop) {        
        __asm {
          xor eax,eax                   // input offset count
          xor ecx,ecx                   // dest offset count
          mov edx, [c_loop]
          shl edx, 2                     // in input bytes (*4)
          movd mm7,[multiplier]
          mov esi, [inbuf];
          mov edi, [outbuf];
          movd mm6,[maxF]               // Maximum value
          pshufw mm7,mm7, 01000100b
          movd mm5,[minF]               // Minimum value
          pshufw mm6,mm6, 01000100b
          pshufw mm5,mm5, 01000100b
          align 16
c24f_loop:
          movq mm1, [esi+eax]            //  b b | a a
            movq mm2, [esi+eax+8]          //  d d | c c
            pfmul mm1,mm7                  // x * 24 bit
            pfmul mm2,mm7                  // x * 24 bit
            pfmax mm1,mm5
            pfmax mm2,mm5
            pfmin mm1,mm6
            pfmin mm2,mm6
            pf2id mm1, mm1                 //  xb=int(b) | xa=int(a)
            pf2id mm2, mm2                 //  xb=int(d) | xa=int(c)            
            pshufw mm3,mm1,11101110b
            pshufw mm4,mm1,11101110b
            movd [edi+ecx], mm1            //  store xb | xa
            movd [edi+ecx+3], mm3            //  store xb | xa
            movd [edi+ecx+6], mm2          //  store xd | xc
            movd [edi+ecx+9], mm4          //  store xd | xc
            add eax,16
            add ecx,12
            cmp eax, edx
            jne c24f_loop
            emms
        }
      }
      for (i=0;i<c_miss;i++) {
        signed int tval = Saturate_int24(inbuf[i+c_loop] * (float)(1<<23));
        samples[(i+c_loop)*3] = tval & 0xff;
        samples[((i+c_loop)*3)+1] = (tval & 0xff00)>>8;
        samples[((i+c_loop)*3)+2] = (tval & 0xff0000)>>16;
      }
      break;
    }
    case SAMPLE_FLOAT: {
      SFLOAT* samples = (SFLOAT*)outbuf;      
      for (i=0;i<count;i++) {
        samples[i]=inbuf[i];
      }
      break;     
    }
    default: { 
    }
  }
}

void ConvertAudio::convertFromFloat(float* inbuf,void* outbuf, char sample_type, int count) {
  int i;
  switch (sample_type) {
    case SAMPLE_INT8: {
      unsigned char* samples = (unsigned char*)outbuf;
      for (i=0;i<count;i++) 
        samples[i]=(unsigned char)Saturate_int8(inbuf[i] * 128.0f)+128;
      break;
      }
    case SAMPLE_INT16: {
      signed short* samples = (signed short*)outbuf;
      for (i=0;i<count;i++) {
        samples[i]=Saturate_int16(inbuf[i] * 32768.0f);
      }
      break;
      }

    case SAMPLE_INT32: {
      signed int* samples = (signed int*)outbuf;
      for (i=0;i<count;i++) 
        samples[i]= Saturate_int32(inbuf[i] * (float)(MAX_INT));
      break;     
    }
    case SAMPLE_INT24: {
      unsigned char* samples = (unsigned char*)outbuf;
      for (i=0;i<count;i++) {
        signed int tval = Saturate_int24(inbuf[i] * (float)(1<<23));
        samples[i*3] = tval & 0xff;
        samples[i*3+1] = (tval & 0xff00)>>8;
        samples[i*3+2] = (tval & 0xff0000)>>16;
      }
      break;
    }
    case SAMPLE_FLOAT: {
      SFLOAT* samples = (SFLOAT*)outbuf;      
      for (i=0;i<count;i++) {
        samples[i]=inbuf[i];
      }
      break;     
    }
    default: { 
    }
  }
}

void ConvertAudio::convertFromFloat_SSE(float* inbuf,void* outbuf, char sample_type, int count) {
  int i;

  switch (sample_type) {
    case SAMPLE_INT8: {
      unsigned char* samples = (unsigned char*)outbuf;
      for (i=0;i<count;i++) 
        samples[i]=(unsigned char)Saturate_int8(inbuf[i] * 128.0f)+128;
      break;
      }
    case SAMPLE_INT16: {
      signed short* samples = (signed short*)outbuf;
      int sleft = count & 3;
      count -= sleft;

      float mult[4];
      mult[0]=mult[1]=mult[2]=mult[3] = 32768.0;

      if (count) {        
		_asm {
		  movups   xmm7, [mult]
		  mov      eax, inbuf
		  mov      ecx, count
		  xor      edx, edx
		  shl      ecx, 1
		  mov      edi, outbuf
		  align    16
cf16_loop:
		  movups   xmm0, [eax+edx*2]           // xd | xc | xb | xa         
		  mulps    xmm0, xmm7                  // *= MAX_SHORT
          minps    xmm0, xmm7                  // x=min(x, MAX_SHORT)  --  +ve Signed Saturation > 2^31
		  movhlps  xmm1, xmm0                  // xx | xx | xd | xc
		  cvtps2pi mm0, xmm0                   // float -> bb | aa
		  cvtps2pi mm1, xmm1                   // float -> dd | cc
		  add      edx,8
		  packssdw mm0, mm1                    // d | c | b | a  --  +/-ve Signed Saturation > 2^15
		  cmp      ecx, edx
		  movq     [edi+edx-8], mm0            // store d | c | b | a
		  jne      cf16_loop
		  emms
		}
      }
      for (i=0;i<sleft;i++) {
        samples[count+i]=Saturate_int16(inbuf[count+i] * 32768.0f);
      }

      break;
    }

    case SAMPLE_INT32: {
      signed int* samples = (signed int*)outbuf;
      int sleft = count & 3;
      count -= sleft;

      float mult  = (float)(MAX_INT);  // (2^23)<<8  
//      float limit = 2147483520.0f;     // (2^24-1)<<7

      if (count) {
		int temp[7];
        _asm {
		  lea      esi, [temp]
          movss    xmm7, [mult]
//          movss    xmm6, [limit]
		  add      esi, 15
          shufps   xmm7, xmm7, 00000000b
		  and      esi, -16                    // dqword align
//          shufps   xmm6, xmm6, 00000000b

          mov      eax, inbuf
          mov      ecx, count
          xor      edx, edx
          shl      ecx, 2
          mov      edi, outbuf
          align    16
cf32_loop:
          movups   xmm0, [eax+edx]             // xd | xc | xb | xa
          mulps    xmm0, xmm7                  // *= MAX_INT
#if 1
          movhlps  xmm1, xmm0                  // xx | xx | xd | xc
          cvtps2pi mm0, xmm0                   // float -> bb | aa  --  -ve Signed Saturation
		  cmpnltps xmm0, xmm7                  // !(xd | xc | xb | xa < MAX_INT)
          cvtps2pi mm1, xmm1                   // float -> dd | cc  --  -ve Signed Saturation
          movdqa   [esi], xmm0                 // md | mc | mb | ma                          -- YUCK!!!
          add      edx,16
		  pxor     mm0, [esi]                  // 0x80000000 -> 0x7FFFFFFF if +ve saturation
		  pxor     mm1, [esi+8]
#else
          minps    xmm0, xmm6                  // x=min(x, Limit)  --  +ve Signed Saturation -- LAME!!!
          add      edx,16
          movhlps  xmm1, xmm0                  // xx | xx | xd | xc
          cvtps2pi mm0, xmm0                   // float -> bb | aa  --  -ve Signed Saturation
          cvtps2pi mm1, xmm1                   // float -> dd | cc  --  -ve Signed Saturation
#endif
          movq     [edi+edx-16], mm0           // store bb | aa
          cmp      ecx, edx
          movq     [edi+edx-8], mm1            // store dd | cc
          jne      cf32_loop
          emms
        }
      }
      for (i=0;i<sleft;i++) {
        samples[count+i]=Saturate_int32(inbuf[count+i] * (float)(MAX_INT));
      }

      break;
    }
    case SAMPLE_INT24: {
      unsigned char* samples = (unsigned char*)outbuf;
      for (i=0;i<count;i++) {
        signed int tval = Saturate_int24(inbuf[i] * (float)(1<<23));
        samples[i*3] = tval & 0xff;
        samples[i*3+1] = (tval & 0xff00)>>8;
        samples[i*3+2] = (tval & 0xff0000)>>16;
      }
      break;
    }
    case SAMPLE_FLOAT: {
      SFLOAT* samples = (SFLOAT*)outbuf;      
      for (i=0;i<count;i++) {
        samples[i]=inbuf[i];
      }
      break;     
    }
    default: { 
    }
  }
}

void ConvertAudio::convertFromFloat_SSE2(float* inbuf,void* outbuf, char sample_type, int count) {
  int i;

  switch (sample_type) {
    case SAMPLE_INT8: {
      unsigned char* samples = (unsigned char*)outbuf;
      for (i=0;i<count;i++) 
        samples[i]=(unsigned char)Saturate_int8(inbuf[i] * 128.0f)+128;
      break;
      }
    case SAMPLE_INT16: {
      signed short* samples = (signed short*)outbuf;

      if ((int)samples & 1) {  // could never dqword align outbuf
        convertFromFloat_SSE(inbuf, outbuf, sample_type, count);
        break;
      }

      while (((int)samples & 15) && count) { // dqword align outbuf
        *samples++ = Saturate_int16(*inbuf++ * 32768.0f);
        count-=1;
      }

      int sleft = count & 7;
      count -= sleft;

      float mult[4];
      mult[0]=mult[1]=mult[2]=mult[3] = 32768.0;

      if (count) {
		_asm {
		  movups   xmm7, [mult]
		  mov      eax, inbuf
		  mov      ecx, count
		  xor      edx, edx
		  shl      ecx, 1
		  mov      edi, samples
		  align    16
cf16_loop:
		  movups   xmm0, [eax+edx*2]           // xd | xc | xb | xa         
		  movups   xmm1, [eax+edx*2+16]        // xh | xg | xf | xe         
		  mulps    xmm0, xmm7                  // *= MAX_SHORT
		  mulps    xmm1, xmm7                  // *= MAX_SHORT
		  minps    xmm0, xmm7                  // x=min(x, MAX_SHORT)  --  +ve Signed Saturation > 2^31
		  minps    xmm1, xmm7                  // x=min(x, MAX_SHORT)  --  +ve Signed Saturation > 2^31
		  cvtps2dq xmm2, xmm0                  // float -> dd | cc | bb | aa
		  cvtps2dq xmm3, xmm1                  // float -> hh | gg | ff | ee
		  add      edx,16
		  packssdw xmm2, xmm3                  // h g | f e | d c | b a  --  +/-ve Signed Saturation > 2^15
		  cmp      ecx, edx
		  movdqa   [edi+edx-16], xmm2          // store h g | f e | d c | b a
		  jne      cf16_loop
		  emms
		}
      }
      for (i=0;i<sleft;i++) {
        samples[count+i]=Saturate_int16(inbuf[count+i] * 32768.0f);
      }

      break;
    }

    case SAMPLE_INT32: {
      signed int* samples = (signed int*)outbuf;

      if ((int)samples & 3) {  // could never dqword align outbuf
        convertFromFloat_SSE(inbuf, outbuf, sample_type, count);
        break;
      }

      while (((int)samples & 15) && count) { // dqword align outbuf
        *samples++=Saturate_int32(*inbuf++ * (float)(MAX_INT));
        count-=1;
      }
      int sleft = count & 7;
      count -= sleft;

      float mult  = (float)(MAX_INT);  // (2^23)<<8
//      float limit = 2147483520.0f;     // (2^24-1)<<7

      if (count) {
        _asm {
          movss    xmm7, [mult]
//          movss    xmm6, [limit]
          shufps   xmm7, xmm7, 00000000b
//          shufps   xmm6, xmm6, 00000000b

          mov      eax, inbuf
          mov      ecx, count
          xor      edx, edx
          shl      ecx, 2
          mov      edi, samples
          align    16
cf32_loop:
		  movups   xmm0, [eax+edx]             // xd | xc | xb | xa         
		  movups   xmm1, [eax+edx+16]          // xh | xg | xf | xe         
          mulps    xmm0, xmm7                  // *= MAX_INT
          mulps    xmm1, xmm7                  // *= MAX_INT
#if 1
// Bloody Intel and their "indefinite integer value" it
// is no use to man or beast, we need proper saturation
// like AMD does with their 3DNow instructions. Grrr!!!
		  cvtps2dq xmm2, xmm0                  // float -> dd | cc | bb | aa  --  -ve Signed Saturation
		  cvtps2dq xmm3, xmm1                  // float -> hh | gg | ff | ee  --  -ve Signed Saturation
		  cmpnltps xmm0, xmm7                  // !(xd | xc | xb | xa < MAX_INT)
		  cmpnltps xmm1, xmm7                  // !(xh | xg | xf | xe < MAX_INT)
          add      edx,32
		  pxor     xmm2, xmm0                  // 0x80000000 -> 0x7FFFFFFF if +ve saturation
		  pxor     xmm3, xmm1
#else
		  minps    xmm0, xmm6                  // x=min(x, Limit)  --  +ve Signed Saturation  --  LAME!!!
		  minps    xmm1, xmm6                  // x=min(x, Limit)  --  +ve Signed Saturation
		  cvtps2dq xmm2, xmm0                  // float -> dd | cc | bb | aa  --  -ve Signed Saturation
		  add      edx,32
		  cvtps2dq xmm3, xmm1                  // float -> hh | gg | ff | ee  --  -ve Signed Saturation
#endif
          movdqa   [edi+edx-32], xmm2          // store dd | cc | bb | aa
          cmp      ecx, edx
          movdqa   [edi+edx-16], xmm3          // store hh | gg | ff | ee
          jne      cf32_loop
          emms
        }
      }
      for (i=0;i<sleft;i++) {
        samples[count+i]=Saturate_int32(inbuf[count+i] * (float)(MAX_INT));
      }

      break;
    }
    case SAMPLE_INT24: {
      unsigned char* samples = (unsigned char*)outbuf;
      for (i=0;i<count;i++) {
        signed int tval = Saturate_int24(inbuf[i] * (float)(1<<23));
        samples[i*3] = tval & 0xff;
        samples[i*3+1] = (tval & 0xff00)>>8;
        samples[i*3+2] = (tval & 0xff0000)>>16;
      }
      break;
    }
    case SAMPLE_FLOAT: {
      SFLOAT* samples = (SFLOAT*)outbuf;      
      for (i=0;i<count;i++) {
        samples[i]=inbuf[i];
      }
      break;     
    }
    default: { 
    }
  }
}

__inline int ConvertAudio::Saturate_int8(float n) {
    if (n <= -128.0f) return -128;
    if (n >= 127.0f) return 127;
    return (int)(n+0.5f);
}


__inline short ConvertAudio::Saturate_int16(float n) {
    if (n <= -32768.0f) return -32768;
    if (n >= 32767.0f) return 32767;
    return (short)(n+0.5f);
}

__inline int ConvertAudio::Saturate_int24(float n) {
    if (n <= (float)-(1<<23)) return -(1<<23);
    if (n >= (float)((1<<23)-1)) return ((1<<23)-1);
    return (int)(n+0.5f);
}

__inline int ConvertAudio::Saturate_int32(float n) {
    if (n <= (float)MIN_INT) return MIN_INT;  
    if (n >= (float)MAX_INT) return MAX_INT;
    return (int)(n)+0.5f;
}


