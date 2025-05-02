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

.global _reset_6502
.align 2
_reset_6502:
    ! set PC to reset vector
    mov   #0, r0             ! put sign extended 0 into r0        
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

    ! set controller pad latch and bit fields
    mov.l PAD1Latch_addr, r1 ! put the target address of the PAD1Latch variable into r1
    mov.b r0, @r1            ! init PAD1Latch to 0
    mov.l PAD1Bit_addr, r1   ! put the target address of the PAD1Bit variable into r1
    mov.b r0, @r1            ! init PAD1Bit to 0
    mov.l PAD2Latch_addr, r1 ! put the target address of the PAD2Latch variable into r1
    mov.b r0, @r1            ! init PAD2Latch to 0
    mov.l PAD2Bit_addr, r1   ! put the target address of the PAD2Bit variable into r1
    mov.b r0, @r1            ! init PAD2Bit to 0

    mov #(FLAG_Z | FLAG_R), r0 ! set the initial status register state of Z and R flags set
    mov.l S_addr, r1         ! put the target address of the S variable into r1
    mov.b r0, @r1            ! init S to Z + R flags set

    mov #0xff, r0            ! put sign extended 255 into r0
    mov.l SP_addr, r1        ! put the target address of the SP variable into r1
    mov.b r0, @r1            ! init SP to 0

    ! set ROMBANK addresses
    mov.l ROMBANKS, r2       ! put rombanks array address into r2
    mov.l RAM_Addr, r1       ! put RAM address into r1
    mov.l r1, @r2            ! move RAM address into ROMBANKS[0]
    mov #4, r3               ! put 4 into r3 
    xor r1, r1               ! clear out r1
    add r3, r2               ! move rombanks array pointer ahead one entry
    mov.l r1, @r2            ! put null into ROMBANKS[1]
    add r3, r2               ! move rombanks array pointer ahead one entry
    mov.l r1, @r2            ! put null into ROMBANKS[2]
    add r3, r2               ! move rombanks array pointer ahead one entry
    mov.l ROMBANK_WRAM_Addr, r1 ! put WRAM address into r1
    mov.l r1, @r2            ! put WRAM address into ROMBANKS[3]
    add r3, r2               ! move rombanks array pointer ahead one entry
    mov.l ROMBANK0_Addr, r1 ! put ROMBANK0 address into r1
    mov.l r1, @r2            ! put ROMBANK0 address into ROMBANKS[4]
    add r3, r2               ! move rombanks array pointer ahead one entry
    mov.l ROMBANK1_Addr, r1 ! put ROMBANK1 address into r1
    mov.l r1, @r2            ! put ROMBANK1 address into ROMBANKS[5]
    add r3, r2               ! move rombanks array pointer ahead one entry
    mov.l ROMBANK2_Addr, r1 ! put ROMBANK2 address into r1
    mov.l r1, @r2            ! put ROMBANK2 address into ROMBANKS[6]
    add r3, r2               ! move rombanks array pointer ahead one entry
    mov.l ROMBANK3_Addr, r1 ! put ROMBANK3 address into r1
    mov.l r1, @r2            ! put ROMBANK3 address into ROMBANKS[7]

    ! returning to caller
    rts
    mov #0, r0

.global _emulate_6502
.align 2
_emulate_6502:
    rts
    mov #0, r0

.align 4
PC_addr: .long 0x7c000b20
SP_addr: .long 0x7c000b22
S_addr: .long 0x7c000b23
A_addr: .long 0x7c000b24
X_addr: .long 0x7c000b25
Y_addr: .long 0x7c000b26
RAM_Addr: .long 0x7c000000
ROMBANK_WRAM_Addr: .long _ROMBANK_WRAM
ROMBANK0_Addr: .long _ROMBANK0
ROMBANK1_Addr: .long _ROMBANK1
ROMBANK2_Addr: .long _ROMBANK2
ROMBANK3_Addr: .long _ROMBANK3

IRQ_REQ_addr: .long 0x7c000b27 ! when nonzero, an IRQ has been requested
NMI_REQ_addr: .long 0x7c000b28 ! when nonzero, an NMI has been requested

CLOCKS_addr: .long 0x7c000b30  ! clocks stored after 

ROMBANKS: .long 0x7c000b50     ! address of 8 * 32-bit pointers for ROM banks 

PAD1Latch_addr: .long 0x7c000b70 ! latch for the 1st controller port
PAD1Bit_addr: .long 0x7c000b71   ! bit shifter for the 1st controller port  
PAD2Latch_addr: .long 0x7c000b72 ! latch for the 2st controller port
PAD2Bit_addr: .long 0x7c000b73   ! bit shifter for the 2st controller port  