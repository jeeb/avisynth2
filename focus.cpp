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


#include "focus.h"




 
/********************************************************************
***** Declare index of new filters for Avisynth's filter engine *****
********************************************************************/

AVSFunction Focus_filters[] = {
  { "Blur", "cf[]f", Create_Blur },                     // amount [0 - 1]
  { "Sharpen", "cf[]f", Create_Sharpen },               // amount [0 - 1]
  { "TemporalSoften", "ciii", TemporalSoften::Create }, // radius, luma_threshold, chroma_threshold
  { "SpatialSoften", "ciii", SpatialSoften::Create },   // radius, luma_threshold, chroma_threshold
  { 0 }
};





/****************************************
 ***  AdjustFocus helper classes     ***
 ***  Originally by Ben R.G.         ***
 ***  MMX code by Marc FD            ***
 ***  Adaptation and bugfixes sh0dan ***
 ***  Code actually requires ISSE!   ***
 ***************************************/

AdjustFocusV::AdjustFocusV(double _amount, PClip _child)
: GenericVideoFilter(FillBorder::Create(_child)), amount(int(32768*pow(2.0, _amount)+0.5)) , line(NULL) {}

AdjustFocusV::~AdjustFocusV(void) 
{ 
  delete[] line; 
}

PVideoFrame __stdcall AdjustFocusV::GetFrame(int n, IScriptEnvironment* env) 
{
	PVideoFrame frame = child->GetFrame(n, env);
	env->MakeWritable(&frame);
	if (!line)
		line = new uc[frame->GetRowSize()];

	if (vi.IsYV12()) {
    int plane,cplane;
		for(cplane=0;cplane<3;cplane++) {
      if (cplane==0)  plane = PLANAR_Y;
      if (cplane==1)  plane = PLANAR_U;
      if (cplane==2)  plane = PLANAR_V;
			uc* buf = frame->GetWritePtr(plane);
			int pitch = frame->GetPitch(plane);
			int row_size = frame->GetRowSize(plane|PLANAR_ALIGNED);
			int height = frame->GetHeight(plane)-2;
			memcpy(line, buf, row_size);
			uc* p = buf + pitch;
			if (env->GetCPUFlags() & CPUF_INTEGER_SSE) {
				AFV_MMX(line,p,height,pitch,row_size,amount);
			} else {
				AFV_C(line,p,height,pitch,row_size,amount);
			}
		}

	} else {
		if (!line)
			line = new uc[frame->GetRowSize()*2];
		uc* buf = frame->GetWritePtr();
		int pitch = frame->GetPitch();
		int row_size = vi.RowSize();
		int height = vi.height-2;
		memcpy(line, buf, row_size);
		uc* p = buf + pitch;
		if (env->GetCPUFlags() & CPUF_INTEGER_SSE) {
			AFV_MMX(line,p,height,pitch,row_size,amount);
		} else {
			AFV_C(line,p,height,pitch,row_size,amount);
		}
	}

  return frame;
}

void AFV_C(uc* l, uc* p, const int height, const int pitch, const int row_size, const int amount) {
	const int center_weight = amount*2;
	const int outer_weight = 32768-amount;
	for (int y = height-2; y>0; --y) {
		for (int x = 0; x < row_size; ++x) {
			uc a = ScaledPixelClip(p[x] * center_weight + (l[x] + p[x+pitch]) * outer_weight);
			l[x] = p[x];
			p[x] = a;
		}
		p += pitch;
	}
}

void AFV_MMX(const uc* l, const uc* p, const int height, const int pitch, const int row_size, const int amount) {
	__declspec(align(8)) static __int64 cw;
	__asm { 
		// signed word center weight ((amount+0x100)>>9) x4
		mov		eax,amount
		add		eax,100h
		sar		eax,9
		movd	mm1,eax
		pshufw	mm2,mm1,0
		movq	cw,mm2
	}
	__declspec(align(8)) static __int64 ow;
	__asm {
		// signed word outer weight ((32768-amount+0x100)>>9) x4
		mov		eax,8000h
		sub		eax,amount
		add		eax,100h
		sar		eax,9
		movd	mm1,eax
		pshufw	mm2,mm1,0
		movq	ow,mm2
	}
	// round masks
	__declspec(align(8)) const static __int64 r6 = 0x0020002000200020;
	__declspec(align(8)) const static __int64 r7 = 0x0040004000400040;

	for (int y=0;y<height;y++) {

	__asm {
		mov			eax,l
		mov			ebx,p
		mov			ecx,pitch
		mov			edi,row_size
		shr			edi,3
		pxor		mm0,mm0

row_loop:

		movq		mm2,[ebx]
		movq		mm1,[eax]
		movq		[eax],mm2

		movq		mm3,[ebx+ecx]

		movq		  mm4,mm2
		 movq		  mm5,mm1
		punpcklbw	mm4,mm0
		 punpcklbw	mm5,mm0
		movq		  mm6,mm3
		 pmullw		mm4,cw
		punpcklbw	mm6,mm0
		 movq		  mm7,mm4
		paddsw		mm5,mm6
 		 paddusw		mm7,r6
		pmullw		mm5,ow
		 psraw		  mm7,6
		paddusw		mm5,r7
		 movq		  mm4,mm2
		psraw		  mm5,7
		 punpckhbw	mm4,mm0
		paddsw		mm7,mm5

		pmullw		mm4,cw
		 movq		  mm5,mm1
		movq		  mm6,mm3
		 punpckhbw	mm5,mm0
		punpckhbw	mm6,mm0
		paddsw		mm5,mm6
		pmullw		mm5,ow
		movq		  mm6,mm4
		 paddusw		mm5,r7
		paddusw		mm6,r6
		 psraw		  mm5,7
		psraw		  mm6,6
		paddsw		mm6,mm5

		packuswb	mm7,mm6
		movq		[ebx],mm7

		add			eax,8
		add			ebx,8
		dec			edi
		cmp			edi,0
		jnle		row_loop
		}
		p += pitch;
	}
	__asm emms
}

AdjustFocusH::AdjustFocusH(double _amount, PClip _child)
: GenericVideoFilter(FillBorder::Create(_child)), amount(int(32768*pow(2.0, _amount)+0.5)) {}

PVideoFrame __stdcall AdjustFocusH::GetFrame(int n, IScriptEnvironment* env) 
{
	PVideoFrame frame = child->GetFrame(n, env);
	env->MakeWritable(&frame);

	if (vi.IsYV12()) {
    int plane,cplane;
		for(cplane=0;cplane<3;cplane++) {
      if (cplane==0) plane = PLANAR_Y;
      if (cplane==1) plane = PLANAR_U;
      if (cplane==2) plane = PLANAR_V;
			const int row_size = frame->GetRowSize(plane|PLANAR_ALIGNED);
			uc* q = frame->GetWritePtr(plane);
			const int pitch = frame->GetPitch(plane);
			int height = frame->GetHeight(plane);
			if (env->GetCPUFlags() & CPUF_INTEGER_SSE) {
				AFH_YV12_MMX(q,height,pitch,row_size,amount);
			} else {
				AFH_YV12_C(q,height,pitch,row_size,amount);
			} 
		}
	} else {
		const int row_size = vi.RowSize();
		uc* q = frame->GetWritePtr();
		const int pitch = frame->GetPitch();
		if (vi.IsYUY2()) {
			if (env->GetCPUFlags() & CPUF_INTEGER_SSE) {
				AFH_YUY2_MMX(q,vi.height,pitch,vi.width,amount);
			} else {
				AFH_YUY2_C(q,vi.height,pitch,vi.width,amount);
			}
		} 
		else if (vi.IsRGB32()) {
			if (env->GetCPUFlags() & CPUF_INTEGER_SSE) {
				AFH_RGB32_MMX(q,vi.height,pitch,vi.width,amount);
			} else {
				AFH_RGB32_C(q,vi.height,pitch,vi.width,amount);
			}
		} 
		else { //rgb24
			AFH_RGB24_C(q,vi.height,pitch,vi.width,amount);
		}
	}

	return frame;
}

void AFH_RGB32_C(uc* p, int height, const int pitch, const int width, const int amount) {
  const int center_weight = amount*2;
  const int outer_weight = 32768-amount;
  for (int y = height; y>0; --y) 
  {
	  uc bb = p[0];
      uc gg = p[1];
      uc rr = p[2];
	  uc aa = p[3];
      for (int x = 1; x < width-1; ++x) 
	  {
        uc b = ScaledPixelClip(p[x*4+0] * center_weight + (bb + p[x*4+4]) * outer_weight);
	    bb = p[x*4+0]; p[x*4+0] = b;
        uc g = ScaledPixelClip(p[x*4+1] * center_weight + (gg + p[x*4+5]) * outer_weight);
	    gg = p[x*4+1]; p[x*4+1] = g;
        uc r = ScaledPixelClip(p[x*4+2] * center_weight + (rr + p[x*4+6]) * outer_weight);
	    rr = p[x*4+2]; p[x*4+2] = r;
        uc a = ScaledPixelClip(p[x*4+3] * center_weight + (aa + p[x*4+7]) * outer_weight);
	    aa = p[x*4+3]; p[x*4+3] = a;
      }
	  p += pitch;
    }
}

void AFH_RGB32_MMX(const uc* p, const int height, const int pitch, const int width, const int amount) {
	__declspec(align(8)) static __int64 cw;
	__asm { 
		// signed word center weight ((amount+0x100)>>9) x4
		mov		eax,amount
		add		eax,100h
		sar		eax,9
		movd	mm1,eax
		pshufw	mm2,mm1,0
		movq	cw,mm2
	}
	__declspec(align(8)) static __int64 ow;
	__asm {
		// signed word outer weight ((32768-amount+0x100)>>9) x4
		mov		eax,8000h
		sub		eax,amount
		add		eax,100h
		sar		eax,9
		movd	mm1,eax
		pshufw	mm2,mm1,0
		movq	ow,mm2
	}
	// round masks
	__declspec(align(8)) const static __int64 r6 = 0x0020002000200020;
	__declspec(align(8)) const static __int64 r7 = 0x0040004000400040;

	for (int y=0;y<height;y++) {

	__asm {

		mov		ecx,p
		mov		edi,width
		shr		edi,1

		pxor		mm0,mm0
		movq		mm1,[ecx]

row_loop:
		movq		mm2,[ecx]

		movq		mm7,mm1
		punpckhbw	mm7,mm0
		movq		mm4,mm2
		punpcklbw	mm4,mm0
		pmullw		mm4,cw
		movq		mm5,mm2
		punpckhbw	mm5,mm0
		paddsw		mm7,mm5
		pmullw		mm7,ow
		paddusw		mm7,r7
		psraw		mm7,7
		paddusw		mm4,r6
		psraw		mm4,6
		paddsw		mm7,mm4

		movq		mm1,mm2
		movq		mm2,[ecx+8]

		movq		mm6,mm1
		punpcklbw	mm6,mm0
		movq		mm4,mm1
		punpckhbw	mm4,mm0
		pmullw		mm4,cw
		movq		mm5,mm2
		punpcklbw	mm5,mm0
		paddsw		mm6,mm5
		pmullw		mm6,ow
		paddusw		mm6,r7
		psraw		mm6,7
		paddusw		mm4,r6
		psraw		mm4,6
		paddsw		mm6,mm4

		packuswb	mm7,mm6
		movq		[ecx],mm7

		add			ecx,8
		dec			edi
		cmp			edi,0
		jnle		row_loop
		}
		p += pitch;
	}
	__asm emms
}

void AFH_YUY2_C(uc* p, int height, const int pitch, const int width, const int amount) {
  const int center_weight = amount*2;
  const int outer_weight = 32768-amount;
		for (int y = height; y>0; --y) 
	    {
	      uc uv = p[1];
		  uc yy = p[2];
	      uc vu = p[3];
		  p[2] = ScaledPixelClip(p[2] * center_weight + (p[0] + p[4]) * outer_weight);
	      for (int x = 2; x < width-2; ++x) 
		  {
	        uc w = ScaledPixelClip(p[x*2+1] * center_weight + (uv + p[x*2+5]) * outer_weight);
		    uv = vu; vu = p[x*2+1]; p[x*2+1] = w;
	        uc y = ScaledPixelClip(p[x*2+0] * center_weight + (yy + p[x*2+2]) * outer_weight);
		    yy = p[x*2+0]; p[x*2+0] = y;
	      }
	      p[width*2-4] = ScaledPixelClip(p[width*2-4] * center_weight + (yy + p[width*2-2]) * outer_weight);
		  p += pitch;
		}
}


void AFH_YUY2_MMX(const uc* p, const int height, const int pitch, const int width, const int amount) {
	__declspec(align(8)) static __int64 cw;
	__asm { 
		// signed word center weight ((amount+0x100)>>9) x4
		mov		eax,amount
		add		eax,100h
		sar		eax,9
		movd	mm1,eax
		pshufw	mm2,mm1,0
		movq	cw,mm2
	}
	__declspec(align(8)) static __int64 ow;
	__asm {
		// signed word outer weight ((32768-amount+0x100)>>9) x4
		mov		eax,8000h
		sub		eax,amount
		add		eax,100h
		sar		eax,9
		movd	mm1,eax
		pshufw	mm2,mm1,0
		movq	ow,mm2
	}
	// round masks
	__declspec(align(8)) const static __int64 r6 = 0x0020002000200020;
	__declspec(align(8)) const static __int64 r7 = 0x0040004000400040;
	// YY and UV masks
	__declspec(align(8)) const static __int64 ym = 0x00FF00FF00FF00FF;
	__declspec(align(8)) const static __int64 cm = 0xFF00FF00FF00FF00;
	// (cm used as clip mask too)
	__declspec(align(8)) const static __int64 zero = 0x0000000000000000;


	for (int y=0;y<height;y++) {

	__asm {
		mov		ecx,p
		mov		edi,width
		shr		edi,2

		movq		mm1,[ecx]

row_loop:
		movq		mm2,[ecx]
		movq		mm3,[ecx+8]

		movq		mm4,mm1
		pand		mm4,ym
		movq		mm5,mm2
		pand		mm5,ym
		movq		mm6,mm3
		pand		mm6,ym
		psrlq		mm4,48
		psllq		mm6,48
		movq		mm7,mm5
		psllq		mm7,16
		por			mm4,mm7
		movq		mm7,mm5
		psrlq		mm7,16
		por			mm6,mm7

		paddsw		mm4,mm6
		pmullw		mm4,ow
		pmullw		mm5,cw
		paddusw		mm4,r7
		psraw		mm4,7
		paddusw		mm5,r6
		psraw		mm5,6
		paddsw		mm4,mm5
		movq		mm0,mm4

		movq		mm4,mm1
		pand		mm4,cm
		psrlq		mm4,8
		movq		mm5,mm2
		pand		mm5,cm
		psrlq		mm5,8
		movq		mm6,mm3
		pand		mm6,cm
		psrlq		mm6,8
		psrlq		mm4,32
		psllq		mm6,32
		movq		mm7,mm5
		psllq		mm7,32
		por			mm4,mm7
		movq		mm7,mm5
		psrlq		mm7,32
		por			mm6,mm7

		paddsw		mm4,mm6
		pmullw		mm4,ow
		pmullw		mm5,cw
		paddusw		mm4,r7
		psraw		mm4,7
		paddusw		mm5,r6
		psraw		mm5,6
		paddsw		mm4,mm5

		packuswb	mm0,mm0
		punpcklbw	mm0,zero
		packuswb	mm4,mm4
		punpcklbw	mm4,zero

		movq		mm1,mm2
		psllq		mm4,8
		por			mm0,mm4
		movq		[ecx],mm0

		add			ecx,8
		dec			edi
		cmp			edi,0
		jnle		row_loop
		}
	p += pitch;
	}
	__asm emms
}

void AFH_RGB24_C(uc* p, int height, const int pitch, const int width, const int amount) {
  const int center_weight = amount*2;
  const int outer_weight = 32768-amount;
  for (int y = height; y>0; --y) 
    {

      uc bb = p[0];
      uc gg = p[1];
      uc rr = p[2];
      for (int x = 1; x < width-1; ++x) 
      {
        uc b = ScaledPixelClip(p[x*3+0] * center_weight + (bb + p[x*3+3]) * outer_weight);
        bb = p[x*3+0]; p[x*3+0] = b;
        uc g = ScaledPixelClip(p[x*3+1] * center_weight + (gg + p[x*3+4]) * outer_weight);
        gg = p[x*3+1]; p[x*3+1] = g;
        uc r = ScaledPixelClip(p[x*3+2] * center_weight + (rr + p[x*3+5]) * outer_weight);
        rr = p[x*3+2]; p[x*3+2] = r;
      }
      p += pitch;
    }
}

void AFH_YV12_C(uc* p, int height, const int pitch, const int row_size, const int amount) 
{
	const int center_weight = amount*2;
	const int outer_weight = 32768-amount;
	uc pp,l;
	for (int y = height; y>0; --y) {
		l = p[0];
		for (int x = 1; x < row_size-1; ++x) {
			pp = ScaledPixelClip(p[x] * center_weight + (l + p[x+1]) * outer_weight);
			l=p[x]; p[x]=pp;
		}
		p += pitch;
	}
}

#define scale(mmAA,mmBB,mmCC,mmA,mmB,mmC,zeros,punpckXbw)	\
__asm	movq		mmA,mmAA	\
__asm	punpckXbw	mmA,zeros	\
__asm	movq		mmB,mmBB	\
__asm	punpckXbw	mmB,zeros	\
__asm	pmullw		mmB,cw		\
__asm	movq		mmC,mmCC	\
__asm	punpckXbw	mmC,zeros	\
__asm	paddsw		mmA,mmC		\
__asm	pmullw		mmA,ow		\
__asm	paddusw		mmA,r7		\
__asm	psraw		mmA,7		\
__asm	paddusw		mmB,r6		\
__asm	psraw		mmB,6		\
__asm	paddsw		mmA,mmB


void AFH_YV12_MMX(uc* p, int height, const int pitch, const int row_size, const int amount) 
{
	__declspec(align(8)) static __int64 cw;
	__asm { 
		// signed word center weight ((amount+0x100)>>9) x4
		mov		eax,amount
		add		eax,100h
		sar		eax,9
		movd	mm1,eax
		pshufw	mm2,mm1,0
		movq	cw,mm2
	}
	__declspec(align(8)) static __int64 ow;
	__asm {
		// signed word outer weight ((32768-amount+0x100)>>9) x4
		mov		eax,8000h
		sub		eax,amount
		add		eax,100h
		sar		eax,9
		movd	mm1,eax
		pshufw	mm2,mm1,0
		movq	ow,mm2
	}

	// round masks
	__declspec(align(8)) const static __int64 r6 = 0x0020002000200020;
	__declspec(align(8)) const static __int64 r7 = 0x0040004000400040;

	for (int y=0;y<height;y++) {

	__asm {

		mov		ecx,p
		mov		edi,row_size
		shr		edi,3
    dec   edi
		pxor		mm0,mm0
; first row
		movq		mm1,[ecx]
		movq		mm2,[ecx]
		movq		mm3,[ecx+1]

    scale(mm1,mm2,mm3,mm4,mm5,mm6,mm0,punpcklbw)
		
		movq		mm7,mm4

		scale(mm1,mm2,mm3,mm4,mm5,mm6,mm0,punpckhbw)

		packuswb	mm7,mm4
		movq		[ecx],mm7

    add			ecx,8
		dec			edi
    jz      out_row_loop

    align 16
row_loop:

		movq		mm1,[ecx-1]
		movq		mm2,[ecx]
		movq		mm3,[ecx+1]

		scale(mm1,mm2,mm3,mm4,mm5,mm6,mm0,punpcklbw)
		
		movq		mm7,mm4

		scale(mm1,mm2,mm3,mm4,mm5,mm6,mm0,punpckhbw)

		packuswb	mm7,mm4
		movq		[ecx],mm7

		add			ecx,8
		dec			edi
		jnz			row_loop
out_row_loop:
		movq		mm1,[ecx-1]
		movq		mm2,[ecx]
		movq		mm3,[ecx]

		scale(mm1,mm2,mm3,mm4,mm5,mm6,mm0,punpcklbw)
		
		movq		mm7,mm4

		scale(mm1,mm2,mm3,mm4,mm5,mm6,mm0,punpckhbw)

		packuswb	mm7,mm4
		movq		[ecx],mm7

		}
		p += pitch;
	}
	__asm emms
}




/************************************************
 *******   Sharpen/Blur Factory Methods   *******
 ***********************************************/

AVSValue __cdecl Create_Sharpen(AVSValue args, void*, IScriptEnvironment* env) 
{
  const double amountH = args[1].AsFloat(), amountV = args[2].AsFloat(amountH);
  if (amountH < -1.5849625 || amountH > 1.0 || amountV < -1.5849625 || amountV > 1.0)
    env->ThrowError("Sharpen: arguments must be in the range -1.58 to 1.0");
  return new AdjustFocusH(amountH, new AdjustFocusV(amountV, args[0].AsClip()));
}

AVSValue __cdecl Create_Blur(AVSValue args, void*, IScriptEnvironment* env) 
{
  const double amountH = args[1].AsFloat(), amountV = args[2].AsFloat(amountH);
  if (amountH < -1.0 || amountH > 1.5849625 || amountV < -1.0 || amountV > 1.5849625)
    env->ThrowError("Blur: arguments must be in the range -1.0 to 1.58");
  return new AdjustFocusH(-amountH, new AdjustFocusV(-amountV, args[0].AsClip()));
}




/***************************
 ****  TemporalSoften  *****
 **************************/

 /**
 * YV12 code (c) by Klaus Post // sh0dan 2002
 *
 * - All frames are loaded (we rely on the cache for this, which is why it has to be rewritten)
 * - Pointer array is given to the mmx function for planes.
 * - One line of each planes is one after one compared to the current frame. 
 * - Accumulated values are stored in an arrays. (as shorts)
 * - The divisor is stored in a separate array (as bytes)
 * - The divisor is looked up.
 * - Result is stored.
 **/


#define SCALE(i)    (int)((double)32768.0/(i)+0.5)
const short TemporalSoften::scaletab[] = {
  0,
  32767,     // because of signed MMX multiplications
  SCALE(2),
  SCALE(3),
  SCALE(4),
  SCALE(5),
  SCALE(6),
  SCALE(7),
  SCALE(8),
  SCALE(9),
  SCALE(10),
  SCALE(11),
  SCALE(12),
  SCALE(13),
  SCALE(14),
  SCALE(15),
};
#undef SCALE



TemporalSoften::TemporalSoften( PClip _child, unsigned radius, unsigned luma_thresh, 
                                unsigned chroma_thresh, IScriptEnvironment* env )
  : GenericVideoFilter  (_child),
    chroma_threshold    (min(chroma_thresh,255)),
    luma_threshold      (min(luma_thresh,255)),
    kernel              (2*min(radius,MAX_RADIUS)+1),
    scaletab_MMX        (NULL)    
{
  if (!vi.IsYUV())
    env->ThrowError("TemporalSoften: requires YUV input");

  child->SetCacheHints(CACHE_RANGE,kernel);
  if ((vi.IsYUY2()) && (vi.width&7)) {
    env->ThrowError("Temporalsoften: YUY2 source must be multiple of 8 in width.");
  }
  if (vi.IsYUV()) {
    planes = new int[8];
    planeP = new const BYTE*[16];
    divtab = new int[16]; // First index = x/1
    for (int i = 0; i<16;i++)
      divtab[i] = 32768/(i+1);
    int c = 0;
    if (vi.IsYV12()) {
      if (luma_thresh>=0) {planes[c++] = PLANAR_Y; planes[c++] = luma_thresh;}
      if (chroma_thresh>=0) { planes[c++] = PLANAR_V;planes[c++] =chroma_thresh;planes[c++] = PLANAR_U;planes[c++] =chroma_thresh;}
    } else {
      planes[c++]=0;
      planes[c++]=luma_thresh|(chroma_thresh<<8);
    }
    planes[c]=0;
    accum_line=(int*)_aligned_malloc(((vi.width*vi.BytesFromPixels(1)+FRAME_ALIGN-1)/8)*16,8);
    div_line=(int*)_aligned_malloc(((vi.width*vi.BytesFromPixels(1)+FRAME_ALIGN-1)/8)*16,8);
    return;
  }

  accu = (DWORD*)new DWORD[(vi.width * vi.height * kernel * vi.BytesFromPixels(1)) >> 2];
  if (!accu)
    env->ThrowError("TemporalSoften: memory allocation error");
  memset(accu, 0, vi.width * vi.height * kernel * vi.BytesFromPixels(1));

  nprev = -100;

  if (env->GetCPUFlags() & CPUF_MMX) {
    scaletab_MMX = new __int64[65536]; 
    if (!scaletab_MMX) {
      delete[] accu;
      env->ThrowError("TemporalSoften: memory allocation error");
    }
    for(int i=0; i<65536; i++)
      scaletab_MMX[i] = ( (__int64) scaletab[ i       & 15]        ) |
                        (((__int64) scaletab[(i >> 4) & 15]) << 16 ) |
                        (((__int64) scaletab[(i >> 8) & 15]) << 32 ) |
                        (((__int64) scaletab[(i >>12) & 15]) << 48 );
  }
}



TemporalSoften::~TemporalSoften(void) 
{
  if (!vi.IsYUV()) {
    delete[] accu;
    delete[] scaletab_MMX;
  } else {
    delete[] planes;
    delete[] divtab;
    delete[] planeP;
    _aligned_free(accum_line);
    _aligned_free(div_line);
  }
}



PVideoFrame TemporalSoften::GetFrame(int n, IScriptEnvironment* env) 
{
  __int64 i64_thresholds = 0x1000010000100001i64;
  // planes[] holds [plane_nr][plane_threshold][plane2_nr][plane2_threshold]... and so on.
  // planeP[] holds current plane pointers for all frames. [frame0_ptr][frame2_ptr]
//  if (vi.IsYUV()) {
    PVideoFrame frame;
    int radius = (kernel-1) / 2 ;
    int c=0;
    PVideoFrame dst = env->NewVideoFrame(vi);
    do {
      int c_thresh = planes[c+1];  // Threshold for current plane.
      int d=0;
      for (int i = radius;i>=1;i--) { // Fetch all planes sequencially
	      PVideoFrame tframe = child->GetFrame(min(vi.num_frames-1,max(n-i,1)), env);
        planeP[d++] = tframe->GetReadPtr(planes[c]);
      }
  	  frame = child->GetFrame(n, env);          // get the new frame
      const BYTE* c_plane= frame->GetReadPtr(planes[c]);
      for (i = 1;i<=radius;i++) { // Fetch all planes sequencially
	      PVideoFrame tframe = child->GetFrame(min(vi.num_frames-1,max(n+i,1)), env);
        planeP[d++] = tframe->GetReadPtr(planes[c]);
      }

      int rowsize=frame->GetRowSize(planes[c]|PLANAR_ALIGNED);
      int h = frame->GetHeight(planes[c]);
      int w=frame->GetRowSize(planes[c]); // Non-aligned
      int pitch = frame->GetPitch(planes[c]);
      i64_thresholds = (__int64)c_thresh | (__int64)(c_thresh<<8) | (((__int64)c_thresh)<<16) | (((__int64)c_thresh)<<24);
      if (vi.IsYUY2()) { 
        i64_thresholds = (__int64)luma_threshold | (__int64)(chroma_threshold<<8) | ((__int64)(luma_threshold)<<16) | ((__int64)(chroma_threshold)<<24);
      }
      i64_thresholds |= (i64_thresholds<<32);
      BYTE* dstp = dst->GetWritePtr(planes[c]);

      if (c_thresh) {
        for (int y=0;y<h;y++) { // One line at the time
          if ((env->GetCPUFlags() & CPUF_INTEGER_SSE)) {
            isse_accumulate_line(c_plane, planeP, d-1, rowsize,&i64_thresholds);
          } else {
            mmx_accumulate_line(c_plane, planeP, d-1, rowsize,&i64_thresholds);
          }
          short* s_accum_line = (short*)accum_line;
          BYTE* b_div_line = (BYTE*)div_line;
          for (int i=0;i<w;i++) {
            int div=divtab[b_div_line[i]];
            dstp[i]=(div*(int)s_accum_line[i]+(div>>1))>>15; //Todo: Attempt asm/mmx mix - maybe faster
          }
          for (int p=0;p<d;p++)
            planeP[p] += pitch;
          dstp+=dst->GetPitch(planes[c]);
          c_plane += pitch;
        }
      } else { // Just copy the plane
        env->BitBlt(dstp, dst->GetPitch(planes[c]), c_plane, pitch, rowsize, h);
      }
      c+=2;
    } while (planes[c]);
    return dst;
//  }
 /*
  int noffset = n % kernel;                             // offset of the frame not yet in the buffer
  int coffset = (noffset + (kernel / 2) + 1) % kernel;  // center offset, offset of the frame to be returned 
  
  if (n != nprev+1)
    FillBuffer(n, noffset, env);                        // refill buffer in case of non-linear access (slow)

  nprev = n;

  int i = min(n + (kernel/2), vi.num_frames-1);         // do not read past the end
  PVideoFrame frame = child->GetFrame(i, env);          // get the new frame
  env->MakeWritable(&frame);
  DWORD* pframe = (DWORD*)frame->GetWritePtr();
  const int rowsize = frame->GetRowSize();
  int height = frame->GetHeight();
  const int modulo = frame->GetPitch() - rowsize;

  if (env->GetCPUFlags() & CPUF_MMX)
    run_MMX(pframe, rowsize, height, modulo, noffset, coffset);
  else
    run_C(pframe, rowsize, height, modulo, noffset, coffset);

  return frame;
*/  
}


void TemporalSoften::mmx_accumulate_line(const BYTE* c_plane, const BYTE** planeP, int planes, int rowsize, __int64* t) {
  __declspec(align(8)) static const __int64 indexer = 0x0101010101010101i64;
  __declspec(align(8)) static const __int64 t2 = *t;
  int* _accum_line=accum_line;
  int* _div_line=div_line;

  __asm {
    mov esi,c_plane;
    xor eax,eax          // EAX will be plane offset (all planes).
    mov ecx,[_accum_line]
testplane:
    mov ebx,[rowsize]
    cmp ebx, eax
    jle outloop

    movq mm0,[esi+eax]  // Load current frame pixels
     pxor mm2,mm2        // Clear mm2
    movq mm1,mm0
     movq mm3,mm0
    PUNPCKlbw mm3,mm2    // mm0 = lower 4 pixels  (exhanging h/l in these two give funny results)
     PUNPCKhbw mm1,mm2     // mm1 = upper 4 pixels

    mov edi,[planeP];  // Adress of planeP array is now in edi
    mov ebx,[planes]   // How many planes (this will be our counter)
    movq [ecx],mm3
     pxor mm7,mm7        // Clear divisor table
    lea edi,[edi+ebx*4]
    movq [ecx+8],mm1
kernel_loop:
    mov edx,[edi]
    movq mm1,[edx+eax]      // Load 8 pixels from test plane
     movq mm2,mm0
    movq mm5, mm1           // Save test plane pixels (twice for unpack)
     pxor mm4,mm4

    movq mm3,mm1            // Calc abs difference
     psubusb   mm1, mm2
    psubusb   mm2, mm3
    por       mm2, mm1               
     movq mm3,[t2]          // Using t also gives funny results
    PSUBUSB mm2,mm3
     movq mm1,mm5
    PCMPEQB mm2,mm4
    movq mm3,mm2
     movq mm6,mm2           // Store mask for accumulation
    punpckhbw mm3,mm4       // mm3 = upper 4 pixels mask
     punpcklbw mm2,mm4      // mm2 = lower 4 pixels mask
    pand mm6,[indexer]
     punpckhbw mm1,mm4       // mm1 = upper 4 pixels
    punpcklbw mm5,mm4      // mm5 = lower 4 pixels  // mm4, 
     paddb mm7,mm6
    pand  mm1,mm3
     pand  mm5,mm2          // mm2,mm3,mm6 also free
    movq mm2,[ecx]          // mm2 = lower accumulated 
     movq mm3,[ecx+8]
    paddusw mm2,mm5         // Add lower pixel values
     paddusw mm3,mm1        // Add upper pixel values
    movq [ecx],mm2
     movq [ecx+8],mm3

    sub edi,4
    dec ebx
    jnz kernel_loop
    mov edx,[_div_line]
    add eax,8   // Next 8 pixels
    movq [edx],mm7
    add ecx,16  // Next 8 accumulated pixels
    add edx,8   // Next 8 divisor pixels
    mov [_div_line],edx
    jmp testplane
outloop:
    emms
  }
}


void TemporalSoften::isse_accumulate_line(const BYTE* c_plane, const BYTE** planeP, int planes, int rowsize, __int64* t) {
  __declspec(align(8)) static const __int64 indexer = 0x0101010101010101i64;
  __declspec(align(8)) static const __int64 t2 = *t;
  int* _accum_line=accum_line;
  int* _div_line=div_line;

  __asm {
    mov esi,c_plane;
    xor eax,eax          // EAX will be plane offset (all planes).
    mov ecx,[_accum_line]
testplane:
    mov ebx,[rowsize]
    cmp ebx, eax
    jle outloop

    movq mm0,[esi+eax]  // Load current frame pixels
     pxor mm2,mm2        // Clear mm2
    movq mm1,mm0
     movq mm3,mm0
    PUNPCKlbw mm3,mm2    // mm0 = lower 4 pixels  (exhanging h/l in these two give funny results)
     PUNPCKhbw mm1,mm2     // mm1 = upper 4 pixels

    mov edi,[planeP];  // Adress of planeP array is now in edi
    mov ebx,[planes]   // How many planes (this will be our counter)
    movq [ecx],mm3
     pxor mm7,mm7        // Clear divisor table
    lea edi,[edi+ebx*4]
    movq [ecx+8],mm1
kernel_loop:
    mov edx,[edi]
    movq mm1,[edx+eax]      // Load 8 pixels from test plane
     movq mm2,mm0
    movq mm5, mm1           // Save test plane pixels (twice for unpack)
     pxor mm4,mm4
    pmaxub mm2,mm1          // Calc abs difference
     pminub mm1,mm0
    psubusb mm2,mm1
     movq mm3,[t2]          // Using t also gives funny results
    PSUBUSB mm2,mm3         // Subtrack threshold (unsigned, so all below threshold will give 0)
     movq mm1,mm5
    PCMPEQB mm2,mm4         // Compare values to 0
     prefetchnta [edx+eax+64]
    movq mm3,mm2
     movq mm6,mm2           // Store mask for accumulation
    punpckhbw mm3,mm4       // mm3 = upper 4 pixels mask
     punpcklbw mm2,mm4      // mm2 = lower 4 pixels mask
    pand mm6,[indexer]
     punpckhbw mm1,mm4       // mm1 = upper 4 pixels
    punpcklbw mm5,mm4      // mm5 = lower 4 pixels  // mm4, 
     paddb mm7,mm6
    pand  mm1,mm3
     pand  mm5,mm2          // mm2,mm3,mm6 also free
    movq mm2,[ecx]          // mm2 = lower accumulated 
     movq mm3,[ecx+8]
    paddusw mm2,mm5         // Add lower pixel values
     paddusw mm3,mm1        // Add upper pixel values
    movq [ecx],mm2
     movq [ecx+8],mm3

    sub edi,4
    dec ebx
    jnz kernel_loop
    mov edx,[_div_line]
    add eax,8   // Next 8 pixels
    movq [edx],mm7
    add ecx,16  // Next 8 accumulated pixels
    add edx,8   // Next 8 divisor pixels
    mov [_div_line],edx
    jmp testplane
outloop:
    emms
  }
}


void TemporalSoften::run_C(DWORD *pframe, int rowsize, int height, int modulo, int noffset, int coffset)
{
  DWORD* pbuf = accu;
  do {
    int x = rowsize >> 2;
    do {
      const DWORD center = pbuf[coffset];
      const int v  =  center >> 24        ;
      const int y2 = (center >> 16) & 0xff;
      const int u  = (center >>  8) & 0xff;
      const int y1 =  center        & 0xff;
      unsigned int y1accum = 0, uaccum = 0, y2accum = 0, vaccum = 0;
      unsigned int county1 = 0, county2 = 0, countu = 0, countv = 0;

      pbuf[noffset] = *pframe;
    
      for(int i=0; i<kernel; ++i) {
        const DWORD c = *pbuf++;
        const int cv  =  c >> 24        ;
        const int cy2 = (c >> 16) & 0xff;
        const int cu  = (c >>  8) & 0xff;
        const int cy1 =  c        & 0xff;
      
        if (IsClose(y1, cy1, luma_threshold  )) { y1accum += cy1; county1++; }
        if (IsClose(y2, cy2, luma_threshold  )) { y2accum += cy2; county2++; }
        if (IsClose(u , cu , chroma_threshold)) { uaccum  += cu ; countu++ ; }
        if (IsClose(v , cv , chroma_threshold)) { vaccum  += cv ; countv++ ; }
      }

      y1accum = (y1accum * scaletab[county1] + 16384) >> 15;
      y2accum = (y2accum * scaletab[county2] + 16384) >> 15;
      uaccum  = (uaccum  * scaletab[countu ] + 16384) >> 15;
      vaccum  = (vaccum  * scaletab[countv ] + 16384) >> 15;

      *pframe++ = (vaccum<<24) + (y2accum<<16) + (uaccum<<8) + y1accum;
        
    } while(--x);

    pframe += modulo >> 2;

  } while(--height);
}



void TemporalSoften::run_MMX(DWORD *pframe, int rowsize, int height, int modulo, int noffset, int coffset)
{
  noffset <<= 2;
  coffset <<= 2;
  const DWORD tresholds = (chroma_threshold << 16) | luma_threshold;
  DWORD* pbuf = accu;
  const int kern = kernel;
  __int64 counters = (__int64)kern * 0x0001000100010001i64;
  const int w = rowsize >> 2;
  const __int64* scaletab = scaletab_MMX;
  __declspec(align(8)) static const __int64 indexer = 0x1000010000100001i64;

  __asm {
    mov       esi, pframe
    mov       edi, pbuf
    movd      mm5, tresholds
    punpckldq mm5, mm5
    pxor      mm0, mm0
    mov       ebx, noffset

TS_yloop:
    mov       ecx, w
TS_xloop:
    mov       edx, [esi]          ; get new pixel
    mov       eax, coffset
    mov       [edi+ebx], edx      ; put into place
    pxor      mm6, mm6            ; clear accumulators
    movd      mm1, [edi+eax]      ; get center pixel
    punpcklbw mm1, mm0            ; 0 Vc 0 Y2c 0 Uc 0 Y1c
    add       esi, 4              ; pframe++
    mov       edx, kern           ; load kernel length
    movq      mm7, counters       ; initialize counters

TS_timeloop:
    movd      mm2, [edi]          ; *pbuf: V Y2 U Y1
    movq      mm3, mm1            ; center pixel
    punpcklbw mm2, mm0            ; 0 V 0 Y2 0 U 0 Y1
    movq      mm4, mm2
    psubusw   mm3, mm2
    psubusw   mm2, mm1
    por       mm3, mm2            ; |V-Vc| |Y2-Y2c| |U-Uc| |Y1-Y1c|
    pcmpgtw   mm3, mm5
    add       edi, 4
    paddw     mm7, mm3            ; -1 to counter if above treshold
    pandn     mm3, mm4            ; else add to accumulators
    paddw     mm6, mm3
    dec       edx
    jne       TS_timeloop         ; kernel times

    psllw     mm6, 1              ; accum *= 2
    paddw     mm6, mm7            ; accum += count  (for rounding)

    ; construct 16 bits index
    ; mm7 = count3 count2 count1 count0 (words, each count<=15)
    pmaddwd   mm7, indexer
    movq      mm2, mm7
    punpckhdq mm7, mm7
    mov       eax, scaletab
    paddd     mm2, mm7
    movd      edx, mm2

    ; index = edx = count0+16*(count1+16*(count2+16*count3))

    movq      mm7, [eax+edx*8]
    pmulhw    mm6, mm7
    packuswb  mm6, mm6
    dec       ecx
    movd      [esi-4], mm6        ; store output pixel
    jne       TS_xloop

    add       esi, modulo         ; skip to next scanline

    dec       height              ; yloop (height)
    jne       TS_yloop

    emms
  };
}



PVideoFrame TemporalSoften::LoadFrame(int n, int offset, IScriptEnvironment* env)
/**
  * Get a frame from child and put it at the specified offset in the interleaved buffer
 **/
{
  int m=min(max(n,0),vi.num_frames-1);

  const PVideoFrame frame = child->GetFrame(m, env);
  const BYTE* srcp = frame->GetReadPtr();
  const int pitch = frame->GetPitch();
  const int width = frame->GetRowSize() >> 2;
  const int height = frame->GetHeight();
  
  DWORD* accum = accu + offset;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++, accum += kernel)
      *accum = ((DWORD*)srcp)[x];
      srcp += pitch;
  }

  return frame;
}



void TemporalSoften::FillBuffer(int n, int offset, IScriptEnvironment* env)
/**
  * (re)initialize the interleaved buffer with necessary frames
 **/ 
{
  for (int i = 1; i < kernel; i++) {
    int numframe = n - (kernel / 2) - 1 + i;
    int pos = (i + offset) % kernel;
    LoadFrame(numframe, pos, env);
  }
}



AVSValue __cdecl TemporalSoften::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  return new TemporalSoften( args[0].AsClip(), args[1].AsInt(), args[2].AsInt(), 
                             args[3].AsInt(), env );
}







/****************************
 ****  Spatial Soften   *****
 ***************************/

SpatialSoften::SpatialSoften( PClip _child, int _radius, unsigned _luma_threshold, 
                              unsigned _chroma_threshold, IScriptEnvironment* env )
  : GenericVideoFilter(_child), diameter(_radius*2+1),
    luma_threshold(_luma_threshold), chroma_threshold(_chroma_threshold)
{
  if (!vi.IsYUY2())
    env->ThrowError("SpatialSoften: requires YUY2 input");
}


PVideoFrame SpatialSoften::GetFrame(int n, IScriptEnvironment* env) 
{
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  const BYTE* srcp = src->GetReadPtr();
  BYTE* dstp = dst->GetWritePtr();
  int src_pitch = src->GetPitch();
  int dst_pitch = dst->GetPitch();
  int row_size = src->GetRowSize();

  for (int y=0; y<vi.height; ++y) 
  {
    const BYTE* line[65];    // better not make diameter bigger than this...
    for (int h=0; h<diameter; ++h)
      line[h] = &srcp[src_pitch * min(max(y+h-(diameter>>1), 0), vi.height-1)];
    int x;

    int edge = (diameter+1) & -4;
    for (x=0; x<edge; ++x)  // diameter-1 == (diameter>>1) * 2
      dstp[y*dst_pitch + x] = srcp[y*src_pitch + x];
    for (; x < row_size - edge; x+=2) 
    {
      int cnt=0, _y=0, _u=0, _v=0;
      int xx = x | 3;
      int Y = srcp[y*src_pitch + x], U = srcp[y*src_pitch + xx - 2], V = srcp[y*src_pitch + xx];
      for (int h=0; h<diameter; ++h) 
      {
        for (int w = -diameter+1; w < diameter; w += 2) 
        {
          int xw = (x+w) | 3;
          if (IsClose(line[h][x+w], Y, luma_threshold) && IsClose(line[h][xw-2], U,
                      chroma_threshold) && IsClose(line[h][xw], V, chroma_threshold)) 
          {
            ++cnt; _y += line[h][x+w]; _u += line[h][xw-2]; _v += line[h][xw];
          }
        }
      }
      dstp[y*dst_pitch + x] = (_y + (cnt>>1)) / cnt;
      if (!(x&3)) {
        dstp[y*dst_pitch + x+1] = (_u + (cnt>>1)) / cnt;
        dstp[y*dst_pitch + x+3] = (_v + (cnt>>1)) / cnt;
      }
    }
    for (; x<row_size; ++x)
      dstp[y*dst_pitch + x] = srcp[y*src_pitch + x];
  }

  return dst;
}


AVSValue __cdecl SpatialSoften::Create(AVSValue args, void*, IScriptEnvironment* env)
{
  return new SpatialSoften( args[0].AsClip(), args[1].AsInt(), args[2].AsInt(), 
                            args[3].AsInt(), env );
}