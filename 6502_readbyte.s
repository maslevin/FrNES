! read from 6502 address space
.global _read_byte_6502
.align 2
_read_byte_6502:
    ! preserve registers - we could maybe use fewer registers and not have to preserve all of these
    mov.l r8, @-r15
    mov.l r9, @-r15
    mov.l r10, @-r15
    mov.l r11, @-r15
    mov.l r12, @-r15 

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

    mov.w RAM_addr, r0              ! put 6502 main memory pointer into r0

    bt/s .read_MEM                  ! if true, jump to read from 6502 memory routine
    and r8, r6                      ! and the index of the byte we're looking for

    mov r1, r7                      ! copy 0x2000 into r7
    mov r1, r8                      ! copy 0x2000 into r8 - if we got here then we didn't need the ram local mask we loaded above

    cmp/eq r5, r1                   ! compare bank and 0x2000
    add r1, r7                      ! put another 0x2000 into r7 (0x4000)

    bt/s .read_PPU_Registers        ! if true, jump to read from PPU registers
    cmp/eq r5, r7                   ! compare bank and 0x4000

    bt/s .read_APU_IO_Region        ! if true, jump to read from APU or IO registers (but also potentially mapper read)
    add r7, r1                      ! put another 0x4000 into r1 (0x6000)

    mov.l wram_addr, r0             ! speculatively put Work RAM address into r0 
    cmp/eq r5, r1                   ! compare bank and 0x6000

    add r8, r1                      ! put another 0x2000 into r1 (0x8000)
    mov.l ROMBANK0_addr, r2         ! speculatively put ROMBANK0 address into r2

    bt/s .read_WRAM_Region          ! if true, jump to read from Work RAM, if present
    add r6, r0                      ! add the index offset into r0    

    cmp/eq r5, r1                   ! compare bank and 0x8000
    add r6, r2                      ! add the index offset into r2

    mov.l ROMBANK1_addr, r3         ! speculatively put ROMBANK1 address into r3
    add r8, r1                      ! put another 0x2000 into r1 (0xa000)

    bt/s .read_ROMBANK0             ! if true, jump to read from ROMBANK0
    add r6, r3                      ! add the index offset into r3

    ! we know we won't need r4, r5, r6 values from here on so reuse the registers
    mov.l ROMBANK2_addr, r4         ! speculatively put ROMBANK2 address into r4
    cmp/eq r5, r1                   ! compare bank and 0xa000

    bt/s .read_ROMBANK1             ! if true, jump to read from ROMBANK1
    add r6, r4                      ! add the index offset into r4

    mov.l ROMBANK3_addr, r0         ! speculatively load ROMBANK3 pointer into r0
    nop

    add r8, r1                      ! put another 0x2000 into r1 (0xc000)
    add r6, r0                      ! add the index offset into r0

    cmp/eq r5, r1                   ! compare bank and 0xc000
    bt .read_ROMBANK2               ! if false, jump to read from ROMBANK2
    bf .read_ROMBANK3

.align 4
address_bank_mask: .short 0xe000
address_index_mask: .short 0x1fff
ram_local_index_mask: .short 0x07ff

.align 4
RAM_addr: .long 0x7c000000
wram_addr: .long _ROMBANK_WRAM
ROMBANK0_addr: .long _ROMBANK0
ROMBANK1_addr: .long _ROMBANK1
ROMBANK2_addr: .long _ROMBANK2
ROMBANK3_addr: .long _ROMBANK3

! r6 - index inside the bank
! r8 - local mask 0x07ff
.align 4
.read_MEM:
    bra .return_to_caller
    mov.b @(r0, r6), r0             ! read the byte we're looking for to r0

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

    bt/s .read_PPUDATA              ! branch to read 2007 if true
    mov.w @r9, r3                   ! speculatively load PPUADDR's value into r3

    mov.l OAMADDR_addr, r9          ! speculatively put the address of OAMADDR into r9
    cmp/eq r0, r2                   ! test if the address is reading PPU Register 2004

    and #2, r0                      ! mask if the 2 bit is set on r0 
    mov.l SPRRAM_addr, r10          ! speculatively put the addresss of SPRRAM into r10

    bt/s .read_OAMDATA              ! branch to read 2004 if true    
    mov.b @r9, r3                   ! speculatively load OAMADDR's value into r3

    mov.l PPUSTATUS_addr, r9        ! speculatively put the address of PPUSTATUS into r9
    cmp/eq #2, r0                   ! test if the address has the 2 bit set post masking

    shlr8 r4                        ! if we got here, r4 still has the read address here, so shift it right by 8 bits because it's likely to be the return value at this point
    nop

    mov.b @r9, r3                   ! speculatively move the value of PPUSTATUS into r3
    bt/s .read_PPUSTATUS            ! branch to read 2002 if true
    mov r4, r0                      ! move the address to the return slot

    bra .return_to_caller
    nop

.align 4
SPRRAM_addr: .long 0x7c000a00

! r3 - contains PPUSTATUS
! r7 - addr of open bus value
! r8 - contains value of open bus
! r9 - contains address of PPUSTATUS
.align 4
.read_PPUSTATUS:
    mov.l PPU_W_addr, r2            ! move the internal W register address into r2
    xor r4, r4                      ! clear r4 out

    mov #31, r5                     ! move 0x1f into r5 to mask the value in r8
    mov r3, r0                      ! copy PPUSTATUS value into r0 

    mov.b @r2, r4                   ! set the W register value to 0
    and r5, r8                      ! and Open Bus value with 0x1f

    mov #127, r2                    ! put the vblank off mask into r2
    nop

    and #224, r0                    ! and PPUSTATUS value with 0xe0
    nop

    and r2, r3                      ! and PPUSTATUS value (to be stored) with 0x7f, turning off vblank flag
    nop

    or r8, r0                       ! or r0 with masked open bus value from r8
    mov.b @r9, r3                   ! store PPUSTATUS value back to memory

    bra .return_to_caller
    mov.b @r7, r0                   ! store the return value as the open bus value

! r3 - contains OAMADDR
! r7 - addr of the open bus value
! r8 - contains value of open bus
! r9 - contains address of OAMADDR
! r10 - contains address of SPRRAM
.align 4
.read_OAMDATA:
    add r3, r10                     ! increment the SPRRAM pointer by the offset of OAMADDR
    nop

    mov.b @r10, r0                  ! get the byte at the SPRRAM pointer to r0
    nop

    bra .return_to_caller
    mov.b r0, @r7                   ! store this value in the open bus

! r3 - contains value of PPUADDR
! r6 - index inside the bank
! r7 - addr of the open bus value
! r8 - contains value of open bus
! r9 - contains address of PPUADDR
! TODO: this branch is not pipeline unoptimized yet
.align 4
.read_PPUDATA:
    mov.l PPUCTRL_addr, r5          ! we will need to get the PPU increment so for that we'll need PPUCTRL
    mov.b @r5, r0                   ! load the PPUCTRL register value into r0 
    and #4, r0                      ! and 0x4 to r0, if zero, PPU increment is 1, if 1 PPU increment is 32
    cmp/eq #0, r0                   ! compare r0 to 0
    bf .increment_32                ! if r0 isn't zero, that means that we need to incrmeent PPUADDR by 32
    add #1, r3                      ! otherwise just increment PPUADDR by 1
    bra .post_increment

.align 4
.increment_32:
    add #32, r3 
!purposeful fallthrough to post_increment

.align 4
.post_increment:
    mov.w r3, @r9                    ! store the value of PPUADDR back to its memory location
    mov.l PPUDATA_addr, r4           ! move the address of PPUDATA into r4
    mov.b @r4, r0                    ! move the value of PPUDATA into r0
    mov r0, @r7                      ! set the open bus value to the return value

    mov r3, r10                      ! copy value of PPUADDR to r10 
    mov #3, r11                      ! start building the PPU bank mask (0x3ff) in r11
    shll8 r11                        ! shift r11 left by 8 bits
    mov #255, r12                    ! put 0xFF into r12
    or r12, r11                      ! finish the PPU bank mask
    and r11, r10                     ! and the mask into r10, to get the index of the byte in the bank we want

    shlr8 r3                         ! shift r3 right by 10 bits as an 8 and then a 2 to get the index of the bank we want
    shlr2 r3                         ! (continued) 

    mov.l PPUBANK_addr, r5           ! move the address of PPUBANK into r5 
    shll2 r3                         ! multiply r3 by 4 by shifting left twice, to equal the number of bytes from PPUBANK to move forward to get the address of bank pointer we want
    add r3, r5                       ! add the number of bytes from PPUBANK to get the bank pointer we want
    pref @r5                         ! prefetch the values of the PPUBANK array - this would be unnecessary if we put the PPUBANK into on-die cache... TODO
    mov.l @r5, r6                    ! move the value of the bank pointer to r6

    add r10, r6                      ! add the index of the byte we want to the bank pointer to get the address of the byte we want
    pref @r6                         ! prefetch the area with the byte we want
    mov.b @r6, r2                    ! move the value from the final byte address to r2
    mov.b r2, @r4                    ! store the value of PPUDATA back to its memory location

    bra .return_to_caller
    nop

.align 4
PPUBANK_addr: .long _PPUBANK

.align 4
.read_APU_IO_Region:

! TODO: we could probably optimize all of these bank reading conditions so that we bank select the correct pointer, prefetch it, read it and return
! r0 - wram pointer
.align 4
.read_WRAM_Region:
    pref @r0
    nop
    bra .return_to_caller
    mov.b @r0, r0    

! r2 - ROMBANK0 pointer
.align 4
.read_ROMBANK0:
    pref @r2
    nop
    bra .return_to_caller
    mov.b @r2, r0

! r3 - ROMBANK1 pointer 
.align 4
.read_ROMBANK1:
    pref @r3
    nop
    bra .return_to_caller    
    mov.b @r3, r0

! r4 - ROMBANK 2 pointer
.align 4
.read_ROMBANK2:
    pref @r4
    nop
    bra .return_to_caller    
    mov.b @r4, r0

! r0 - ROMBANK 3 pointer
.align 4
.read_ROMBANK3:                     
    pref @r0
    nop
    bra .return_to_caller    
    mov.b @r0, r0

.align 4
.return_to_caller:
    ! restore registers
    mov.l @r15+, r12
    mov.l @r15+, r11
    mov.l @r15+, r10
    mov.l @r15+, r9
    mov.l @r15+, r8

    rts

! PPU register locations
.align 4
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
