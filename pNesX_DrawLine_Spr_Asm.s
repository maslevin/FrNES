! FrNES 0.60 Final
!   Assembly Language PPU Renderer

!r4 - Address of Struct Containing PPU Information
 
!r5 - Address of sprite Ram
!r6 - Address of character Ram
!r7 - Sprite Buffer (264 Bytes)
!
	.align       4
	.globl       _pNesX_DrawLine_Spr

! r8 - Spram Iterator
! r9 - nX
! r10 - nY
! r11 - nAttr
! r12 - nYBit
! r13 - Sprite Counter 
_pNesX_DrawLine_Spr:
	!preservation
	mov.l         r8, @-r15
	mov.l         r9, @-r15
	mov.l         r10, @-r15
	mov.l         r11, @-r15
	mov.l         r12, @-r15
	mov.l         r13, @-r15
	mov.l         r14, @-r15
	pref          @r4

	!Start SP Pointer / Iterator
	mov           r5, r8
	mov           #63, r0
	shll2         r0
	add           r0, r8
	pref          @r8

	!Clear Sprite Count
	mov           #0, r0
	mov           r0, r13

	!PPU_Scanline in r3
	mov.l         @(8, r4), r3

	!PPU_SP_Height in r2
	mov.l         @(16, r4), r2

.start_spr_render_loop:
	mov.b         @r8, r10
	extu.b        r10, r10

	!if nY > PPU_Scanline
	cmp/hi        r3, r10
	bt            .first_condition

	! or if nY + PPU_SP_Height <= PPU_Scanline
	mov           r10, r0
	add           r2, r0
	cmp/hs        r0, r3
	bt            .first_condition

	!if nSprCnt == 0
	mov           r13, r0
	cmp/eq        #0, r0

	bt            .memclear
	bf            .post_memclear

.memclear:
	mov           #68, r0
	mov           r0, r1
	mov           r7, r14
	mov           #0, r0

.start_clear:
	pref          @r14
	nop
	dt            r1
	mov.l         r0, @r14
	add           #4, r14

	bf            .start_clear

.post_memclear:
	
	! nSprCnt++
	pref @r0
	add           #1, r13

	! nAttr in r11
	mov           r8, r0
	add           #2, r0
	mov.b         @r0, r11

	! nYBit in r12
	mov           r3, r12
	neg           r10, r0
	add           r0, r12

	! if nAttr & spr_vflip
	mov           r11, r0
	and           #0x80, r0
	cmp/eq        #0x00, r0
	bf            .vflip_correction1
	bt            .vflip_correction2

.vflip_correction1:
	mov           r2, r0
	neg           r12, r1
	add           #-1, r0
	pref          @r4
	add           r1, r0
	mov           r0, r12

.vflip_correction2:
	shll2         r12
	shll          r12

.post_vflip_correction:

	!using r10 because we've got it to trash from here out...
    mov           r8, r0
	add           #1, r0
	mov.b         @r0, r10

	!ppu register 0 into r0
	mov.l         @(20, r4), r0
	extu.b        r10, r10
	and           #0x20, r0
	cmp/eq        #0x00, r0
	bf            .sprite_size_16
	bt            .sprite_size_8

.sprite_size_16:
	mov           r10, r0
	and           #1, r0
	cmp/eq        #0x00, r0
	mov           r6, r14
	bf            .firstcalc
	bt            .secondcalc

.firstcalc:
	mov           #0x40, r0
	shll8         r0
	add           r0, r14

.secondcalc:
	mov           r10, r0
	and           #0xfe, r0
	shll2         r0
	add           r12, r14
	shll2         r0
	shll2         r0
	add           r0, r14

	bra           .post_sprite_size
	nop

.sprite_size_8:
	mov.l         @(24, r4), r14
	mov           r10, r0
	shll2         r0
	add           r12, r14
	shll2         r0
	shll2         r0
	add           r0, r14


.post_sprite_size:
	mov           #0x20, r0
	extu.b        r0, r0
	xor           r0, r11
	
!nY and nYBit are no longer in use so we can use regs r10 and r12 without preserving them
!nY will now be bySprCol - r10
!nYBit will now be pSprBuf[nX]
	mov           #0x23, r0
    mov           r11, r10
	and           r0, r10
	shll2         r10

	!nX is now in r9
	mov           r8, r0
	add           #3, r0
	mov.b         @r0, r9

	!pSprBuf[nX] will now be in r12
	mov           r7, r12
	extu.b        r9, r9
	add           r9, r12

	pref          @r14
	mov           r11, r0
	and           #0x40, r0
	cmp/eq        #0x00, r0
	bf            .horizontal_flip
	bt            .non_flip

.horizontal_flip:
	add           #7, r14

.hpixel1:
	mov.b         @r14, r0
	pref          @r12
	cmp/eq        #0, r0
	add           #-1, r14
	bt            .hpixel2

	or            r10, r0
	mov.b         r0, @r12

.hpixel2:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #-1, r14
	bt            .hpixel3

	or            r10, r0
	mov.b         r0, @r12

.hpixel3:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #-1, r14
	bt            .hpixel4

	or            r10, r0
	mov.b         r0, @r12

.hpixel4:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #-1, r14
	bt            .hpixel5

	or            r10, r0
	mov.b         r0, @r12

.hpixel5:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #-1, r14
	bt            .hpixel6

	or            r10, r0
	mov.b         r0, @r12

.hpixel6:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #-1, r14
	bt            .hpixel7

	or            r10, r0
	mov.b         r0, @r12

.hpixel7:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #-1, r14
	bt            .hpixel8

	or            r10, r0
	mov.b         r0, @r12

.hpixel8:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #-1, r14
	bt            .post_flip

	or            r10, r0
	mov.b         r0, @r12

	bra           .post_flip
	nop

.non_flip:

.pixel1:
	mov.b         @r14, r0
	pref          @r12
	cmp/eq        #0, r0
	add           #1, r14
	bt            .pixel2

	or            r10, r0
	mov.b         r0, @r12

.pixel2:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #1, r14
	bt            .pixel3

	or            r10, r0
	mov.b         r0, @r12

.pixel3:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #1, r14
	bt            .pixel4

	or            r10, r0
	mov.b         r0, @r12

.pixel4:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #1, r14
	bt            .pixel5

	or            r10, r0
	mov.b         r0, @r12

.pixel5:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #1, r14
	bt            .pixel6

	or            r10, r0
	mov.b         r0, @r12

.pixel6:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #1, r14
	bt            .pixel7

	or            r10, r0
	mov.b         r0, @r12

.pixel7:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #1, r14
	bt            .pixel8

	or            r10, r0
	mov.b         r0, @r12

.pixel8:
	add           #1, r12
	mov.b         @r14, r0
	cmp/eq        #0, r0
	add           #1, r14
	bt            .post_flip

	or            r10, r0
	mov.b         r0, @r12

.post_flip:


!if pSprPtr (r8) >= sprite base (r5)
.first_condition:
	add           #-4, r8
	pref          @r8
	cmp/hs        r5, r8
	bt            .start_spr_render_loop
	bf            .finish_spr_render_loop

!and the sprite count is less than 8
.second_condition:
	mov           #8, r0
	cmp/hi        r13, r0
	bt            .start_spr_render_loop

.finish_spr_render_loop:

	!return the number of sprites found
    mov           r13, r0

	mov.l         @r15+, r14
	mov.l         @r15+, r13
	mov.l         @r15+, r12
	mov.l         @r15+, r11
	mov.l         @r15+, r10
	mov.l         @r15+, r9
	rts
	mov.l         @r15+, r8
