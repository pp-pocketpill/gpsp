@@
@@	Copyright (C) 2016 Fox Cunning
@@
@@ Permission is hereby granted, free of charge,
@@ to any person obtaining a copy of this software and associated documentation files (the "Software"),
@@ to deal in the Software without restriction, including without limitation the rights to
@@ use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
@@ and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
@@
@@ The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
@@
@@ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
@@ INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
@@ FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
@@ IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
@@ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
@@ OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
@@

	.syntax unified
	.arch armv7-a
	.eabi_attribute 27, 3
	.eabi_attribute 28, 1
	.fpu neon
	.eabi_attribute 23, 1
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4

	.arm
	.text
	.align	4
	.global	neon_scaler16
	.type	neon_scaler16, %function
neon_scaler16:
@ ---- Args ----
@ r0 = void *dst
@ r1 = void *src
	push { r4, r5 }

	mov	r2, #16	@ y counter
	mov	r3, #24	@ x counter
	
.LH:
.LW:
	ldrh r4, [r1], #2
	strh r4, [r0], #2
	
	subs r3, r3, #1
	bne	.LW
	
	mov	r3, #24
	
	subs r2, r2, #1
	bne	.LH

	pop	{ r4, r5 }
	bx	lr
	.size	neon_scaler16, .-neon_scaler16
	.ident	"GCC: (Debian 4.9.2-10) 4.9.2"
	.section	.note.GNU-stack,"",%progbits
