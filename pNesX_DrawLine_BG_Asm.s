! FrNES 0.60 Final
!   Assembly Language PPU Renderer

!r4 - Address of Struct Containing PPU Information
!   - Offset 0 - PPU_Addr
!   -        4 - PPU_PPU_H_Scr_Bit;
!   -        8 - PPU_SCANLINE
!   -        12 - PPU_BG_BASE
!   -        16 - PPU_SPRITE_HEIGHT
!   -        20 - PPU_R0
!   -        24 - PPU_SP_BASE
!r5 - Address of PPUBank Array (16 * 32bits long)
!r6 - Buffer For Rendered BG Palette Indeces - 512 bytes
!r7 - Palette data
	.align       4
	.globl       _pNesX_DrawLine_BG

! r9 - nX
! r10 - nY
! r11 - nY4
! r12 - nYBit
! r13 - nametable index
_pNesX_DrawLine_BG:

!start out by preserving r8-r13
	pref          @r15
	xor           r3, r3

	xor           r1, r1
	xor           r2, r2

	add           #-12, r3
	add           #-10, r2

	add           #-5, r1
	pref          @r4

	mov.l         r8, @-r15
	mov           #0x0C, r0

	mov.l         r9, @-r15
	shll8         r0

	mov.l         r10, @-r15
	mov.l         @r4, r9

	mov.l         r11, @-r15
	mov           r9, r10

	mov.l         r12, @-r15
	mov           r0, r8

	mov.l         r13, @-r15
	mov           r10, r12

	mov.l         r14, @-r15
	mov           r12, r13

	and           r8, r13
	mov           #0x1f, r0

	shld          r2, r13
	and           r0, r9

	add           #8, r13
	shld          r1, r10

	shld          r3, r12
	and           r0, r10

	mov           #7, r0
	mov           r10, r11

	and           r0, r12
	mov           #2, r0

	and           r0, r11
	shll2         r12

	shll          r11
	shll          r12

	mov.l         @(4, r4), r0
	add           #-8, r0
	neg           r0, r0
	shll          r0
	add           r0, r6
	add           #-16, r6

!in this section
!r1 - NameTablePointer
!r2 - ChrDataPointer
!r3 - paloffset
!r8 - Attribute Table Base

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	!start loop setup
!--------------------------loop 1

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup
	bf            .small_fixup

.fixup:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup
	nop

.small_fixup:
	add           #1, r1

.post_fixup:

!loop 2

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup2
	bf            .small_fixup2

.fixup2:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup2
	nop

.small_fixup2:
	add           #1, r1

.post_fixup2:


!loop 3

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup3
	bf            .small_fixup3

.fixup3:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup3
	nop

.small_fixup3:
	add           #1, r1

.post_fixup3:


!loop 4
	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup4
	bf            .small_fixup4

.fixup4:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup4
	nop

.small_fixup4:
	add           #1, r1

.post_fixup4:

!loop 5

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup5
	bf            .small_fixup5

.fixup5:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup5
	nop

.small_fixup5:
	add           #1, r1

.post_fixup5:

!loop 6

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup6
	bf            .small_fixup6

.fixup6:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup6
	nop

.small_fixup6:
	add           #1, r1

.post_fixup6:


!loop 7

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup7
	bf            .small_fixup7

.fixup7:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup7
	nop

.small_fixup7:
	add           #1, r1

.post_fixup7:


!loop 8
	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup8
	bf            .small_fixup8

.fixup8:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup8
	nop

.small_fixup8:
	add           #1, r1

.post_fixup8:

!loop 9

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup9
	bf            .small_fixup9

.fixup9:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup9
	nop

.small_fixup9:
	add           #1, r1

.post_fixup9:

!loop 10

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup10
	bf            .small_fixup10

.fixup10:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup10
	nop

.small_fixup10:
	add           #1, r1

.post_fixup10:


!loop 11

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup11
	bf            .small_fixup11

.fixup11:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup11
	nop

.small_fixup11:
	add           #1, r1

.post_fixup11:


!loop 12
	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup12
	bf            .small_fixup12

.fixup12:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup12
	nop

.small_fixup12:
	add           #1, r1

.post_fixup12:

!loop 13

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup13
	bf            .small_fixup13

.fixup13:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup13
	nop

.small_fixup13:
	add           #1, r1

.post_fixup13:

!loop 14

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup14
	bf            .small_fixup14

.fixup14:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup14
	nop

.small_fixup14:
	add           #1, r1

.post_fixup14:


!loop 15

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup15
	bf            .small_fixup15

.fixup15:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup15
	nop

.small_fixup15:
	add           #1, r1

.post_fixup15:


!loop 16
	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup16
	bf            .small_fixup16

.fixup16:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup16
	nop

.small_fixup16:
	add           #1, r1

.post_fixup16:

!loop 17

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup17
	bf            .small_fixup17

.fixup17:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup17
	nop

.small_fixup17:
	add           #1, r1

.post_fixup17:

!loop 18

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup18
	bf            .small_fixup18

.fixup18:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup18
	nop

.small_fixup18:
	add           #1, r1

.post_fixup18:


!loop 19

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup19
	bf            .small_fixup19

.fixup19:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup19
	nop

.small_fixup19:
	add           #1, r1

.post_fixup19:


!loop 20
	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup20
	bf            .small_fixup20

.fixup20:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup20
	nop

.small_fixup20:
	add           #1, r1

.post_fixup20:

!loop 21

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup21
	bf            .small_fixup21

.fixup21:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup21
	nop

.small_fixup21:
	add           #1, r1

.post_fixup21:

!loop 22

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup22
	bf            .small_fixup22

.fixup22:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup22
	nop

.small_fixup22:
	add           #1, r1

.post_fixup22:


!loop 23

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup23
	bf            .small_fixup23

.fixup23:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup23
	nop

.small_fixup23:
	add           #1, r1

.post_fixup23:


!loop 24
	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup24
	bf            .small_fixup24

.fixup24:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup24
	nop

.small_fixup24:
	add           #1, r1

.post_fixup24:

!loop 25

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup25
	bf            .small_fixup25

.fixup25:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup25
	nop

.small_fixup25:
	add           #1, r1

.post_fixup25:

!loop 26

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup26
	bf            .small_fixup26

.fixup26:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup26
	nop

.small_fixup26:
	add           #1, r1

.post_fixup26:


!loop 27

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup27
	bf            .small_fixup27

.fixup27:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup27
	nop

.small_fixup27:
	add           #1, r1

.post_fixup27:


!loop 28
	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup28
	bf            .small_fixup28

.fixup28:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup28
	nop

.small_fixup28:
	add           #1, r1

.post_fixup28:

!loop 29

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup29
	bf            .small_fixup29

.fixup29:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup29
	nop

.small_fixup29:
	add           #1, r1

.post_fixup29:

!loop 30

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup30
	bf            .small_fixup30

.fixup30:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup30
	nop

.small_fixup30:
	add           #1, r1

.post_fixup30:


!loop 31

	!calculate the character data address - should be in r2 after this
	mov.l         @(12, r4), r2

	mov.b         @r1, r3
	mov           #6, r0
	extu.b        r3, r3
	add           r12, r2
	shld          r0, r3
	add           r3, r2

	!calculate the palette table offset
	mov           r9, r0
	mov           r8, r3
	shlr2         r0
	add           r0, r3

	mov           r9, r0
	mov.b         @r3, r3

	and           #2, r0
	extu.b        r3, r3

	add           r11, r0
	pref          @r2

	!negative is shift to the right
	neg           r0, r0
	shld          r0, r3

	mov           #3, r0
	and           r0, r3
	shll2         r3
	shll          r3
	add           r7, r3

	pref          @r3

	pref          @r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	!start rendering
	mov.b         @r2+, r0
	shll          r0
	add           r3, r0
	mov.w         @r0, r0
	mov.w         r0, @r6
	add           #2, r6

	add           #1, r9
	mov           #0x1F, r0

	and           r9, r0
	cmp/eq        #0, r0

	bt            .fixup31
	bf            .small_fixup31

.fixup31:
	mov           #1, r0
	xor           r9, r9
	xor           r0, r13

	!calculate the nametable address - should be in r1 after this
	mov           r5, r1

	mov           r13, r2
	clrmac

	shll2         r2
	mov           #32, r0

	add           r2, r1
	mulu.w        r0, r10

	pref          @r1
	nop
	sts           macl, r2
	nop
	add           r9, r2
	!we use this value to calculate the attribute base - put it in register 8
	mov.l         @r1, r8
	pref          @r8
	add           r8, r2
	mov           r2, r1

	!calculate the attribute table base
	mov           #0x03, r0
	shll8         r0
	or            #0xc0, r0
	extu.w        r0, r0
	add           r0, r8
	! div 4
	mov           r10, r3
	shlr2         r3
	! mul 8
	shll2         r3
	shll          r3
	add           r3, r8

	bra           .post_fixup31
	nop

.small_fixup31:
	add           #1, r1

.post_fixup31:


!finish off by restoring r8-r14
	mov.l         @r15+, r14
	mov.l         @r15+, r13
	mov.l         @r15+, r12
	mov.l         @r15+, r11
	mov.l         @r15+, r10
	mov.l         @r15+, r9
	rts
	mov.l         @r15+, r8
