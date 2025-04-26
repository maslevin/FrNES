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
_read_6502:
    ! 6502 read address is in r4
    mov.w .address_bank_mask, r0    ! put 0xe000 into r0
    mov #32, r1                     ! put 0x2000 into r1 

    mov.w .address_index_mask, r2   ! put 0x1fff into r2
    xor r7, r7                      ! clear r7

    mov r4, r5                      ! put a copy of the address into r5
    shll8 r1                        ! shift left to complete r1

    mov r4, r5                      ! put a copy of the address into r5
    mov r4, r6                      ! put a copy of the address into r6

    and r0, r5                      ! and r5 with the address mask to get the bank to switch options    
    mov.w .ram_local_index_mask, r8 ! put the ram local mask into r8 just in case we will need it

    and r2, r6                      ! and r6 with the index mask to get the index inside the bank we are interested in 
    cmp/eq r5, r7                   ! compare bank and 0x0000

    bt/s .read_MEM:                 ! if true, jump to read from 6502 memory routine
    mov r1, r7                      ! copy 0x2000 into r7

    mov r1, r8                      ! copy 0x2000 into r8
    cmp/eq r5, r1                   ! compare bank and 0x2000

    add r1, r7                      ! put another 0x2000 into r7 (0x4000)
    bt/s .read_PPU_Registers:       ! if true, jump to read from PPU registers

    cmp/eq r5, r7                   ! compare bank and 0x4000
    bt/s .read_APU_IO_Region:       ! if true, jump to read from APU or IO registers (but also potentially mapper read)

    add r7, r1                      ! put another 0x4000 into r1 (0x6000)
    cmp/eq r5, r1                   ! compare bank and 0x6000

    add r8, r1                      ! put another 0x2000 into r1 (0x8000)
    bt/s .read_WRAM_Region          ! if true, jump to read from Work RAM, if present

    cmp/eq r5, r1                   ! compare bank and 0x8000
    add r8, r1                      ! put another 0x2000 into r1 (0xa000)

    bt/s .read_ROMBANK0             ! if true, jump to read from ROMBANK0
    cmp/eq r5, r1                   ! compare bank and 0xa000

    add r8, r1                      ! put another 0x2000 into r1 (0xc000)
    bt/s .read_ROMBANK1             ! if true, jump to read from ROMBANK1

    cmp/eq r5, r1                   ! compare bank and 0xc000
    bt .read_ROMBANK2               ! if false, jump to read from ROMBANK2

.read_ROMBANK3                      ! otherwise, we are definitively in ROMBANK3

.address_bank_mask:
    .short 0xe000

.address_index_mask:
    .short 0x1fff

.ram_local_index_mask:
    .short 0x07ff

.align 4
.read_MEM:


.align 4
RAM_addr: .long 0x7c000000

.align 4
.read_PPU_Registers:


.align 4
.read_WRAM_Region:


    rts
    mov #0, 0




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

! clocks stored after 
CLOCKS_addr: .long 0x7c000b30