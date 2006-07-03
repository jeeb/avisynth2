BITS 64

%include "convert\amd64inc.asm"

;=============================================================================
; Constants
;=============================================================================
;=============================================================================
; Read only data
;=============================================================================
ALIGN 16
%ifdef FORMAT_COFF
SECTION .rodata
%else
SECTION .rodata align=16
%endif
rounder:	dq 0x0000400000004000
addi:		dq 0x0080008000800080
allones:	dq 0xFFFFFFFFFFFFFFFF

SECTION .text

cglobal mmx_weigh_planar
;(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch,int rowsize, int height, int weight, int invweight)
; assumes non-zero height
mmx_weigh_planar:
	  mov r10w, [rsp+64]
	  mov [rsp+58], r10w
      movq mm5,[rounder wrt rip]
      pxor mm6,mm6
      ;movq mm7,[weight64]
      pshufw mm7, [rsp+56], 0x44
      mov r10d, [rsp+40]
;      mov esi,[p1]
;      mov edi,[p2]
      mov r11d, [rsp+48]  ;// Height
.yloopback:
      lea rax, [r10-4]
.testloop:
      punpcklbw mm0,[rcx+rax]  ;// 4 pixels
       pxor mm3,mm3
      punpcklbw mm1,[rdx+rax]  ;// y300 y200 y100 y000
       psrlw mm0,8              ;// 00y3 00y2 00y1 00y0
      psrlw mm1,8              ;// 00y3 00y2 00y1 00y0  
       pxor mm2,mm2
      movq mm4,mm1
       punpcklwd mm2,mm0
      punpckhwd mm3,mm0  
       punpcklwd mm4,mm6
      punpckhwd mm1,mm6
       por mm2,mm4
      por mm3,mm1
       pmaddwd mm2,mm7     ;// Multiply pixels by weights.  pixel = img1*weight + img2*invweight (twice)
      pmaddwd mm3,mm7      ;// Stalls 1 cycle (multiply unit stall)
       paddd mm2,mm5       ;// Add rounder
      paddd mm3,mm5
       psrld mm2,15        ;// Shift down, so there is no fraction.
      psrld mm3,15        
      packssdw mm2,mm3
      packuswb mm2,mm6 
      movd [rcx+rax],mm2
      sub rax,4
      jnc .testloop
.outloop:
      add rcx, r8;
      add rdx, r9;
	dec r11d
	jnz .yloopback
.outy:
      emms
      ret

cglobal MMerge_MMX
;(unsigned char *dstp, const unsigned char *srcp,
;        const unsigned char *maskp, const int dst_pitch, const int src_pitch,
;        const int mask_pitch, const int row_size, const int height)
MMerge_MMX:
;    mov     eax,[dstp]
;    mov     ebx,[srcp]
;    mov     ecx,[maskp]
	firstpush rsi
	pushreg rsi
	push rdi
	pushreg rdi
	endprolog
	
    mov     r10d,[rsp+56+16]
    movq    mm7,[addi wrt rip]   ;// sh0dan: Stored in register to avoid having to read from cache.
    pxor    mm6, mm6
    mov r11d, [rsp+64+16]
    mov esi, [rsp+40+16]
    mov edi, [rsp+48+16]
.loopy:
	lea rax, [r10-8]
.loopx:
    movq    mm0, [rcx+rax]
     movq    mm1, [rdx+rax]
    movq    mm2, [r8+rax]
     movq    mm3, mm0
    movq    mm4, mm1
     movq    mm5, mm2  ;// sh0dan: No longer loading & unpacking the same data.

    punpcklbw mm0, mm6
     punpcklbw   mm1, mm6
    punpcklbw mm2, mm6
     punpckhbw   mm3, mm6
    punpckhbw mm4, mm6
     punpckhbw   mm5, mm6

    psubw   mm1, mm0
     psubw     mm4, mm3

    pmullw    mm1, mm2
     pmullw    mm4, mm5

    psllw   mm0, 8
     psllw     mm3, 8

    pxor    mm0, mm7
     pxor    mm3, mm7

    paddw   mm0, mm1
     paddw     mm3, mm4

    psrlw   mm0, 8
     psrlw     mm3, 8

    packuswb  mm0, mm3

    movq    [rcx+rax],mm0

	sub eax, byte 8
    jnc      .loopx

    add     rcx, r9
    add     rdx, rsi
    add     r8, rdi
    dec     r11d

    jnz     .loopy

    emms
    pop rdi
    pop rsi
	ret
	endfunc
	
cglobal mmx_darken_planar
;(BYTE *p1, BYTE *p1U, BYTE *p1V, const BYTE *p2, 
;   const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch,
;    int rowsize, int height)
mmx_darken_planar:
	firstpush rsi
	pushreg rsi
	push rdi
	pushreg rdi
	push rbx
	pushreg rbx
	endprolog
	; rcx = p1
	; rdx = p1U
	; r8 = p1V
	; r9 = p2
	mov r10, [rsp+40]
	; r10 = p2U
	mov r11, [rsp+48]
	; r11 = p2V
	mov esi, [rsp+56]
	; esi = p1_pitch
	mov edi, [rsp+64]
	; edi = p2_pitch
	; eax = rowsize
	mov ebx, [rsp+80]
	; ebx = height
	
	
      movq mm7,[allones wrt rip]
      pxor mm6,mm6
      align 16
.yloopback:
	mov eax, [rsp+72]
	sub eax, byte 8
.testloop:
      movq mm0,[rax+rcx]
      movq mm1,[rax+r9]
      movq mm2, mm1
      psubusb mm2,mm0
      pcmpeqb mm2,mm6
      
      movq mm3,mm2   
      pxor mm2,mm7   ;// Inverted
      pand mm0,mm2
      pand mm1,mm3
      por mm0,mm1
      movq [rax+rcx], mm0

      movq mm4,[rax+rdx]
      movq mm5,[rax+r10]
      pand mm4,mm2
      pand mm5,mm3
      por mm4,mm5
      movq [rax+rdx], mm4

      movq mm0,[rax+r8]
      movq mm1,[rax+r11]
      pand mm0,mm2
      pand mm1,mm3
      por mm0,mm1
      movq [rax+r11], mm0

      sub eax, byte 8
      jnc .testloop
      align 16
.outloop:
	  add rcx, rsi
	  add rdx, rsi
	  add r8, rsi
	  add r9, rdi
	  add r10, rdi
	  add r11, rdi

      dec ebx

      jnz .yloopback
.outy:
      emms
      pop rbx
      pop rdi
      pop rsi
      endfunc

cglobal mmx_lighten_planar
;(BYTE *p1, BYTE *p1U, BYTE *p1V, const BYTE *p2, 
;   const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch,
;   int rowsize, int height) 
mmx_lighten_planar:
	firstpush rsi
	pushreg rsi
	push rdi
	pushreg rdi
	push rbx
	pushreg rbx
	endprolog
	; rcx = p1
	; rdx = p1U
	; r8 = p1V
	; r9 = p2
	mov r10, [rsp+40]
	; r10 = p2U
	mov r11, [rsp+48]
	; r11 = p2V
	mov esi, [rsp+56]
	; esi = p1_pitch
	mov edi, [rsp+64]
	; edi = p2_pitch
	; eax = rowsize
	mov ebx, [rsp+80]
	; ebx = height
	
	
      movq mm7,[allones wrt rip]
      pxor mm6,mm6
      align 16
.yloopback:
	mov eax, [rsp+72]
	sub eax, byte 8
.testloop:
      movq mm0,[rax+rcx]
      movq mm1,[rax+r9]
      movq mm2, mm0
      psubusb mm2,mm1
      pcmpeqb mm2,mm6
      
      movq mm3,mm2   
      pxor mm2,mm7   ;// Inverted
      pand mm0,mm2
      pand mm1,mm3
      por mm0,mm1
      movq [rax+rcx], mm0

      movq mm4,[rax+rdx]
      movq mm5,[rax+r10]
      pand mm4,mm2
      pand mm5,mm3
      por mm4,mm5
      movq [rax+rdx], mm4

      movq mm0,[rax+r8]
      movq mm1,[rax+r11]
      pand mm0,mm2
      pand mm1,mm3
      por mm0,mm1
      movq [rax+r11], mm0

      sub eax, byte 8
      jnc .testloop
      align 16
.outloop:
	  add rcx, rsi
	  add rdx, rsi
	  add r8, rsi
	  add r9, rdi
	  add r10, rdi
	  add r11, rdi

      dec ebx

      jnz .yloopback
.outy:
      emms
      pop rbx
      pop rdi
      pop rsi
      endfunc


