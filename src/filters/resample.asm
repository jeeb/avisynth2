
BITS 64

%include "..\convert\amd64inc.asm"

struc FilteredResizeH
.vftable:						resb 8	; 0
.refcnt:						resb 4  ; 8
								alignb 8  ; 12 (alignment)
.pclip:							resb 8	; 16
.vi_width:						resb 4	; 24
.vi_height:						resb 4  ; 28
.vi_fps_numerator:				resb 4	; 32
.vi_fps_denominator:			resb 4	; 36
.vi_num_frames:					resb 4	; 40
.vi_pixel_type:					resb 4	; 44
.vi_audio_samples_per_second:	resb 4	; 48
.vi_sample_type:				resb 4	; 52
.vi_num_audio_samples:			resb 8	; 56
.vi_nchannels:					resb 4	; 64
.vi_image_type:					resb 4	; 68
.pattern_luma:					resb 8	; 72
.pattern_chroma:				resb 8	; 80
.original_width:				resb 4	; 88
.use_dynamic_code:				resb 4	; 92
.tempY:							resb 8	; 96
.tempUV:						resb 8	; 104
.gen_srcp:						resb 8	; 112
.gen_dstp:						resb 8	; 120
.gen_src_pitch:					resb 4	; 128
.gen_dst_pitch:					resb 4	; 132
.gen_h:							resb 4	; 136
.gen_x:							resb 4	; 140
.gen_temp_destp:				resb 8	; 144
endstruc




;=============================================================================
; Read only data
;=============================================================================

ALIGN 64
%ifdef FORMAT_COFF
SECTION .rodata
%else
SECTION .rodata align=16
%endif

FPround:
	dq	0x0000200000002000
Mask1:
	dq	0x00000000ffff0000
Mask3:
	dq	0x000000000000ffff
Mask00ff:
	dq	0x0000000000ff00ff
Maskffff0000:
	dq	0xffff0000ffff0000
	
SECTION .text

cglobal ?GenerateResizer@FilteredResizeH@@QEAAXHPEAVIScriptEnvironment@@@Z

?GenerateResizer@FilteredResizeH@@QEAAXHPEAVIScriptEnvironment@@@Z:
;	void YV12ResampleY(FilteredResizeH* this, int gen_plane, IScriptEnvironment* env);
	firstpush rbx
	pushreg rbx
	push rsi
	pushreg rsi
	push rdi
	pushreg rdi
	push r12
	pushreg r12
	push r13
	pushreg r13
	push r14
	pushreg r14
	push r15
	pushreg r15
	push rbp
	pushreg rbp
	sub rsp, byte 48
	endprolog
	
	mov r11, [rcx+FilteredResizeH.pattern_luma] ; array
	mov r10d, dword [rcx+FilteredResizeH.vi_height] ; vi_height
	mov eax, dword [rcx+FilteredResizeH.vi_width] ; vi_dst_width
	mov r9d, dword [rcx+FilteredResizeH.original_width] ; vi_src_width
	test dl, 1
	jnz .plane_done
	sar r10d, 1
	sar eax, 1
	sar r9d, 1
	mov r11, [rcx+FilteredResizeH.pattern_chroma] ; array
.plane_done
	mov [rsp], r10d		; vi_height
	mov [rsp+4], eax	; vi_dst_width
	add r9d, byte 15
	mov edx, [r11] ; fir_filter_size
	and r9b, 0xF0 ; round vi_src_width up to the nearest 16-byte boundary
	shl edx, byte 2
	mov [byte rsp+8], r9d	; vi_src_width
	
	add r11, 8

	mov r8, [rcx+FilteredResizeH.tempY] ; tempY
	mov r15, [rcx+FilteredResizeH.gen_srcp] ; gen_srcp
	mov ebp, [rcx+FilteredResizeH.gen_dst_pitch] ; gen_dst_pitch
	mov rdi, [rcx+FilteredResizeH.gen_dstp] ; gen_dstp
	mov esi, [rcx+FilteredResizeH.gen_src_pitch] ; gen_src_pitch
	
	sub ebp, eax
	
	mov [rsp+32], r8   ; store tempY for later use
	mov [rsp+16], rbp   ; store gen_dst_pitch-gen_dst_width for later use
	mov [rsp+40], r11  ; store cur_luma for later use
	mov [rsp+24], rsi   ; store gen_src_pitch for later use
	
	; eax = vi_dst_width
	; ebx = free
	; rcx = free
	; edx = fir_filter_size*4
	; esi = free
	; rdi = gen_dstp
	; rbp = free
	; r8 = tempY
	; r9d = vi_src_width
	; r10 = free
	; r11 = cur_luma
	; r12 = free
	; r13 = free
	; r14 = free
	; r15 =  gen_srcp
	
	movq mm7, [FPround wrt rip]
	pxor xmm5, xmm5

align 16	
.yloop
	; tempY has enough extra space to let us read past the end of the line a bit
	movq xmm0, [r15+r9-16]
	movq xmm1, [r15+r9-8]
	punpcklbw xmm0, xmm5
	punpcklbw xmm1, xmm5
	movdqa [r8+r9*2-32], xmm0
	movdqa [r8+r9*2-16], xmm1
	sub r9d, byte 16
	jnz .yloop

	add r15, [rsp+24]  ; add gen_src_pitch
	
	; rbx, rcx, rsi, rbp, r8, r9, r10, r12, r13, r14 are free
align 16
.xloop
	lea r13, [r11+4*rdx+32]
	mov rbx, [r11]
	mov rcx, [r11+8]
	mov rsi, [r11+2*rdx+16]
	mov rbp, [r11+2*rdx+24]
	mov r8, [r11+4*rdx+32]
	mov r9, [r11+4*rdx+40]
	
	mov r10, [r13+2*rdx+16]
	mov r12, [r13+2*rdx+24]
	lea r13, [r13+2*rdx+16+16]
	pxor mm3, mm3
	pxor mm5, mm5
	pxor mm4, mm4
	pxor mm6, mm6
	add r11, byte 16
	xor r14d, r14d
align 16
.fir_filter_do
	movd mm0, dword [rbx+r14]
	movd mm1, dword [rsi+r14]
	punpckldq mm0, qword [rcx+r14]
	punpckldq mm1, qword [rbp+r14]
	pmaddwd mm0, qword [r11]
	movd mm2, dword [r8+r14]
	pmaddwd mm1, qword [r11+2*rdx+16]
	punpckldq mm2, qword [r9+r14]
	add r11, byte 8
	paddd mm3, mm0
	pmaddwd mm2, qword [r11+4*rdx+32-8]
	movd mm0, dword [r10+r14]
	punpckldq mm0, qword [r12+r14]
	paddd mm4, mm1
	pmaddwd mm0, qword [r13+r14*2]
	add r14d, byte 4
	paddd mm5, mm2
	cmp edx, r14d
	paddd mm6, mm0
	ja .fir_filter_do
	lea r11, [r13+2*rdx]
	paddd mm3, mm7
	paddd mm4, mm7
	psrad mm3, 14
	paddd mm5, mm7
	paddd mm6, mm7
	psrad mm4, 14
	psrad mm5, 14
	psrad mm6, 14
	packssdw mm3, mm4
	sub eax, byte 8
	packssdw mm5, mm6
	packuswb mm3, mm5
	movntq [rdi], mm3
	lea rdi, [rdi+8]
	jz .endx
	cmp eax, byte 8
	jae .xloop
	; there will be 2, 4 or 6 pixels left over
	pxor mm6, mm6
.twoleft
	; two, four or six pixels left
	mov rbx, [r11]
	mov rcx, [r11+8]
	pxor mm3, mm3
	xor esi, esi
align 16
.fir_filter_do_two
	movd mm0, dword [rbx+rsi]
	punpckldq mm0, qword [rcx+rsi]
	pmaddwd mm0, qword [r11+rsi*2+16]
	add esi, byte 4
	paddd mm3, mm0
	cmp edx, esi
	ja .fir_filter_do_two
	sub eax, byte 2
	lea r11, [r11+16+2*rdx]
	jnz .fourleft
	paddd mm3, mm7
	add rdi, byte 2
	psrad mm3, 14
	packssdw mm3, mm6
	packuswb mm3, mm6
	movntq [rdi-2], mm3
	jmp .endx
align 16
.fourleft
	; two or four pixels left, mm3 loaded
	pxor mm4, mm4
	mov rbx, [r11]
	mov rcx, [r11+8]
	xor esi, esi
align 16
.fir_filter_do_four
	movd mm0, dword [rbx+rsi]
	punpckldq mm0, qword [rcx+rsi]
	pmaddwd mm0, qword [r11+rsi*2+16]
	add esi, byte 4
	paddd mm4, mm0
	cmp edx, esi
	ja .fir_filter_do_four
	sub eax, byte 2
	lea r11, [r11+16+2*rdx]
	jnz .sixleft
	paddd mm3, mm7
	add rdi, byte 4
	paddd mm4, mm7
	psrad mm3, 14
	psrad mm4, 14
	packssdw mm3, mm4
	packuswb mm3, mm6
	movntq [rdi-4], mm3
	jmp .endx
align 16
.sixleft
	; two pixels left, mm3 and mm4 loaded
	pxor mm5, mm5
	mov rbx, [r11]
	mov rcx, [r11+8]
	xor esi, esi
align 16
.fir_filter_do_six
	movd mm0, dword [rbx+rsi]
	punpckldq mm0, qword [rcx+rsi]
	pmaddwd mm0, qword [r11+rsi*2+16]
	add esi, byte 4
	paddd mm5, mm0
	cmp edx, esi
	ja .fir_filter_do_six
	paddd mm3, mm7
	add rdi, byte 6
	paddd mm4, mm7
	psrad mm3, 14
	paddd mm5, mm7
	psrad mm4, 14
	psrad mm5, 14
	packssdw mm3, mm4
	packssdw mm5, mm6
	packuswb mm3, mm5
	movntq [rdi-6], mm3
align 16
.endx
	dec dword [rsp] ; dec height
	jz .yend
	mov eax, [rsp+4] ; reload gen_dst_width
	add rdi, [rsp+16] ; move gen_dstp to start of next line
	mov r11, [rsp+40] ; reload cur_luma
	mov r8, [rsp+32] ; tempY
	mov r9d, [rsp+8] ; vi_src_width
	jmp .yloop
.yend
	emms
	add rsp, byte 48
	pop rbp
	pop r15
	pop r14
	pop r13
	pop r12
	pop rdi
	pop rsi
	pop rbx
	ret
	endfunc
	
;void ResampleVertical(int* cur, int fir_filter_size, int* yOfs2, const BYTE* srcp, 
;                      int src_pitch, BYTE* dstp, int dst_pitch, int xloops, int y);
cglobal ResampleVertical
ResampleVertical:
	firstpush rbx
	pushreg rbx
	push rsi
	pushreg rsi
	push rdi
	pushreg rdi
	push r12
	pushreg r12
	push r14
	pushreg r14
	push r15
	pushreg r15
	endprolog
	
	movq mm6, [FPround wrt rip]
	mov r11, [rsp+48+48] ; dstp
	mov edi, [rsp+40+48] ; src_pitch
	pxor mm0, mm0
	mov r12d, [rsp+56+48] ; dst_pitch
	mov r14d, [rsp+64+48] ; xloops
	mov r15d, [rsp+72+48] ; y
	
align 16
.yloop
	mov eax, [rcx] ; eax = *cur
	mov esi, [r8+rax*4] ; yOfs2+eax*4
	add rcx, byte 4
	add rsi, r9 ; rsi += srcp
	xor r10d, r10d
	pxor mm7, mm7
	pxor mm1, mm1
align 16
.xloop
	lea rax, [rsi+r10*4]
	xor ebx, ebx
align 16
.bloop
	movd mm2, [rax]
	movd mm3, [rcx+rbx*4]
	punpcklbw mm2, mm0
	inc ebx
	punpckldq mm3, mm3
	movq mm4, mm2
	punpcklwd mm2, mm0
	add rax, rdi ; src_pitch
	pmaddwd mm2, mm3
	punpckhwd mm4, mm0
	pmaddwd mm4, mm3
	paddd mm1, mm2
	paddd mm7, mm4
	cmp ebx, edx
	jz .out_bloop
; unroll 1	
	movd mm2, [rax]
	movd mm3, [rcx+rbx*4]
	punpcklbw mm2, mm0
	inc ebx
	punpckldq mm3, mm3
	movq mm4, mm2
	punpcklwd mm2, mm0
	add rax, rdi ; src_pitch
	pmaddwd mm2, mm3
	punpckhwd mm4, mm0
	pmaddwd mm4, mm3
	paddd mm1, mm2
	paddd mm7, mm4
	cmp ebx, edx
	jz .out_bloop
; unroll 2
	movd mm2, [rax]
	movd mm3, [rcx+rbx*4]
	punpcklbw mm2, mm0
	inc ebx
	punpckldq mm3, mm3
	movq mm4, mm2
	punpcklwd mm2, mm0
	add rax, rdi ; src_pitch
	pmaddwd mm2, mm3
	punpckhwd mm4, mm0
	pmaddwd mm4, mm3
	paddd mm1, mm2
	paddd mm7, mm4
	cmp ebx, edx
	jz .out_bloop
; unroll 3
	movd mm2, [rax]
	movd mm3, [rcx+rbx*4]
	punpcklbw mm2, mm0
	inc ebx
	punpckldq mm3, mm3
	movq mm4, mm2
	punpcklwd mm2, mm0
	add rax, rdi ; src_pitch
	pmaddwd mm2, mm3
	punpckhwd mm4, mm0
	pmaddwd mm4, mm3
	paddd mm1, mm2
	paddd mm7, mm4
	cmp ebx, edx
	jz .out_bloop
; unroll 4
	movd mm2, [rax]
	movd mm3, [rcx+rbx*4]
	punpcklbw mm2, mm0
	inc ebx
	punpckldq mm3, mm3
	movq mm4, mm2
	punpcklwd mm2, mm0
	add rax, rdi ; src_pitch
	pmaddwd mm2, mm3
	punpckhwd mm4, mm0
	pmaddwd mm4, mm3
	paddd mm1, mm2
	paddd mm7, mm4
	cmp ebx, edx
	jz .out_bloop
; unroll 5
	movd mm2, [rax]
	movd mm3, [rcx+rbx*4]
	punpcklbw mm2, mm0
	inc ebx
	punpckldq mm3, mm3
	movq mm4, mm2
	punpcklwd mm2, mm0
	add rax, rdi ; src_pitch
	pmaddwd mm2, mm3
	punpckhwd mm4, mm0
	pmaddwd mm4, mm3
	paddd mm1, mm2
	paddd mm7, mm4
	cmp ebx, edx
	jz .out_bloop
; unroll 6
	movd mm2, [rax]
	movd mm3, [rcx+rbx*4]
	punpcklbw mm2, mm0
	inc ebx
	punpckldq mm3, mm3
	movq mm4, mm2
	punpcklwd mm2, mm0
	add rax, rdi ; src_pitch
	pmaddwd mm2, mm3
	punpckhwd mm4, mm0
	pmaddwd mm4, mm3
	paddd mm1, mm2
	paddd mm7, mm4
	cmp ebx, edx
	jz .out_bloop
; unroll 7
	movd mm2, [rax]
	movd mm3, [rcx+rbx*4]
	punpcklbw mm2, mm0
	inc ebx
	punpckldq mm3, mm3
	movq mm4, mm2
	punpcklwd mm2, mm0
	add rax, rdi ; src_pitch
	pmaddwd mm2, mm3
	punpckhwd mm4, mm0
	pmaddwd mm4, mm3
	paddd mm1, mm2
	paddd mm7, mm4
	cmp ebx, edx
	jnz .bloop
align 16
.out_bloop
	paddd mm1, mm6
	paddd mm7, mm6
	pslld mm1, 2
	pslld mm7, 2
	packuswb mm1, mm7
	psrlw mm1, 8
	packuswb mm1, mm2
	movd [r11+r10*4], mm1
	pxor mm7, mm7
	inc r10d
	pxor mm1, mm1
	cmp r10d, r14d ; xloops
	jnz .xloop
	add r11, r12 ; dst_pitch
	lea rcx, [rcx+rdx*4] ; cur += fir_filter_size
	dec r15d
	jnz .yloop

	emms
	
	pop r15
	pop r14
	pop r12
	pop rdi
	pop rsi
	pop rbx
	
	ret
	endfunc

cglobal ?YUY2Resize@FilteredResizeH@@QEAAXXZ

?YUY2Resize@FilteredResizeH@@QEAAXXZ:
; void YUY2Resize(FilteredResizeH* this);
	firstpush rbx
	pushreg rbx
	push rsi
	pushreg rsi
	push rdi
	pushreg rdi
	push rbp
	pushreg rbp
	push r12
	pushreg r12
	push r13
	pushreg r13
	push r14
	pushreg r14
	push r15
	pushreg r15
	endprolog

	pxor mm0, mm0
	movq mm7, [Mask00ff wrt rip]
	movq mm6, [FPround wrt rip]
	movq mm5, [Maskffff0000 wrt rip]

	mov r12, [rcx+FilteredResizeH.pattern_luma]
	mov r15, [rcx+FilteredResizeH.pattern_chroma]
	mov r10d, [r12]
	mov r11d, [r15]
	mov r9d, [rcx+FilteredResizeH.vi_width]
	add r12, 8
	add r15, 8
	mov r14d, [rcx+FilteredResizeH.vi_height]
	mov r13d, [rcx+FilteredResizeH.original_width]
	shr r13d, 1
	mov rdx, [rcx+FilteredResizeH.gen_srcp]
	mov r8, [rcx+FilteredResizeH.gen_dstp]
	mov rsi, [rcx+FilteredResizeH.tempY]
	mov rdi, [rcx+FilteredResizeH.tempUV]
	mov eax, [rcx+FilteredResizeH.gen_src_pitch]
	mov [rsp+8+64], rax	; gen_src_pitch
	mov eax, [rcx+FilteredResizeH.gen_dst_pitch]
	mov [rsp+16+64], rax	; gen_dst_pitch
	mov [rcx+FilteredResizeH.gen_x], r13d
	mov [rsp+24+64], r12 ; cur_luma
	mov [rsp+32+64], r15 ; cur_chroma
align 16
.loopy
	dec r13d
	prefetchnta [rdx+r13*4+256]
	movd mm1, [rdx+r13*4]
	movq mm2, mm1
	punpcklbw mm2, mm0
	pand mm1, mm7
	movd [rsi+r13*4], mm1
	psrld mm2, 16
	movq [rdi+r13*8], mm2
	jnz .loopy
	; r13d is 0
align 16
.i_xloopYUV
	mov rax, [r12]
	movq mm1, mm0
	mov rbx, [r12+8]
	movq mm3, mm0
	mov rsi, [r15]
	add r12, 16
	add r15, 16
	xor ebp, ebp
align 16
.i_aloop
	movd mm2, [rax+4*rbp]
	movq mm4, [rsi+8*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm4, [r15+8*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm3, mm4
	paddd mm1, mm2
	cmp ebp, r11d
	jge .aloopY_only
	cmp ebp, r10d
	jge .aloopUV_only

	movd mm2, [rax+4*rbp]
	movq mm4, [rsi+8*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm4, [r15+8*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm3, mm4
	paddd mm1, mm2
	cmp ebp, r11d
	jge .aloopY_only
	cmp ebp, r10d
	jge .aloopUV_only

	movd mm2, [rax+4*rbp]
	movq mm4, [rsi+8*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm4, [r15+8*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm3, mm4
	paddd mm1, mm2
	cmp ebp, r11d
	jge .aloopY_only
	cmp ebp, r10d
	jge .aloopUV_only

	movd mm2, [rax+4*rbp]
	movq mm4, [rsi+8*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm4, [r15+8*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm3, mm4
	paddd mm1, mm2
	cmp ebp, r11d
	jge .aloopY_only
	cmp ebp, r10d
	jge .aloopUV_only

	movd mm2, [rax+4*rbp]
	movq mm4, [rsi+8*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm4, [r15+8*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm3, mm4
	paddd mm1, mm2
	cmp ebp, r11d
	jge .aloopY_only
	cmp ebp, r10d
	jge .aloopUV_only

	movd mm2, [rax+4*rbp]
	movq mm4, [rsi+8*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm4, [r15+8*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm3, mm4
	paddd mm1, mm2
	cmp ebp, r11d
	jge .aloopY_only
	cmp ebp, r10d
	jge .aloopUV_only

	movd mm2, [rax+4*rbp]
	movq mm4, [rsi+8*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm4, [r15+8*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm3, mm4
	paddd mm1, mm2
	cmp ebp, r11d
	jge .aloopY_only
	cmp ebp, r10d
	jge .aloopUV_only

	movd mm2, [rax+4*rbp]
	movq mm4, [rsi+8*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm4, [r15+8*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm3, mm4
	paddd mm1, mm2
	cmp ebp, r11d
	jge .aloopY_only
	cmp ebp, r10d
	jge .aloopUV_only
	jmp .i_aloop
align 16	
.aloopY_only
	cmp ebp, r10d
	jge .out_i_aloop
	movd mm2, [rax+4*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm1, mm2
	cmp ebp, r10d
	jge .out_i_aloop

	movd mm2, [rax+4*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm1, mm2
	cmp ebp, r10d
	jge .out_i_aloop

	movd mm2, [rax+4*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm1, mm2
	cmp ebp, r10d
	jge .out_i_aloop

	movd mm2, [rax+4*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm1, mm2
	cmp ebp, r10d
	jge .out_i_aloop

	movd mm2, [rax+4*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm1, mm2
	cmp ebp, r10d
	jge .out_i_aloop

	movd mm2, [rax+4*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm1, mm2
	cmp ebp, r10d
	jge .out_i_aloop

	movd mm2, [rax+4*rbp]
	punpckldq mm2, [rbx+4*rbp]
	pmaddwd mm2, [r12+8*rbp]
	inc ebp
	paddd mm1, mm2
	jmp .aloopY_only
align 16
.aloopUV_only
	movq mm4, [rsi+8*rbp]
	pmaddwd mm4, [r15+8*rbp]
	inc ebp
	paddd mm3, mm4
	cmp ebp, r11d
	jge .out_i_aloop
	
	movq mm4, [rsi+8*rbp]
	pmaddwd mm4, [r15+8*rbp]
	inc ebp
	paddd mm3, mm4
	cmp ebp, r11d
	jge .out_i_aloop

	movq mm4, [rsi+8*rbp]
	pmaddwd mm4, [r15+8*rbp]
	inc ebp
	paddd mm3, mm4
	cmp ebp, r11d
	jge .out_i_aloop

	movq mm4, [rsi+8*rbp]
	pmaddwd mm4, [r15+8*rbp]
	inc ebp
	paddd mm3, mm4
	cmp ebp, r11d
	jge .out_i_aloop

	movq mm4, [rsi+8*rbp]
	pmaddwd mm4, [r15+8*rbp]
	inc ebp
	paddd mm3, mm4
	cmp ebp, r11d
	jge .out_i_aloop

	movq mm4, [rsi+8*rbp]
	pmaddwd mm4, [r15+8*rbp]
	inc ebp
	paddd mm3, mm4
	cmp ebp, r11d
	jge .out_i_aloop

	movq mm4, [rsi+8*rbp]
	pmaddwd mm4, [r15+8*rbp]
	inc ebp
	paddd mm3, mm4
	cmp ebp, r11d
	jl .aloopUV_only
	
align 16
.out_i_aloop
	paddd mm3, mm6
	paddd mm1, mm6
	add r13d, 2
	pslld mm3, 2
	psrad mm1, 14
	lea r12, [r12+8*r10]
	lea r15, [r15+8*r11]
	pmaxsw mm1, mm0
	pand mm3, mm5
	cmp r13d, r9d
	por mm3, mm1
	packuswb mm3, mm3
	movd [r8+r13*2-4], mm3
	jl .i_xloopYUV
	;otherwise we are at the end of a row
	dec r14d
	jz .endframe
	;otherwise we have another row to process
	mov r13d, [rcx+FilteredResizeH.gen_x]
	add r8, [rsp+16+64]
	add rdx, [rsp+8+64]
	mov rsi, [rcx+FilteredResizeH.tempY]
	mov r12, [rsp+24+64]
	mov r15, [rsp+32+64]
	jmp .loopy
align 16
.endframe
	emms
	
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rdi
	pop rsi
	pop rbx
	
	ret
	endfunc