! read from 6502 address space
! r4 - unsigned short int address in 6502 space to read
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
    ! MS - the 0xe000 AND mask is not necessary, just shift 13 right 
    mov r4, r0                      ! put a copy of the address into r0 then shift right 13 bits to get page index 0-7 in r0
    nop

    mov r4, r6                      ! put another copy of the address into r6
    shlr8 r0                        !  (shift 8)

    mov.w address_index_mask, r2    ! put 0x1fff into r2
    shlr2 r0                        !  (shift 2)

    mov.w ram_local_index_mask, r3  ! put 0x07ff into r3
    shlr2 r0                        !  (shift 2)

    mov.l ROMBANKS_addr, r5         ! move address of RAM_addr (the below list of address pointers) into r5
    shlr r0                         !  (shift 1)

    mov r4, r7                      ! put another copy of the address into r7
    cmp/eq #0, r0                   ! compare r0 to 0 - if true we are reading from the 6502 RAM

    mov.l @r5, r9                   ! put the ROMBANKS[0] into r9
    and r2, r6                      ! and the 0x1fff mask with the address to get the bank index

    and r3, r7                      ! mask r7 with 0x7ff

    bt/s .read_MEM                  ! branch to read_MEM if we are on page 0
    add r9, r7                      ! add RAM_addr to r7

    cmp/eq #1, r0                   ! compare r0 to 1 - if true we are reading from the PPU registers

    bt/s .read_PPU_Registers        ! branch to read_PPU_Registers if we are on page 1

    cmp/eq #2, r0                   ! compare r0 to 2 - if true we are reading from the APU and IO registers

    bt/s .read_APU_IO_Region        ! branch to read_APU_IO_Region if we are on page 2 

    shll2 r0                        ! multiply this index by 4 by shifting left twice - this is the number of bytes from the RAM_addr address of the next memory read_WRAM_Region
    nop

    add r0, r5                      ! increment the bank pointer by r0, this becomes the bank pointer we are reading from 
    nop

    mov.l @r5, r2                   ! load the bank pointer value into r2
    nop
    nop
    nop

    add r6, r2                      ! add the bank index into r2
    nop

    pref @r2                        ! prefetch the byte address we are trying to read
    nop

    bra .return_to_caller           ! return to caller 
    mov.b @r2, r0                   ! move the value at r2 into r0 

.align 4
address_bank_mask: .short 0xe000
address_index_mask: .short 0x1fff
ram_local_index_mask: .short 0x07ff
ppu_local_index_mask: .short 0x03ff

! the arrangement of these labels matters because we load RAM_addr to calculate which buffer we are going to read from
.align 4
ROMBANKS_addr: .long 0x7c000b50

! r7 - address of byte in 6502 memory we are going to read
.align 4
.read_MEM:
    bra .return_to_caller
    mov.b @r7, r0             ! read the byte we're looking for to r0

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
    mov.b r0, @r7                   ! store the return value as the open bus value

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
    nop

    bra .return_to_caller
    mov.b r0, @r7                   ! store this value in the open bus

! r3 - contains value of PPUADDR
! r6 - index inside the bank
! r7 - addr of the open bus value
! r8 - contains value of open bus
! r9 - contains address of PPUADDR
! TODO: this branch is not pipeline unoptimized yet
!   essentially, I think to optimize PPUDATA reading here, we need to integrate or intersperse the two parts of this algorithm
.align 4
.read_PPUDATA:
    mov.l PPUCTRL_addr, r5          ! we will need to get the PPU increment so for that we'll need PPUCTRL
    mov.b @r5, r0                   ! load the PPUCTRL register value into r0 
    and #4, r0                      ! and 0x4 to r0, if zero, PPU increment is 1, if 1 PPU increment is 32
    cmp/eq #0, r0                   ! compare r0 to 0
    bf .increment_32                ! if r0 isn't zero, that means that we need to incrmeent PPUADDR by 32
    add #1, r3                      ! otherwise just increment PPUADDR by 1
    bra .post_PPUADDR_increment

.align 4
.increment_32:
    add #32, r3 
!purposeful fallthrough to post_PPUADDR_increment

.align 4
.post_PPUADDR_increment:
    mov.w r3, @r9                    ! store the value of PPUADDR back to its memory location
    mov.l PPUDATA_addr, r4           ! move the address of PPUDATA into r4
    mov.b @r4, r0                    ! move the value of PPUDATA into r0
    mov.b r0, @r7                    ! set the open bus value to the return value

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

! TODO: this branch is not pipeline unoptimized yet
.align 4
.read_APU_IO_Region:
    mov r4, r0                  ! move copy of the requested address into r0
    and #255, r0                ! mask out the least significant bit of the address
    cmp/eq #22, r0              ! check to see if the address is 0x4016 
    mov #21, r1                 ! move 0x15 into r1, to compare later if r0 is an audio register (0x4000 - 0x4015)
    bt/s .read_joypad_1         ! if the address was 0x4016, branch to the joypad 1 read routine 
    cmp/eq #23, r0              ! check to see if the address is 0x4017
    bt/s .read_joypad_2         ! if the address was 0x4017, branch to the joypad 2 read routine
    cmp/hs r0, r1               ! check to see if the address is less than or equal to 0x4015
    bt/s .read_audio            ! if the address was less than 0x4015, go to the audio read routine 
    nop 

    bra .read_mapper            ! otherwise, branch to the mapper read routine
    nop 

.align 4
.read_joypad_1:
    mov.l PAD1Latch_addr, r0    ! move the address of the PAD1 Latch variable into r0
    bra .read_joypad_routine
    mov.l PAD1Bit_addr, r1      ! move the address of the PAD1 Bit variable into r1

.align 4
.read_joypad_2:
    mov.l PAD2Latch_addr, r0    ! move the address of the PAD2 Latch variable into r0
    mov.l PAD2Bit_addr, r1      ! move the address of the PAD2 Bit variable into r1
    ! intentional fallthrough to read_joypad_routine

.align 4
.read_joypad_routine:
    mov.b @r0, r0               ! move the value of Latch into r0
    mov #1, r6                  ! move 1 into r6

    mov.b @r1, r2               ! move the value of Bit into r2 
    neg r1, r3                  ! convert the value of Bit into a negative shift value in r3

    shld r3, r0                 ! shift r0 to the right by Bit bits in r3
    nop

    and r6, r0                  ! mask out the 1's place of r0 
    nop

    mov #23, r3                 ! move 23 into r3 so that we can compare it with Bit later
    nop

    or #64, r0                  ! or r0 with 0x40
    cmp/eq r3, r2               ! compare if Bit's value is 23

    xor r5, r5                  ! clear out r5

    bt/s .reset_pad_bit         ! if Bit's value is 23 go to the reset pad bit routine
    add r6, r2                  ! add 1 more into Bit

    bra .return_to_caller
    mov.b r2, @r1               ! move the updated Bit value back into it's memory location 

.align 4
.reset_pad_bit:
    bra .return_to_caller
    mov.b r5, @r1               ! move the updated Bit value back into it's memory location     

.align 4
PAD1Latch_addr: .long 0x7c000b70 ! latch for the 1st controller port
PAD1Bit_addr: .long 0x7c000b71   ! bit shifter for the 1st controller port  
PAD2Latch_addr: .long 0x7c000b72 ! latch for the 2st controller port
PAD2Bit_addr: .long 0x7c000b73   ! bit shifter for the 2st controller port    

.align 4
.read_audio:
    ! TODO: call global audio_read(wAddr)

    bra .return_to_caller
    nop

.align 4
.read_mapper:
    ! TODO: call global read_mapper(wAddr)

    bra .return_to_caller
    nop

.align 4
.return_to_caller:
    ! restore registers
    mov.l @r15+, r12
    mov.l @r15+, r11
    mov.l @r15+, r10
    mov.l @r15+, r9
    mov.l @r15+, r8

    rts
    nop

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

! APU register locations 
