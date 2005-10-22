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

// ConvertMatrix (c) 2005 by Klaus Post and Ian B


#include "stdafx.h"

#include "convert_matrix.h"


#define USE_DYNAMIC_COMPILER true



MatrixGenerator3x3::MatrixGenerator3x3() : dyn_dest(0), dyn_src(0) {
  pre_add = post_add = 0;
  src_pixel_step = 4;   // Change to 3 for RGB24 for instance.
  dest_pixel_step = 4;  // Note that 4 bytes will be written at the time, always.
  aligned_rounder = NULL;
}

MatrixGenerator3x3::~MatrixGenerator3x3() {
  assembly.Free();
  if (aligned_rounder) {
    _aligned_free(aligned_rounder);
    aligned_rounder = 0;
  }
}

/***************************
 * Planar to interleaved converter
 *
 * MMX code by IanB
 ***************************/

void MatrixGenerator3x3::GeneratePacker(int width, IScriptEnvironment* env) {

  __declspec(align(8)) static const __int64 rounder_ones = 0x0101010101010101; 

  Assembler x86;   // This is the class that assembles the code.

  bool isse = !!(env->GetCPUFlags() & CPUF_INTEGER_SSE);

  int loops = width / 8;

  // Store registers
  x86.push(eax);
  x86.push(ebx);
  x86.push(ecx);
  x86.push(edx);
  x86.push(esi);
  x86.push(edi);
  x86.push(ebp);

  x86.mov(eax, (int)&unpck_src);
  x86.mov(edx, (int)&unpck_dst);  

  x86.mov(esi, dword_ptr [eax]); // Pointer to array of src planes
  x86.mov(edx, dword_ptr [edx]);  // Load dest pointer
  x86.mov(eax, dword_ptr [esi]); // Plane 1
  x86.mov(ebx, dword_ptr [esi+sizeof(BYTE*)]); // Plane 2
  x86.mov(ecx, dword_ptr [esi+sizeof(BYTE*)*2]); // Plane 3
  x86.mov(edi, dword_ptr [edx]);  // edx no longer used
	x86.movq(mm7, qword_ptr[(int)&rounder_ones]);

  x86.xor(esi, esi);

  x86.mov(edx, loops);
  x86.label("loopback");

	x86.movq(mm0,qword_ptr[eax+esi]);			// S1R1Q1P1s1r1q1p1
	x86.movq(mm2,qword_ptr[ecx+esi]);			// S3R3Q3P3s3r3q3p3

	x86.movq(mm4,mm0);  						// S1R1Q1P1s1r1q1p1
	x86.movq(mm6,mm2);  						// S3R3Q3P3s3r3q3p3

  x86.punpcklbw(mm0,qword_ptr[ebx+esi]);			// s2s1r2r1q2q1p2p1
  x86.punpckhbw(mm4,qword_ptr[ebx+esi]);			// S2S1R2R1Q2Q1P2P1
  x86.punpcklbw(mm2,mm7);					        		// +1s3+1r3+1q3+1p3
  x86.punpckhbw(mm6,mm7);							        // +1S3+1R3+1Q3+1P3

  x86.movq(mm1,mm0);							// s2s1r2r1q2q1p2p1
  x86.movq(mm5,mm4);							// S2S1R2R1Q2Q1P2P1

  x86.punpcklwd(mm0,mm2);							// +1q3q2q1+1p3p2p1
  x86.punpckhwd(mm1,mm2);							// +1s3s2s1+1r3r2r1
  x86.punpcklwd(mm4,mm6);							// +1Q3Q2Q1+1P3P2P1
  x86.punpckhwd(mm5,mm6);							// +1S3S2S1+1R3R2R1

  x86.movq(qword_ptr[edi+esi*4+ 0],mm0);		// +1q3q2q1+1p3p2p1
  x86.movq(qword_ptr[edi+esi*4+ 8],mm1);		// +1s3s2s1+1r3r2r1
  x86.movq(qword_ptr[edi+esi*4+16],mm4);		// +1Q3Q2Q1+1P3P2P1
  x86.movq(qword_ptr[edi+esi*4+24],mm5);		// +1S3S2S1+1R3R2R1

  x86.add(esi, 8);
  
  x86.dec(edx);
  x86.jnz("loopback");

  x86.emms();
    // Restore registers
  x86.pop(ebp);
  x86.pop(edi);
  x86.pop(esi);
  x86.pop(edx);
  x86.pop(ecx);
  x86.pop(ebx);
  x86.pop(eax);
  x86.ret();
  packer = DynamicAssembledCode(x86, env, "ConvertMatrix: Dynamic MMX code could not be compiled.");
}


/***************************
 * Interleaved to planar converter
 *
 * Unpacks 8 pixels per loop.
 * Input must have enough space for this to happend.
 *
 * MMX code by IanB
 ***************************/


void MatrixGenerator3x3::GenerateUnPacker(int width, IScriptEnvironment* env) {

  Assembler x86;   // This is the class that assembles the code.

  bool isse = !!(env->GetCPUFlags() & CPUF_INTEGER_SSE);

  int loops = width / 8;


  // Store registers
  x86.push(eax);
  x86.push(ebx);
  x86.push(ecx);
  x86.push(edx);
  x86.push(esi);
  x86.push(edi);
  x86.push(ebp);

  x86.mov(eax, (int)&unpck_src);
  x86.mov(edx, (int)&unpck_dst);  

  x86.mov(esi, dword_ptr [eax]); // Pointer to array of src planes
  x86.mov(esi, dword_ptr [esi]); // Pointer to src plane
  x86.mov(edx, dword_ptr [edx]);  // Load dest pointer
  x86.mov(eax, dword_ptr [edx]); // Plane 1 dest
  x86.mov(ebx, dword_ptr [edx+sizeof(BYTE*)]); // Plane 2 dest
  x86.mov(ecx, dword_ptr [edx+sizeof(BYTE*)*2]); // Plane 3 dest  edx free

  x86.pxor(mm6,mm6);
  x86.xor(edi, edi);

  x86.mov(edx, loops);
  x86.label("loopback");

  x86.movq(mm0, qword_ptr[esi+edi*4]);  //P1, P2
  x86.movq(mm1, qword_ptr[esi+edi*4]);  //P3, P4

  x86.movq(mm0,qword_ptr[esi+edi*4]);    //   XXP3P2P1xxp3p2p1
  x86.movq(mm1,qword_ptr[esi+edi*4+8]);  //   XXQ3Q2Q1xxq3q2q1
  x86.movq(mm2,qword_ptr[esi+edi*4+16]); //   XXR3R2R1xxr3r2r1
  x86.movq(mm3,qword_ptr[esi+edi*4+24]); //   XXS3S2S1xxs3s2s1

  x86.punpckldq(mm4,mm0); //   xxp3p2p1........
  x86.punpckldq(mm5,mm1); //   xxq3q2q1........
  x86.punpckldq(mm6,mm2); //   xxr3r2r1........
  x86.punpckldq(mm7,mm3); //   xxs3s2s1........

  x86.punpckhbw(mm4,mm0); //   XXxxP3p3P2p2P1p1
  x86.punpckhbw(mm5,mm1); //   XXxxQ3q3Q2q2Q1q1
  x86.punpckhbw(mm6,mm2); //   XXxxR3r3R2r2R1r1
  x86.punpckhbw(mm7,mm3); //   XXxxS3s3S2s2S1s1

  x86.movq(mm0,mm4);      //   XXxxP3p3P2p2P1p1
  x86.movq(mm2,mm6);      //   XXxxR3r3R2r2R1r1

  x86.punpcklwd(mm4,mm5); //   Q2q2P2p2Q1q1P1p1
  x86.punpckhwd(mm0,mm5); //   XXxxXXxxQ3q3P3p3
  x86.punpcklwd(mm6,mm7); //   S2s2R2r2S1s1R1r1
  x86.punpckhwd(mm2,mm7); //   XXxxXXxxS3q3R3r3

  x86.movq(mm1,mm4);      //   Q2q2P2p2Q1q1P1p1
  x86.punpckldq(mm4,mm6); //   S1s1R1r1Q1q1P1p1

  x86.punpckhdq(mm1,mm6); //   S2s2R2r2Q2q2P2p2
  x86.punpckldq(mm0,mm2); //   S3s3R3r3Q3q3P3p3

  x86.movq(qword_ptr[eax+edi],mm4); //   S1s1R1r1Q1q1P1p1
  x86.movq(qword_ptr[ebx+edi],mm1); //   S2s2R2r2Q2q2P2p2
  x86.movq(qword_ptr[ecx+edi],mm0); //   S3s3R3r3Q3q3P3p3
 
  x86.add(edi,8);
  x86.dec(edx);
  x86.jnz("loopback");
  x86.emms();
    // Restore registers
  x86.pop(ebp);
  x86.pop(edi);
  x86.pop(esi);
  x86.pop(edx);
  x86.pop(ecx);
  x86.pop(ebx);
  x86.pop(eax);
  x86.ret();
  unpacker = DynamicAssembledCode(x86, env, "ConvertMatrix: Dynamic MMX code could not be compiled.");
}




/***
 *
 * (c) Klaus Post, 2005
 *
 * 8 bytes/loop
 * 2 pixels/loop
 * Following MUST be initialized prior to GenerateAssembly:
 * pre_add, post_add, src_pixel_step, dest_pixel_step
 * 
 * Faction is the fraction part in bits.
 * upper32_ones removes separate rounder, but requires every four pixel in the input to be ones.
 ***/

void MatrixGenerator3x3::GenerateAssembly(int width, int frac_bits, bool upper32_ones, IScriptEnvironment* env) {

  Assembler x86;   // This is the class that assembles the code.
  rounder = 0x0000000100000001 * (1<<(frac_bits-1)); 

  bool isse = !!(env->GetCPUFlags() & CPUF_INTEGER_SSE);
  bool unroll = false;   // Unrolled code ~30% slower on Athlon XP.

  int loops = width / 2;
  aligned_rounder = (__int64*)_aligned_malloc(sizeof(__int64),64);

  *aligned_rounder = rounder;

  if (pre_add && post_add)
    env->ThrowError("Internal error - only pre_add OR post_add can be used.");

  int last_pix_mask = 0;

  if (dest_pixel_step == 3) 
    last_pix_mask = 0xff000000;
  if (dest_pixel_step == 2) 
    last_pix_mask = 0xffff0000;
  if (dest_pixel_step == 1) 
    last_pix_mask = 0xffffff00;

  // Store registers
  x86.push(eax);
  x86.push(ebx);
  x86.push(ecx);
  x86.push(edx);
  x86.push(esi);
  x86.push(edi);
  x86.push(ebp);
  
  x86.mov(eax, (int)&dyn_src);
  x86.mov(ebx, (int)&dyn_matrix);  // Used in loop
  x86.mov(edx, (int)&dyn_dest);
  x86.mov(esi, dword_ptr [eax]); // eax no longer used
  x86.mov(ebx, dword_ptr [ebx]);
  x86.mov(edi, dword_ptr [edx]);  // edx no longer used

  if (last_pix_mask) {
    x86.mov(eax,(int)&last_pix); // Pointer to last pixel
    x86.mov(edx, dword_ptr[edi+width*dest_pixel_step]);
    x86.mov(dword_ptr[eax], edx);
  }

  if (pre_add) {
    x86.mov(eax, (int)&pre_add);  
    x86.movq(mm2, qword_ptr[eax]);  // Must be ready when entering loop
  }
  if (post_add) {
    x86.mov(eax, (int)&post_add);  
  }

  x86.mov(edx,(int)aligned_rounder);  // Rounder pointer in edx
  if (!unroll) {  // Should we create a loop instead of unrolling?
    x86.mov(ecx, loops);
    x86.label("loopback");
    loops = 1;
  }
  x86.pxor(mm7,mm7);
/****
 * Inloop usage:
 * eax: pre/post-add pointer
 * ebx: matrix pointer
 * ecx: loop counter
 * edx: rounder pointer
 * esi: source
 * edi: destination
 *****/
  for (int i=0; i<loops; i++) {
    if (isse && src_pixel_step == 4) {  // Load both.
      x86.movq(mm0, qword_ptr[esi]);
      x86.pswapd(mm1,mm0);
    } else {
      x86.movd(mm0, dword_ptr[esi]);
      x86.movd(mm1, dword_ptr[esi+src_pixel_step]);
    }

    x86.punpcklbw(mm0, mm7);     // Unpack bytes -> words
    x86.punpcklbw(mm1, mm7);     // Unpack bytes -> words

    if (pre_add) {
      x86.paddw(mm0,mm2);
      x86.paddw(mm1,mm2);
    }

    x86.movq(mm4,qword_ptr [ebx]);
    x86.movq(mm2, mm0);
    x86.pmaddwd(mm0,mm4);  // Part 1/3

    x86.movq(mm3, mm1);
    x86.pmaddwd(mm1,mm4);  // mm4 free

    if (!isse) {
      x86.movq(mm6,mm0);  // pixel 1
      x86.movq(mm5,mm1);  // pixel 2
      x86.psrlq(mm6, 32);
      x86.psrlq(mm5, 32);
    } else {
      x86.pswapd(mm6,mm0);  // Swap upper and lower on move
      x86.pswapd(mm5,mm1);  // Swap upper and lower on move
    }

    x86.paddd(mm6, mm0);    // First ready in lower
    x86.paddd(mm5, mm1);    // First ready in lower

    x86.movq(mm4, qword_ptr [ebx+8]);
    x86.punpckldq(mm6,mm5);  // Move mm5 lower to mm6 upper

// Element 2/3
    x86.movq(mm0, mm2);
    x86.movq(mm1, mm3);

    x86.pmaddwd(mm0,mm4);  // Part 1/3
    x86.pmaddwd(mm1,mm4);  // mm4 free

    if (!isse) {
      x86.movq(mm7,mm0);
      x86.movq(mm5,mm1);
      x86.psrlq(mm7, 32);
      x86.psrlq(mm5, 32);
    } else {
      x86.pswapd(mm7,mm0);  // Swap upper and lower on move
      x86.pswapd(mm5,mm1);  // Swap upper and lower on move
    }


    x86.movq(mm4,qword_ptr [ebx+16]);
    x86.paddd(mm7, mm0);    // First ready in lower

    x86.pmaddwd(mm2,mm4);  // Part 1/3
    x86.paddd(mm5, mm1);    // First ready in lower

    x86.pmaddwd(mm3,mm4);  // mm4 free
    x86.punpckldq(mm7,mm5);  // Move mm5 lower to mm7 upper

// Element 3/3

    if (!isse) {
      x86.movq(mm4,mm2);
      x86.movq(mm5,mm3);
      x86.psrlq(mm4, 32);
      x86.psrlq(mm5, 32);
    } else {
      x86.pswapd(mm4,mm2);  // Swap upper and lower on move
      x86.pswapd(mm5,mm3);  // Swap upper and lower on move
    }

    x86.paddd(mm4, mm2);   // E3/3
    x86.paddd(mm5, mm3);   // E3/3

    if (!upper32_ones) {
      x86.movq(mm0, qword_ptr[edx]);  // Load rounder
    }

    x86.punpckldq(mm4, mm5);  // Move mm5 lower to mm4 upper

    if (!upper32_ones) {
      x86.paddd(mm6, mm0);  // Add rounder  (1)
      x86.paddd(mm7, mm0);  // Add rounder  (2)
    }

    x86.psrad(mm6, frac_bits);  // Shift down

    if (!upper32_ones) {
      x86.paddd(mm4, mm0);  // Add rounder  (3)
    }

    x86.psrad(mm7, frac_bits);  // Shift down
    x86.movq(mm0,mm6);       // pixel 1

    x86.psrad(mm4, frac_bits);  // Shift down

    x86.punpckldq(mm0,mm7);  // Move mm7 lower to mm0 upper
    x86.punpckhdq(mm6,mm7);  // Move mm6 high to low, put mm7 high in upper pixel 2

    if (post_add) {
      x86.movq(mm2, qword_ptr[eax]);
    }

    x86.packssdw(mm0, mm4);    // Pack results into words (pixel 1 ready)

    if (isse) {
      x86.pswapd(mm4,mm4);
    } else {
      x86.psrlq(mm4, 32);  // Shift down
    }

    x86.packssdw(mm6, mm4);    // Pack results into words (pixel 2 ready)

    if (post_add) {
      x86.paddw(mm0,mm2);
      x86.paddw(mm6,mm2);
    }

    if (pre_add) {
      x86.movq(mm2, qword_ptr[eax]);
    }

    if (dest_pixel_step == 4) {
      x86.packuswb(mm0, mm6);    // Into bytes
      x86. pxor(mm7,mm7);

      x86.movq(qword_ptr[edi], mm0);  // Store
    } else {
      x86.packuswb(mm0, mm0);    // Into bytes
      x86.packuswb(mm6, mm6);    // Into bytes

      x86.movd(dword_ptr[edi], mm0);  // Store
      x86.pxor(mm7,mm7);
      x86.movd(dword_ptr[edi+dest_pixel_step], mm6);  // Store
    }

    x86.add(esi, src_pixel_step*2);
    x86.add(edi, dest_pixel_step*2);
  }
  if (!unroll) {  // Loop on if not unrolling
    x86.dec(ecx);
    x86.jnz("loopback");
  }

  if (last_pix_mask) {
    x86.mov(eax,(int)&last_pix); // Pointer to last pixel
    x86.mov(ebx, dword_ptr[eax]);  // Load Stored pixel
    x86.mov(ecx, dword_ptr[edi-dest_pixel_step]);  // Load new pixel
    x86.and(ebx, last_pix_mask);
    x86.and(ecx, last_pix_mask ^ 0xffffffff);
    x86.or(ebx, ecx);
    x86.mov(dword_ptr[edi-dest_pixel_step], ebx);  // Store new pixel
  }

   x86.emms();
    // Restore registers
   x86.pop(ebp);
   x86.pop(edi);
   x86.pop(esi);
   x86.pop(edx);
   x86.pop(ecx);
   x86.pop(ebx);
   x86.pop(eax);
   x86.ret();
  assembly = DynamicAssembledCode(x86, env, "ConvertMatrix: Dynamic MMX code could not be compiled.");
}


