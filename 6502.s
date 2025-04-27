.equ FLAG_C, 0x01
.equ FLAG_Z, 0x02
.equ FLAG_I, 0x04
.equ FLAG_D, 0x08
.equ FLAG_B, 0x10
.equ FLAG_R, 0x20
.equ FLAG_V, 0x40
.equ FLAG_N, 0x80

.equ BASE_STACK, 0x100

.equ VECTOR_NMI, 0xfffa
.equ VECTOR_RESET, 0xfffc
.equ VECTOR_IRQ, 0xfffe

! read from 6502 address space
.align 2
_read_byte_6502:
    ! 6502 read address is in r4
    mov.w address_bank_mask, r0     ! put 0xe000 into r0
    mov #32, r1                     ! put 0x2000 into r1 

    mov.w address_index_mask, r2    ! put 0x1fff into r2
    xor r7, r7                      ! clear r7

    mov r4, r5                      ! put a copy of the address into r5
    shll8 r1                        ! shift left to complete r1

    mov r4, r5                      ! put a copy of the address into r5
    mov r4, r6                      ! put a copy of the address into r6

    and r0, r5                      ! and r5 with the address mask to get the bank to switch options    
    mov.w ram_local_index_mask, r8  ! put the ram local mask into r8 just in case we need it

    and r2, r6                      ! and r6 with the index mask to get the index inside the bank we are interested in 
    cmp/eq r5, r7                   ! compare bank and 0x0000

    mov.w .RAM_addr, r0             ! put 6502 main memory pointer into r0

    bt/s read_MEM                   ! if true, jump to read from 6502 memory routine
    and r8, r6                      ! and the index of the byte we're looking for

    mov r1, r7                      ! copy 0x2000 into r7
    mov r1, r8                      ! copy 0x2000 into r8 - if we got here then we didn't need the ram local mask we loaded above

    cmp/eq r5, r1                   ! compare bank and 0x2000
    add r1, r7                      ! put another 0x2000 into r7 (0x4000)

    bt/s read_PPU_Registers         ! if true, jump to read from PPU registers
    cmp/eq r5, r7                   ! compare bank and 0x4000

    bt/s read_APU_IO_Region         ! if true, jump to read from APU or IO registers (but also potentially mapper read)
    add r7, r1                      ! put another 0x4000 into r1 (0x6000)

    mov.l wram_addr, r0             ! speculatively put Work RAM address into r0 
    cmp/eq r5, r1                   ! compare bank and 0x6000

    add r8, r1                      ! put another 0x2000 into r1 (0x8000)
    mov.l ROMBANK0_addr, r2         ! speculatively put ROMBANK0 address into r2

    bt/s read_WRAM_Region           ! if true, jump to read from Work RAM, if present
    add r6, r0                      ! add the index offset into r0    

    cmp/eq r5, r1                   ! compare bank and 0x8000
    add r6, r2                      ! add the index offset into r2

    mov.l ROMBANK1_addr, r3         ! speculatively put ROMBANK1 address into r3
    add r8, r1                      ! put another 0x2000 into r1 (0xa000)

    bt/s read_ROMBANK0              ! if true, jump to read from ROMBANK0
    add r6, r3                      ! add the index offset into r3

    ! we know we won't need r4, r5, r6 values from here on so reuse the registers
    mov.l ROMBANK2_addr, r4         ! speculatively put ROMBANK2 address into r4
    cmp/eq r5, r1                   ! compare bank and 0xa000

    bt/s read_ROMBANK1              ! if true, jump to read from ROMBANK1
    add r6, r4                      ! add the index offset into r4

    mov.l ROMBANK3_addr, r0         ! speculatively load ROMBANK3 pointer into r0
    nop

    add r8, r1                      ! put another 0x2000 into r1 (0xc000)
    add r6, r0                      ! add the index offset into r0

    cmp/eq r5, r1                   ! compare bank and 0xc000
    bt read_ROMBANK2                ! if false, jump to read from ROMBANK2

! r0 - byte address
.align 4
.read_ROMBANK3:                     ! otherwise, we are definitively in ROMBANK3
    pref @r0
    nop
    mov.b @r0, r0
    rts

! r6 - index inside the bank
! r8 - local mask 0x07ff
.align 4
.read_MEM:
    mov.b @(r0, r6), r0             ! read the byte we're looking for to r0
    nop
    rts                             ! return to caller

! r6 - index inside the bank
.align 4
.read_PPU_Registers:
    mov r6, r0                      ! move a copy of the index into r0
    mov #7, r2                      ! put 7 into r2 

    and #15, r0                     ! and 0xf with r0 for further masking the index into PPU register range
    mov.l PPUOPENBUS_addr, r7       ! put the address of the open bus variable into r7

    cmp/eq r0, r2                   ! test if the address is reading PPU Register 2007
    mov.l PPUADDR_addr, r9          ! speculatively put the addresss of PPUADDR into r9

    mov #4, r2                      ! put 4 into r2 
    mov.b @r7, r8                   ! put the value of the open bus into r8

    bt/s read_PPUDATA               ! branch to read 2007 if true
    mov.b @r9, r3                   ! speculatively load PPUADDR's value into r3

    mov.l OAMADDR_addr, r9           ! speculatively put the address of OAMADDR into r9
    cmp/eq r0, r2                   ! test if the address is reading PPU Register 2004

    and #2, r0                      ! mask if the 2 bit is set on r0 
    mov.l SPRRAM_addr, r10          ! speculatively put the addresss of SPRRAM into r10

    bt/s read_OAMDATA               ! branch to read 2004 if true    
    mov.b @r9, r3                   ! speculatively load OAMADDR's value into r3

    mov.l PPUSTATUS_addr, r9        ! speculatively put the address of PPUSTATUS into r9
    cmp/eq #2, r0                   ! test if the address has the 2 bit set post masking

    shlr8 r4                        ! if we got here, r4 still has the read address here, so shift it right by 8 bits because it's likely to be the return value at this point
    nop

    mov.b @r9, r3                   ! speculatively move the value of PPUSTATUS into r3
    bt/s read_PPUSTATUS             ! branch to read 2002 if true
    mov r4, r0                      ! move the address to the return slot

    nop
    rts                             ! then return

! r3 - contains PPUSTATUS
! r7 - addr of open bus value
! r8 - contains value of open bus
! r9 - contains address of PPUSTATUS
.align 4
.read_PPUSTATUS
    mov.l PPU_W_addr, r2            ! move the internal W register address into r2
    xor r4, r4                      ! clear r4 out

    mov #31, r5                     ! move 0x1f into r5 to mask the value in r8
    mov r3, r0                      ! copy PPUSTATUS value into r0 

    mov.b @r2, r4                   ! set the W register value to 0
    and r5, r8                      ! and Open Bus value with 0x1f

    and #224, r0                    ! and PPUSTATUS value with 0xe0
    mov #127, r2                    ! put the vblank off mask into r2

    and r2, r3                      ! and PPUSTATUS value (to be stored) with 0x7f, turning off vblank flag
    or r8, r0                       ! or r0 with masked open bus value from r8

    mov.b @r7, r0                   ! store the return value as the open bus value
    rts

! r3 - contains SPRRAM
.align 4
.read_OAMDATA:

! r3 - contains PPUADDR
.align 4
.read_PPUDATA:
    mov.b PPUCTRL_addr, r4          ! we will need to get the PPU increment so for that we'll need PPUCTRL


    

! r0 - wram pointer
.align 4
.read_WRAM_Region:
    pref @r0
    nop
    mov.b @r0, r0
    nop
    rts

.align 4
.read_ROMBANK0:

.align 4
.read_ROMBANK1:

.align 4
.read_ROMBANK2:

    rts
    mov #0, 0

.align 4
address_bank_mask: .short 0xe000
address_index_mask: .short 0x1fff
ram_local_index_mask: .short 0x07ff
RAM_addr: .long 0x7c000000
wram_addr: .long _ROMBANK_WRAM
ROMBANK0_addr: .long _ROMBANK0
ROMBANK1_addr: .long _ROMBANK1
ROMBANK2_addr: .long _ROMBANK2
ROMBANK3_addr: .long _ROMBANK3

.global _reset_6502
.align 2
_reset_6502:
    ! set PC to reset vector
    mov.l PC_addr, r1        ! put the target address of the PC variable into r1
    mov.w r0, @r1            ! init PC to 0 - #TODO: read from reset vector

    ! set A, X, Y to zero
    mov   #0, r0             ! put sign extended 0 into r0    
    mov.l A_addr, r1         ! put the target address of the A variable into r1
    mov.b r0, @r1            ! init A to 0
    mov.l X_addr, r1         ! put the target address of the X variable into r1
    mov.b r0, @r1            ! init X to 0
    mov.l Y_addr, r1         ! put the target address of the Y variable into r1
    mov.b r0, @r1            ! init Y to 0

    mov #(FLAG_Z | FLAG_R), r0 ! set the initial status register state of Z and R flags set
    mov.l S_addr, r1         ! put the target address of the S variable into r1
    mov.b r0, @r1            ! init S to Z + R flags set

    mov #0xff, r0            ! put sign extended 255 into r0
    mov.l SP_addr, r1        ! put the target address of the SP variable into r1
    mov.b r0, @r1            ! init SP to 0

    ! returning to caller
    rts
    mov #0, r0

.global _emulate_6502
.align 2
_emulate_6502

.align 4
PC_addr: .long 0x7c000b20
SP_addr: .long 0x7c000b22
S_addr: .long 0x7c000b23
A_addr: .long 0x7c000b24
X_addr: .long 0x7c000b25
Y_addr: .long 0x7c000b26

IRQ_REQ_addr: .long 0x7c000b27 ! when nonzero, an IRQ has been requested
NMI_REQ_addr: .long 0x7c000b28 ! when nonzero, an NMI has been requested

CLOCKS_addr: .long 0x7c000b30  ! clocks stored after 

! PPU register locations
PPUCTRL_addr: .long 0x7c000b40
PPUMASK_addr: .long 0x7c000b41
PPUSTATUS_addr: .long 0x7c000b42
OAMADDR_addr: .long 0x7c000b43
OAMDATA_addr: .long 0x7c000b44
OAMDMA_addr: .long 0x7c000b45
PPUDATA_addr: .long 0x7c000b46
PPUOPENBUS_addr: .long 0x7c000b47
PPUSCROLL_addr: .long 0x7c000b48  ! word
PPUADDR_addr: .long 0x7c000b4a    ! word
PPU_V_addr: .long 0x7c000b4c
PPU_T_addr: .long 0x7c000b4d
PPU_X_addr: .long 0x7c000b4e
PPU_W_addr: .long 0x7c000b4f
