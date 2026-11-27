! FrNES 1.0.0 - 6502_core.s
!
! SH4 assembly 6502 core for the most frequently executed opcodes (see the opcode profile, CPU_PROFILE_OPCODES).
! cpu_run() in cpu.c calls cpu_run_asm(budget) and falls back to the C core (cpu_run_c) for one instruction at a
! time whenever this core stops: on an opcode it doesn't implement, an instruction outside the RAM / ROM regions
! it can fetch from directly, a pending interrupt, or when the budget is used up. Behaviour, cycle counts and the
! timing of memory mapped I/O match the C core exactly, so instruction traces from both compare one for one.
!
! Register use (r8-r14 are callee saved, so they survive the C calls on the I/O paths)
!   r5   PC limit: the dispatch leaves to the C core once r11 reaches it (end of the RAM / ROM region)
!   r6   P without N and Z (C, Z, I, D, B, R, V, N bits as in the 6502, N and Z bits ignored)
!   r7   N / Z result: Z = (r7 & 0xff) == 0, N = r7 & 0x180 (bit 8 only comes from a restored P with N and Z set)
!   r8   A
!   r9   X
!   r10  Y
!   r11  PC as a host pointer: NES_RAM_BASE + PC
!   r12  cycles left in the budget (the instruction's cycles are subtracted when it completes)
!   r13  dispatch table
!   r14  NES_RAM_BASE (0x02000000, the MMU mapping of the 6502 address space)
!   r0-r4 scratch
!
! Stack frame (r15)
!   0   budget, reduced when an I/O access raises an interrupt so the batch ends after that instruction
!   4   cpu_registers pointer
!   8   saved r5, 12 r6, 16 r7, 20 pr, 24 r3 (around C calls), 28 read result
!   32, 36  handler scratch (values that must survive an rd_abs / wr_abs)
!   40, 44  dummy_io's saved r4 and pr
!
! GBR is not used: KOS code is built with -ftls-model=local-exec and uses GBR as the thread pointer.

    .section .text.hot.1_core, "ax"

! cpu_registers field offsets (6502_registers.h)
.equ REG_PC,  4
.equ REG_SP,  6
.equ REG_S,   7
.equ REG_A,   8
.equ REG_X,   9
.equ REG_Y,   10
.equ REG_NMI, 11
.equ REG_IRQ, 12
.equ REG_DMA, 18
.equ REG_READ_SKIP, 28
.equ REG_IO_PC, 30

.equ FRAME_SIZE, 48

! fetch a 16 bit operand into r4
.macro FETCH16
    mov.b   @r11+, r4
    extu.b  r4, r4
    mov.b   @r11+, r0
    extu.b  r0, r0
    shll8   r0
    or      r0, r4
.endm

! finish an instruction of n cycles
.macro NEXT n
    bra     dispatch
    add     #-\n, r12
.endm

!---------------------------------------------------------------------------------------------------------------------
! Dummy reads of indexed accesses: the address with the index added to the low byte only, r4 - (r3 << 8) (r4 the
! effective address, r3 1 when the index crossed a page). Only register reads ($2000-$401F) have side effects, so
! only those are made, through io_read; over open bus ($4020-$7FFF) the dummy read leaves the operand's high byte
! on the data bus (see dummy_io). DUMMY_RD keeps r3 and r4.
!---------------------------------------------------------------------------------------------------------------------
.macro DUMMY_RD
    mov     r3, r0
    shll8   r0
    mov     r4, r1
    sub     r0, r1
    mov     #0x20, r0
    shll8   r0
    sub     r0, r1
    mov     #0x60, r0
    shll8   r0
    cmp/hs  r0, r1
    bt      98f
    bsr     dummy_io
    nop
98:
.endm

! abs,X / abs,Y reads: r3 = 1 when the index crosses a page (the extra cycle is taken after the read, as in C)
.macro INDEX_ABS reg
    FETCH16
    mov     r4, r3
    add     \reg, r4
    extu.w  r4, r4
    xor     r4, r3
    shlr8   r3
    mov     r3, r0
    and     #1, r0
    mov     r0, r3
.endm

! compare register with r1: C = reg >= r1, N/Z from reg - r1
.macro COMPARE reg
    cmp/hs  r1, \reg
    movt    r0
    mov     #-2, r2
    and     r2, r6
    or      r0, r6
    mov     \reg, r7
    sub     r1, r7
    extu.b  r7, r7
.endm

! A = A + r1 + C, with C and V (the 2A03 has no decimal mode)
.macro ADC_R1
    mov     r6, r0
    and     #1, r0
    add     r8, r0
    add     r1, r0
    mov     r8, r2
    xor     r0, r2
    mov     r1, r3
    xor     r0, r3
    and     r3, r2
    mov     r0, r3
    mov     r6, r0
    and     #0xbe, r0
    mov     r0, r6
    mov     r3, r0
    shlr8   r0
    and     #1, r0
    or      r0, r6
    mov     r2, r0
    shlr    r0
    and     #0x40, r0
    or      r0, r6
    extu.b  r3, r8
    mov     r8, r7
.endm

!=====================================================================================================================
! Second set (from the Mega Man 3 profile)
!=====================================================================================================================
! r0 = zero page address
.macro ZP_ADDR
    mov.b   @r11+, r0
    extu.b  r0, r0
.endm

! r1 = zero page value, r4 = its address
.macro ZP_LOAD
    ZP_ADDR
    mov     r0, r4
    mov.b   @(r0, r14), r1
    extu.b  r1, r1
.endm

!=====================================================================================================================
! More addressing mode / operation combinations, placed before cpu_run_asm so that their branches back to
! dispatch and the access helpers stay in the 12 bit range. These (the C core used to run these: each fallback costs a register
! hand-off both ways). Read macros leave the value in r0, zero extended; the cycles before the read are taken from r12
!=====================================================================================================================
.macro RD_ABS
    FETCH16
    add     #-3, r12
    bsr     rd_abs
    nop
.endm

! abs,X / abs,Y: the extra cycle and dummy read when the index crosses a page
.macro RD_ABSI reg
    INDEX_ABS \reg
    add     #-3, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
.endm

! (zp),Y
.macro RD_INDY
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r4
    add     #1, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r0
    extu.b  r4, r4
    extu.b  r0, r0
    shll8   r0
    or      r0, r4
    mov     r4, r3
    add     r10, r4
    extu.w  r4, r4
    xor     r4, r3
    shlr8   r3
    mov     r3, r0
    and     #1, r0
    mov     r0, r3
    add     #-4, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
.endm

! zp,X / zp,Y (zero page wraps)
.macro RD_ZPI reg
    mov.b   @r11+, r0
    add     \reg, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r0
    extu.b  r0, r0
.endm

.macro OP_ORA
    or      r0, r8
    mov     r8, r7
.endm
.macro OP_AND
    and     r0, r8
    mov     r8, r7
.endm
.macro OP_EOR
    xor     r0, r8
    mov     r8, r7
.endm
.macro OP_ADC
    mov     r0, r1
    mov.l   r3, @-r15
    ADC_R1
    mov.l   @r15+, r3
.endm
.macro OP_SBC
    not     r0, r1
    extu.b  r1, r1
    mov.l   r3, @-r15
    ADC_R1
    mov.l   @r15+, r3
.endm
.macro OP_CMP reg
    mov     r0, r1
    mov.l   r3, @-r15
    COMPARE \reg
    mov.l   @r15+, r3
.endm

op_0D:  ! ORA abs
    RD_ABS
    OP_ORA
    NEXT    1
op_2D:  ! AND abs
    RD_ABS
    OP_AND
    NEXT    1
op_4D:  ! EOR abs
    RD_ABS
    OP_EOR
    NEXT    1
op_ED:  ! SBC abs
    RD_ABS
    OP_SBC
    NEXT    1
op_CD:  ! CMP abs
    RD_ABS
    OP_CMP  r8
    NEXT    1
op_EC:  ! CPX abs
    RD_ABS
    OP_CMP  r9
    NEXT    1
op_CC:  ! CPY abs
    RD_ABS
    OP_CMP  r10
    NEXT    1
op_AE:  ! LDX abs
    RD_ABS
    mov     r0, r9
    mov     r0, r7
    NEXT    1
op_3D:  ! AND abs,X
    RD_ABSI r9
    OP_AND
    NEXT    1
op_5D:  ! EOR abs,X
    RD_ABSI r9
    OP_EOR
    NEXT    1
op_DD:  ! CMP abs,X
    RD_ABSI r9
    OP_CMP  r8
    NEXT    1
op_FD:  ! SBC abs,X
    RD_ABSI r9
    OP_SBC
    NEXT    1
op_19:  ! ORA abs,Y
    RD_ABSI r10
    OP_ORA
    NEXT    1
op_39:  ! AND abs,Y
    RD_ABSI r10
    OP_AND
    NEXT    1
op_59:  ! EOR abs,Y
    RD_ABSI r10
    OP_EOR
    NEXT    1
op_F9:  ! SBC abs,Y
    RD_ABSI r10
    OP_SBC
    NEXT    1
op_BE:  ! LDX abs,Y
    RD_ABSI r10
    mov     r0, r9
    mov     r0, r7
    NEXT    1
op_11:  ! ORA (zp),Y
    RD_INDY
    OP_ORA
    NEXT    1
op_31:  ! AND (zp),Y
    RD_INDY
    OP_AND
    NEXT    1
op_51:  ! EOR (zp),Y
    RD_INDY
    OP_EOR
    NEXT    1
op_D1:  ! CMP (zp),Y
    RD_INDY
    OP_CMP  r8
    NEXT    1
op_F1:  ! SBC (zp),Y
    RD_INDY
    OP_SBC
    NEXT    1
op_15:  ! ORA zp,X
    RD_ZPI  r9
    OP_ORA
    NEXT    4
op_35:  ! AND zp,X
    RD_ZPI  r9
    OP_AND
    NEXT    4
op_55:  ! EOR zp,X
    RD_ZPI  r9
    OP_EOR
    NEXT    4
op_75:  ! ADC zp,X
    RD_ZPI  r9
    OP_ADC
    NEXT    4
op_F5:  ! SBC zp,X
    RD_ZPI  r9
    OP_SBC
    NEXT    4
op_D5:  ! CMP zp,X
    RD_ZPI  r9
    OP_CMP  r8
    NEXT    4
op_B4:  ! LDY zp,X
    RD_ZPI  r9
    mov     r0, r10
    mov     r0, r7
    NEXT    4
op_B6:  ! LDX zp,Y
    RD_ZPI  r10
    mov     r0, r9
    mov     r0, r7
    NEXT    4
op_25:  ! AND zp
    ZP_LOAD
    and     r1, r8
    mov     r8, r7
    NEXT    3
op_C4:  ! CPY zp
    ZP_LOAD
    COMPARE r10
    NEXT    3
op_94:  ! STY zp,X
    mov.b   @r11+, r0
    add     r9, r0
    extu.b  r0, r0
    mov.b   r10, @(r0, r14)
    NEXT    4
op_96:  ! STX zp,Y
    mov.b   @r11+, r0
    add     r10, r0
    extu.b  r0, r0
    mov.b   r9, @(r0, r14)
    NEXT    4
op_8E:  ! STX abs
    FETCH16
    add     #-3, r12
    bsr     wr_abs
    mov     r9, r1
    NEXT    1
op_8C:  ! STY abs
    FETCH16
    add     #-3, r12
    bsr     wr_abs
    mov     r10, r1
    NEXT    1
op_2A:  ! ROL A
    mov     r6, r0
    and     #1, r0
    mov     r0, r2
    shll    r8
    mov     r8, r0
    shlr8   r0
    mov     #-2, r1
    and     r1, r6
    or      r0, r6
    or      r2, r8
    extu.b  r8, r8
    mov     r8, r7
    NEXT    2
op_6A:  ! ROR A
    mov     r6, r0
    and     #1, r0
    shll8   r0
    shlr    r0
    mov     r0, r2
    mov     r8, r0
    and     #1, r0
    mov     #-2, r1
    and     r1, r6
    or      r0, r6
    shlr    r8
    or      r2, r8
    mov     r8, r7
    NEXT    2
op_9A:  ! TXS
    mov.l   @(4, r15), r1
    mov     r9, r0
    mov.b   r0, @(REG_SP, r1)
    NEXT    2
op_BA:  ! TSX
    mov.l   @(4, r15), r1
    mov.b   @(REG_SP, r1), r0
    extu.b  r0, r9
    mov     r9, r7
    NEXT    2
op_B8:  ! CLV
    mov     #-65, r0
    and     r0, r6
    NEXT    2
op_D8:  ! CLD
    mov     #-9, r0
    and     r0, r6
    NEXT    2
op_F8:  ! SED
    mov     r6, r0
    or      #8, r0
    mov     r0, r6
    NEXT    2

! INC / DEC memory once r4 holds the address and the cycles up to the read are taken: read, write the old value
! back (skipped for RAM, where it can't be seen), write the result (as op_FE)
.macro RMW_INCDEC delta
    bsr     rd_abs
    mov.l   r4, @(32, r15)
    mov.l   @(32, r15), r4
    add     #-1, r12
    mov.w   99f, r1
    cmp/hs  r1, r4
    bf      1f
    mov.l   r0, @(36, r15)
    bsr     wr_abs
    mov     r0, r1
    ! an NMI raised by the first write is polled before the last one, so it isn't delayed (see write_byte $2000)
    mov.l   @(4, r15), r1
    mov.b   @(REG_NMI, r1), r0
    cmp/eq  #2, r0
    bf      2f
    mov     #1, r0
    mov.b   r0, @(REG_NMI, r1)
2:  mov.l   @(32, r15), r4
    mov.l   @(36, r15), r0
1:  add     #-1, r12
    add     #\delta, r0
    extu.b  r0, r1
    bsr     wr_abs
    mov     r1, r7
    NEXT    1
    .align 1
99: .word 0x2000
.endm

op_EE:  ! INC abs
    FETCH16
    add     #-3, r12
    RMW_INCDEC 1
op_CE:  ! DEC abs
    FETCH16
    add     #-3, r12
    RMW_INCDEC -1
op_DE:  ! DEC abs,X: dummy read with only the low byte indexed first
    INDEX_ABS r9
    add     #-3, r12
    DUMMY_RD
    add     #-1, r12
    RMW_INCDEC -1
op_91:  ! STA (zp),Y: always a dummy read with only the low byte indexed, then the write
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r4
    add     #1, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r0
    extu.b  r4, r4
    extu.b  r0, r0
    shll8   r0
    or      r0, r4
    mov     r4, r3
    add     r10, r4
    extu.w  r4, r4
    xor     r4, r3
    shlr8   r3
    mov     r3, r0
    and     #1, r0
    mov     r0, r3
    add     #-4, r12
    DUMMY_RD
    add     #-1, r12
    bsr     wr_abs
    mov     r8, r1
    NEXT    1


!=====================================================================================================================
! uint32 cpu_run_asm(uint32 budget)
!=====================================================================================================================
    .global _cpu_run_asm
    .align 5
_cpu_run_asm:
    mov.l   r8, @-r15
    mov.l   r9, @-r15
    mov.l   r10, @-r15
    mov.l   r11, @-r15
    mov.l   r12, @-r15
    mov.l   r13, @-r15
    mov.l   r14, @-r15
    sts.l   pr, @-r15
    add     #-FRAME_SIZE, r15
    mov.l   r4, @r15
    mov.l   .Lcpu_registers, r1
    mov.l   @r1, r1
    mov.l   r1, @(4, r15)

    ! a pending NMI, or an IRQ that isn't masked, is serviced by the C core
    mov.b   @(REG_NMI, r1), r0
    tst     r0, r0
    bf      .Lentry_return_zero
    mov.b   @(REG_IRQ, r1), r0
    tst     r0, r0
    bt      .Lentry_load
    mov.b   @(REG_S, r1), r0
    tst     #4, r0
    bt      .Lentry_return_zero

.Lentry_load:
    mov.b   @(REG_A, r1), r0
    extu.b  r0, r8
    mov.b   @(REG_X, r1), r0
    extu.b  r0, r9
    mov.b   @(REG_Y, r1), r0
    extu.b  r0, r10
    mov.b   @(REG_S, r1), r0
    extu.b  r0, r6
    ! r7 from the N and Z bits of P
    and     #0x80, r0
    mov     r0, r7
    mov     r6, r0
    tst     #2, r0
    bt/s    .Lentry_z_clear
    nop
    ! Z set: low byte 0, N moves to bit 8
    bra     .Lentry_flags_done
    shll    r7
.Lentry_z_clear:
    add     #1, r7
.Lentry_flags_done:
    mov.l   .Lnes_ram_base, r14
    mov.l   .Ldispatch_table, r13
    mov.l   @r15, r12
    mov.w   @(REG_PC, r1), r0
    extu.w  r0, r4
    bsr     set_region
    nop
    ! fall into dispatch

!---------------------------------------------------------------------------------------------------------------------
! dispatch: check the budget and the region, fetch and jump to the next opcode's handler
!---------------------------------------------------------------------------------------------------------------------
!   The region check comes first (a PC outside the directly fetched regions may be unmapped), the opcode load
!   next, and the budget check in its load shadow. mov.b sign extends the opcode: r13 points at the middle of the
!   dispatch table (opcodes $80-$FF are its first half), so the index needs no extu.b. (sh4-sim: the next handler
!   decodes 2 cycles earlier than with budget check, region check, load, extu.b.)
dispatch:
    cmp/hs  r5, r11
    bt      exit_asm
    mov.b   @r11+, r0
    cmp/pl  r12
    bf      exit_asm_unfetch
    shll2   r0
    mov.l   @(r0, r13), r1
    jmp     @r1
    nop

! leaving with the opcode already fetched: PC back on it
exit_asm_unfetch:
    add     #-1, r11

!---------------------------------------------------------------------------------------------------------------------
! exit: store the registers back and return the cycles run
!---------------------------------------------------------------------------------------------------------------------
exit_asm:
    mov.l   @(4, r15), r1
    mov     r11, r0
    sub     r14, r0
    mov.w   r0, @(REG_PC, r1)
    mov     r8, r0
    mov.b   r0, @(REG_A, r1)
    mov     r9, r0
    mov.b   r0, @(REG_X, r1)
    mov     r10, r0
    mov.b   r0, @(REG_Y, r1)
    ! P = (r6 & ~(N | Z)) | N | Z
    mov     #0x7d, r2
    and     r6, r2
    mov     r7, r0
    shlr    r0
    tst     #0xc0, r0
    bt      1f
    mov     #-128, r3
    or      r3, r2
1:  mov     r7, r0
    tst     #0xff, r0
    bf      2f
    mov     #2, r3
    or      r3, r2
2:  mov     r2, r0
    mov.b   r0, @(REG_S, r1)
    mov.l   @r15, r0
    sub     r12, r0
.Lentry_return:
    add     #FRAME_SIZE, r15
    lds.l   @r15+, pr
    mov.l   @r15+, r14
    mov.l   @r15+, r13
    mov.l   @r15+, r12
    mov.l   @r15+, r11
    mov.l   @r15+, r10
    mov.l   @r15+, r9
    rts
    mov.l   @r15+, r8

.Lentry_return_zero:
    bra     .Lentry_return
    mov     #0, r0

    .align 2
.Lcpu_registers:    .long _cpu_registers
.Lnes_ram_base:     .long 0x02000000
.Ldispatch_table:   .long dispatch_middle

!---------------------------------------------------------------------------------------------------------------------
! set_region: r4 = new PC (16 bit). Sets r11 and the limit r5. PCs the core can't fetch from directly ($07FE-$5FFF,
! $6000-$7FFF unless it's plain PRG RAM, $FFFE-$FFFF) get a limit equal to the PC, so the next dispatch leaves to the
! C core.
!---------------------------------------------------------------------------------------------------------------------
set_region:
    mov     r14, r11
    add     r4, r11
    mov.w   .Lc07fe, r0
    cmp/hs  r0, r4
    bt      1f
    ! RAM $0000-$07FD
    mov     r14, r5
    rts
    add     r0, r5
1:  mov     r4, r0
    shll16  r0
    cmp/pz  r0
    bt      2f
    ! ROM $8000-$FFFD
    mov.w   .Lcfffe, r0
    extu.w  r0, r0
    mov     r14, r5
    rts
    add     r0, r5
2:  ! $6000-$7FFF when it's plain PRG RAM (WRAM present, no mapper answering the reads itself): fetched directly
    ! through the window up to $8000 (writes to it go through the same window addresses, so nothing is stale)
    mov.w   .Lsr_c6000, r0
    mov     r4, r1
    sub     r0, r1
    mov.w   .Lsr_c2000, r0
    cmp/hs  r0, r1
    bt      3f
    mov.l   .Lsr_wram, r0
    mov.l   @r0, r0
    tst     r0, r0
    bt      3f
    mov.l   .Lsr_6000_reads, r0
    mov.b   @r0, r0
    tst     r0, r0
    bf      3f
    mov.w   .Lsr_c8000, r0
    extu.w  r0, r0
    mov     r14, r5
    rts
    add     r0, r5
    ! anything else ($07FE-$5FFF): the C core
3:  rts
    mov     r11, r5

    .align 1
.Lsr_c6000: .word 0x6000
.Lsr_c2000: .word 0x2000
.Lsr_c8000: .word 0x8000
    .align 2
.Lsr_wram:          .long _WRAM
.Lsr_6000_reads:    .long _mapper_6000_reads

!---------------------------------------------------------------------------------------------------------------------
! rd_abs: r0 = read(r4). RAM and ROM directly, $0800-$1FFF masked to the canonical RAM, the rest through read_byte.
!---------------------------------------------------------------------------------------------------------------------
rd_abs:
    mov.w   .Lc2000, r0
    cmp/hs  r0, r4
    bt      1f
    mov.w   .Lc07ff, r0
    and     r4, r0
    mov.b   @(r0, r14), r0
    rts
    extu.b  r0, r0
1:  mov     r4, r0
    shll16  r0
    cmp/pz  r0
    bt      io_read
    mov     r4, r0
    mov.b   @(r0, r14), r0
    rts
    extu.b  r0, r0

! wr_abs: write(r4, r1). RAM directly (mirrors masked), everything else through write_byte.
wr_abs:
    mov.w   .Lc2000, r0
    cmp/hs  r0, r4
    bt      io_write
    mov.w   .Lc07ff, r0
    and     r4, r0
    rts
    mov.b   r1, @(r0, r14)

    .align 1
.Lc07fe:    .word 0x07fe
.Lcfffe:    .word 0xfffe
.Lc2000:    .word 0x2000
.Lc07ff:    .word 0x07ff

!---------------------------------------------------------------------------------------------------------------------
! I/O through the C core's read_byte / write_byte. cpu_cycle_now is set to the cycle the instruction started at
! (r12 still holds the cycles left at the start of the instruction), the same as IO_READ / IO_WRITE in cpu.c.
! If the access raised an interrupt the budget is cut so the batch ends after this instruction.
!---------------------------------------------------------------------------------------------------------------------
.macro IO_ENTER
    mov.l   r5, @(8, r15)
    mov.l   r6, @(12, r15)
    mov.l   r7, @(16, r15)
    sts     pr, r0
    mov.l   r0, @(20, r15)
    mov.l   r3, @(24, r15)
    ! cpu_cycle_now = cpu_cycle_total + (budget - r12)
    mov.l   @r15, r0
    sub     r12, r0
    mov.l   .Lcpu_cycle_total, r2
    mov.l   @r2, r2
    add     r2, r0
    mov.l   .Lcpu_cycle_now, r2
    mov.l   r0, @r2
.endm

.macro IO_LEAVE
    mov.l   @(8, r15), r5
    mov.l   @(12, r15), r6
    mov.l   @(16, r15), r7
    mov.l   @(20, r15), r0
    lds     r0, pr
    mov.l   @(24, r15), r3
    ! an NMI or IRQ raised by the access, or a device event moved earlier (cpu_batch_cut), ends the batch after
    ! this instruction
    mov.l   @(4, r15), r1
    mov.b   @(REG_NMI, r1), r0
    mov     r0, r2
    mov.b   @(REG_IRQ, r1), r0
    or      r0, r2
    mov.l   .Lcpu_batch_cut, r0
    mov.b   @r0, r0
    or      r2, r0
    tst     r0, r0
    bt      99f
    mov.l   @r15, r0
    sub     r12, r0
    mov.l   r0, @r15
    mov     #0, r12
99:
.endm

io_read:
    IO_ENTER
    ! the PC after this instruction, for a $2002 poll loop's skip (ppu_poll_skip)
    mov     r11, r0
    sub     r14, r0
    mov.l   @(4, r15), r1
    mov.w   r0, @(REG_IO_PC, r1)
    mov.l   .Lread_byte, r0
    jsr     @r0
    nop
    extu.b  r0, r0
    mov.l   r0, @(28, r15)
    IO_LEAVE
    ! a poll loop's iterations skipped: this instruction takes their cycles
    mov.w   @(REG_READ_SKIP, r1), r0
    extu.w  r0, r0
    tst     r0, r0
    bt      1f
    sub     r0, r12
    mov     #0, r0
    mov.w   r0, @(REG_READ_SKIP, r1)
1:  rts
    mov.l   @(28, r15), r0

io_write:
    mov     r1, r3
    IO_ENTER
    mov.l   .Lwrite_byte, r0
    jsr     @r0
    mov     r3, r5
    IO_LEAVE
    ! OAM DMA stalls the CPU, those cycles belong to this instruction
    mov.w   @(REG_DMA, r1), r0
    extu.w  r0, r0
    tst     r0, r0
    bt      1f
    sub     r0, r12
    mov     #0, r0
    mov.w   r0, @(REG_DMA, r1)
1:  rts
    nop

    .align 2
.Lcpu_cycle_total:  .long _cpu_cycle_total
.Lcpu_batch_cut:    .long _cpu_batch_cut
.Lcpu_cycle_now:    .long _cpu_cycle_now
.Lread_byte:        .long _read_byte_6502_c
.Lwrite_byte:       .long _write_byte_6502_c


! r1 = dummy address - $2000
dummy_io:
    mov.w   .Lc2020, r0
    cmp/hs  r0, r1
    bt      dummy_open
    mov.l   r4, @(40, r15)
    sts     pr, r0
    mov.l   r0, @(44, r15)
    mov     #0x20, r0
    shll8   r0
    add     r0, r1
    bsr     io_read
    mov     r1, r4
    mov.l   @(44, r15), r0
    lds     r0, pr
    rts
    mov.l   @(40, r15), r4

! $4020-$7FFF: the buses keep the high byte of the address (cpu_io_bus_value / cpu_internal_bus_value /
! cpu_io_bus_cycle, 6502_readbyte.c)
dummy_open:
    mov     r1, r0
    shlr8   r0
    add     #0x20, r0
    mov.l   .Lcpu_io_bus_value, r2
    mov.b   r0, @r2
    mov.l   .Lcpu_internal_bus_value, r2
    mov.b   r0, @r2
    mov.l   @r15, r0
    sub     r12, r0
    mov.l   .Lcpu_cycle_total2, r2
    mov.l   @r2, r2
    add     r2, r0
    mov.l   .Lcpu_io_bus_cycle, r2
    rts
    mov.l   r0, @r2

    .align 1
.Lc2020:    .word 0x2020
    .align 2
.Lcpu_io_bus_value: .long _cpu_io_bus_value
.Lcpu_internal_bus_value: .long _cpu_internal_bus_value
.Lcpu_io_bus_cycle: .long _cpu_io_bus_cycle
.Lcpu_cycle_total2: .long _cpu_cycle_total

!---------------------------------------------------------------------------------------------------------------------
! Opcodes the core doesn't implement: back up to the opcode and let the C core run it
!---------------------------------------------------------------------------------------------------------------------
    .global _cpu_asm_op_fallback
_cpu_asm_op_fallback:
op_fallback:
    bra     exit_asm
    add     #-1, r11

!=====================================================================================================================
! Loads / stores
!=====================================================================================================================
op_A9:  ! LDA #
    mov.b   @r11+, r8
    extu.b  r8, r8
    mov     r8, r7
    NEXT    2
op_A2:  ! LDX #
    mov.b   @r11+, r9
    extu.b  r9, r9
    mov     r9, r7
    NEXT    2
op_A0:  ! LDY #
    mov.b   @r11+, r10
    extu.b  r10, r10
    mov     r10, r7
    NEXT    2
op_A5:  ! LDA zp
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r8
    extu.b  r8, r8
    mov     r8, r7
    NEXT    3
op_A6:  ! LDX zp
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r9
    extu.b  r9, r9
    mov     r9, r7
    NEXT    3
op_A4:  ! LDY zp
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r10
    extu.b  r10, r10
    mov     r10, r7
    NEXT    3
op_B5:  ! LDA zp,X
    mov.b   @r11+, r0
    add     r9, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r8
    extu.b  r8, r8
    mov     r8, r7
    NEXT    4
op_85:  ! STA zp
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   r8, @(r0, r14)
    NEXT    3
op_95:  ! STA zp,X
    mov.b   @r11+, r0
    add     r9, r0
    extu.b  r0, r0
    mov.b   r8, @(r0, r14)
    NEXT    4
op_86:  ! STX zp
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   r9, @(r0, r14)
    NEXT    3
op_AD:  ! LDA abs
    FETCH16
    add     #-3, r12
    bsr     rd_abs
    nop
    mov     r0, r8
    mov     r0, r7
    NEXT    1
op_AC:  ! LDY abs
    FETCH16
    add     #-3, r12
    bsr     rd_abs
    nop
    mov     r0, r10
    mov     r0, r7
    NEXT    1


op_BD:  ! LDA abs,X
    INDEX_ABS r9
    add     #-3, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    mov     r0, r8
    mov     r0, r7
    NEXT    1
op_B9:  ! LDA abs,Y
    INDEX_ABS r10
    add     #-3, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    mov     r0, r8
    mov     r0, r7
    NEXT    1
op_B1:  ! LDA (zp),Y
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r4
    add     #1, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r0
    extu.b  r4, r4
    extu.b  r0, r0
    shll8   r0
    or      r0, r4
    mov     r4, r3
    add     r10, r4
    extu.w  r4, r4
    xor     r4, r3
    shlr8   r3
    mov     r3, r0
    and     #1, r0
    mov     r0, r3
    add     #-4, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    mov     r0, r8
    mov     r0, r7
    NEXT    1
op_8D:  ! STA abs
    FETCH16
    add     #-3, r12
    bsr     wr_abs
    mov     r8, r1
    NEXT    1
op_9D:  ! STA abs,X (always reads the address with only the low byte indexed first)
    INDEX_ABS r9
    add     #-3, r12
    DUMMY_RD
    add     #-1, r12
    bsr     wr_abs
    mov     r8, r1
    NEXT    1
op_99:  ! STA abs,Y (always reads the address with only the low byte indexed first)
    INDEX_ABS r10
    add     #-3, r12
    DUMMY_RD
    add     #-1, r12
    bsr     wr_abs
    mov     r8, r1
    NEXT    1

!=====================================================================================================================
! Register / flag operations
!=====================================================================================================================
op_C8:  ! INY
    add     #1, r10
    extu.b  r10, r10
    mov     r10, r7
    NEXT    2
op_88:  ! DEY
    add     #-1, r10
    extu.b  r10, r10
    mov     r10, r7
    NEXT    2
op_E8:  ! INX
    add     #1, r9
    extu.b  r9, r9
    mov     r9, r7
    NEXT    2
op_CA:  ! DEX
    add     #-1, r9
    extu.b  r9, r9
    mov     r9, r7
    NEXT    2
op_A8:  ! TAY
    mov     r8, r10
    mov     r8, r7
    NEXT    2
op_AA:  ! TAX
    mov     r8, r9
    mov     r8, r7
    NEXT    2
op_18:  ! CLC
    mov     #-2, r0
    and     r0, r6
    NEXT    2
op_38:  ! SEC
    mov     r6, r0
    or      #1, r0
    mov     r0, r6
    NEXT    2
op_EA:  ! NOP
    NEXT    2

!=====================================================================================================================
! ALU
!=====================================================================================================================
op_29:  ! AND #
    mov.b   @r11+, r0
    extu.b  r0, r0
    and     r0, r8
    mov     r8, r7
    NEXT    2
op_09:  ! ORA #
    mov.b   @r11+, r0
    extu.b  r0, r0
    or      r0, r8
    mov     r8, r7
    NEXT    2
op_05:  ! ORA zp
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r0
    extu.b  r0, r0
    or      r0, r8
    mov     r8, r7
    NEXT    3
op_4A:  ! LSR A
    mov     r8, r0
    and     #1, r0
    mov     #-2, r1
    and     r1, r6
    or      r0, r6
    shlr    r8
    mov     r8, r7
    NEXT    2
op_0A:  ! ASL A
    mov     r8, r0
    shll16  r0
    shll8   r0
    shll    r0
    movt    r0
    mov     #-2, r1
    and     r1, r6
    or      r0, r6
    shll    r8
    extu.b  r8, r8
    mov     r8, r7
    NEXT    2


op_C9:  ! CMP #
    mov.b   @r11+, r1
    extu.b  r1, r1
    COMPARE r8
    NEXT    2
op_E0:  ! CPX #
    mov.b   @r11+, r1
    extu.b  r1, r1
    COMPARE r9
    NEXT    2


op_69:  ! ADC #
    mov.b   @r11+, r1
    extu.b  r1, r1
    ADC_R1
    NEXT    2
op_79:  ! ADC abs,Y
    INDEX_ABS r10
    add     #-3, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    mov     r0, r1
    mov.l   r3, @-r15
    ADC_R1
    mov.l   @r15+, r3
    NEXT    1

!=====================================================================================================================
! Stack
!=====================================================================================================================
! r1 = cpu_registers, r2 = SP, r0 = 0x100 + SP
.macro STACK_SETUP
    mov.l   @(4, r15), r1
    mov.b   @(REG_SP, r1), r0
    extu.b  r0, r2
    mov     #1, r0
    shll8   r0
    add     r2, r0
.endm

op_48:  ! PHA
    STACK_SETUP
    mov.b   r8, @(r0, r14)
    add     #-1, r2
    mov     r2, r0
    mov.b   r0, @(REG_SP, r1)
    NEXT    3
op_68:  ! PLA
    STACK_SETUP
    add     #1, r0
    add     #1, r2
    extu.b  r2, r2
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   @(r0, r14), r8
    extu.b  r8, r8
    mov     r8, r7
    mov     r2, r0
    mov.b   r0, @(REG_SP, r1)
    NEXT    4

op_20:  ! JSR abs, pushes the address of the instruction's last byte. The target's high byte is read after the
        ! push, so a JSR whose push overwrites its own operand jumps through the new byte (as hardware does)
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.l   r0, @(32, r15)
    STACK_SETUP
    mov     r11, r3
    sub     r14, r3
    ! high byte at 0x100 + SP
    mov     r3, r0
    shlr8   r0
    mov     r0, r4
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   r4, @(r0, r14)
    ! low byte at 0x100 + (SP - 1)
    add     #-1, r2
    extu.b  r2, r2
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   r3, @(r0, r14)
    add     #-1, r2
    mov     r2, r0
    mov.b   r0, @(REG_SP, r1)
    ! now the high byte of the target
    mov.b   @r11, r4
    extu.b  r4, r4
    shll8   r4
    mov.l   @(32, r15), r0
    or      r0, r4
    bsr     set_region
    nop
    NEXT    6

op_60:  ! RTS
    STACK_SETUP
    add     #1, r2
    extu.b  r2, r2
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   @(r0, r14), r4
    extu.b  r4, r4
    add     #1, r2
    extu.b  r2, r2
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   @(r0, r14), r0
    extu.b  r0, r0
    shll8   r0
    or      r0, r4
    add     #1, r4
    extu.w  r4, r4
    mov     r2, r0
    mov.b   r0, @(REG_SP, r1)
    bsr     set_region
    nop
    NEXT    6

!=====================================================================================================================
! Jumps and branches
!=====================================================================================================================
op_4C:  ! JMP abs
    FETCH16
    ! a jump to itself burns 3 cycles per pass until the batch ends, take all the passes at once (as the C core does)
    mov     r11, r0
    sub     r14, r0
    add     #-3, r0
    extu.w  r0, r0
    cmp/eq  r0, r4
    bf      1f
    mov     #3, r0
    cmp/hi  r0, r12
    bf      1f
    ! not while an NMI is pending (one delayed by an instruction is taken right after this jump)
    mov.l   @(4, r15), r1
    mov.b   @(REG_NMI, r1), r0
    tst     r0, r0
    bf      1f
    ! r12 = r12 - 3 * ceil(r12 / 3) + 3, so that NEXT 3 leaves exactly the remainder
    mov     r12, r1
    add     #2, r1
    mov.w   .Lc_aaab, r2
    extu.w  r2, r2
    mul.l   r2, r1
    sts     macl, r1
    shlr16  r1
    shlr    r1
    mov     r1, r2
    add     r1, r1
    add     r2, r1
    sub     r1, r12
    add     #3, r12
1:  bsr     set_region
    nop
    NEXT    3

    .align 1
.Lc_aaab:   .word 0xaaab

! taken branch: r1 = signed offset, r11 points after the operand. 3 cycles, 4 when the target is on another page
branch_taken:
    mov     r11, r2
    add     r1, r11
    xor     r11, r2
    shlr8   r2
    mov     r2, r0
    tst     #0xff, r0
    bf      1f
    ! a taken branch that stays on the page polls for interrupts before its second cycle, not its last: when it
    ! ends the batch, tell cpu_run_devices() (cpu_poll_early)
    mov     r12, r0
    add     #-3, r0
    cmp/pl  r0
    bt      2f
    mov.l   .Lcpu_poll_early, r2
    mov     #1, r0
    mov.b   r0, @r2
2:  NEXT    3
1:  ! page crossed: the target may be outside the current region (or wrap past $0000 / $FFFF)
    mov     r11, r4
    sub     r14, r4
    extu.w  r4, r4
    bsr     set_region
    nop
    NEXT    4

    .align 2
.Lcpu_poll_early:   .long _cpu_poll_early

.macro BRANCH_ON_T_CLEAR
    bf      branch_taken
    NEXT    2
.endm
.macro BRANCH_ON_T_SET
    bt      branch_taken
    NEXT    2
.endm

op_D0:  ! BNE (taken when Z clear: (r7 & 0xff) != 0, T = 0)
    mov.b   @r11+, r1
    mov     r7, r0
    tst     #0xff, r0
    BRANCH_ON_T_CLEAR
op_F0:  ! BEQ
    mov.b   @r11+, r1
    mov     r7, r0
    tst     #0xff, r0
    BRANCH_ON_T_SET
op_10:  ! BPL (taken when N clear)
    mov.b   @r11+, r1
    mov     r7, r0
    shlr    r0
    tst     #0xc0, r0
    BRANCH_ON_T_SET
op_30:  ! BMI
    mov.b   @r11+, r1
    mov     r7, r0
    shlr    r0
    tst     #0xc0, r0
    BRANCH_ON_T_CLEAR
op_90:  ! BCC
    mov.b   @r11+, r1
    mov     r6, r0
    tst     #1, r0
    BRANCH_ON_T_SET
op_B0:  ! BCS
    mov.b   @r11+, r1
    mov     r6, r0
    tst     #1, r0
    BRANCH_ON_T_CLEAR




! store r1 to zero page r4, N/Z from r1
.macro ZP_STORE_NZ
    mov     r4, r0
    mov.b   r1, @(r0, r14)
    mov     r1, r7
.endm

! C (bit 0 of r6) = r2 (0 or 1)
.macro SET_C_R2
    mov     #-2, r3
    and     r3, r6
    or      r2, r6
.endm

op_26:  ! ROL zp
    ZP_LOAD
    shll    r1
    mov     r6, r0
    and     #1, r0
    or      r0, r1
    mov     r1, r2
    shlr8   r2
    extu.b  r1, r1
    SET_C_R2
    ZP_STORE_NZ
    NEXT    5
op_66:  ! ROR zp
    ZP_LOAD
    mov     r1, r2
    mov     #1, r0
    and     r0, r2
    mov     r6, r0
    and     #1, r0
    shll8   r0
    or      r0, r1
    shlr    r1
    SET_C_R2
    ZP_STORE_NZ
    NEXT    5
op_06:  ! ASL zp
    ZP_LOAD
    shll    r1
    mov     r1, r2
    shlr8   r2
    extu.b  r1, r1
    SET_C_R2
    ZP_STORE_NZ
    NEXT    5
op_46:  ! LSR zp
    ZP_LOAD
    mov     #1, r2
    and     r1, r2
    shlr    r1
    SET_C_R2
    ZP_STORE_NZ
    NEXT    5
op_C6:  ! DEC zp
    ZP_LOAD
    add     #-1, r1
    extu.b  r1, r1
    ZP_STORE_NZ
    NEXT    5
op_E6:  ! INC zp
    ZP_LOAD
    add     #1, r1
    extu.b  r1, r1
    ZP_STORE_NZ
    NEXT    5
op_8A:  ! TXA
    mov     r9, r8
    mov     r9, r7
    NEXT    2
op_98:  ! TYA
    mov     r10, r8
    mov     r10, r7
    NEXT    2
op_49:  ! EOR #
    mov.b   @r11+, r0
    extu.b  r0, r0
    xor     r0, r8
    mov     r8, r7
    NEXT    2
op_45:  ! EOR zp
    ZP_ADDR
    mov.b   @(r0, r14), r0
    extu.b  r0, r0
    xor     r0, r8
    mov     r8, r7
    NEXT    3
op_84:  ! STY zp
    ZP_ADDR
    mov.b   r10, @(r0, r14)
    NEXT    3
op_E4:  ! CPX zp
    ZP_LOAD
    COMPARE r9
    NEXT    3
op_C5:  ! CMP zp
    ZP_LOAD
    COMPARE r8
    NEXT    3
op_C0:  ! CPY #
    mov.b   @r11+, r1
    extu.b  r1, r1
    COMPARE r10
    NEXT    2
op_D9:  ! CMP abs,Y
    INDEX_ABS r10
    add     #-3, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    mov     r0, r1
    mov.l   r3, @-r15
    COMPARE r8
    mov.l   @r15+, r3
    NEXT    1
op_E9:  ! SBC # (ADC of the complement)
    mov.b   @r11+, r1
    not     r1, r1
    extu.b  r1, r1
    ADC_R1
    NEXT    2
op_E5:  ! SBC zp
    ZP_LOAD
    not     r1, r1
    extu.b  r1, r1
    ADC_R1
    NEXT    3
op_65:  ! ADC zp
    ZP_LOAD
    ADC_R1
    NEXT    3
op_6D:  ! ADC abs
    FETCH16
    add     #-3, r12
    bsr     rd_abs
    nop
    mov     r0, r1
    ADC_R1
    NEXT    1
op_7D:  ! ADC abs,X
    INDEX_ABS r9
    add     #-3, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    mov     r0, r1
    mov.l   r3, @-r15
    ADC_R1
    mov.l   @r15+, r3
    NEXT    1
op_71:  ! ADC (zp),Y
    mov.b   @r11+, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r4
    add     #1, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r0
    extu.b  r4, r4
    extu.b  r0, r0
    shll8   r0
    or      r0, r4
    mov     r4, r3
    add     r10, r4
    extu.w  r4, r4
    xor     r4, r3
    shlr8   r3
    mov     r3, r0
    and     #1, r0
    mov     r0, r3
    add     #-4, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    mov     r0, r1
    mov.l   r3, @-r15
    ADC_R1
    mov.l   @r15+, r3
    NEXT    1
op_BC:  ! LDY abs,X
    INDEX_ABS r9
    add     #-3, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    mov     r0, r10
    mov     r0, r7
    NEXT    1
op_1D:  ! ORA abs,X
    INDEX_ABS r9
    add     #-3, r12
    tst     r3, r3
    bt      97f
    DUMMY_RD
97:
    sub     r3, r12
    bsr     rd_abs
    nop
    or      r0, r8
    mov     r8, r7
    NEXT    1
op_FE:  ! INC abs,X: dummy read with only the low byte indexed, read, write the old value back, write the result
    INDEX_ABS r9
    add     #-3, r12
    DUMMY_RD
    add     #-1, r12
    bsr     rd_abs
    mov.l   r4, @(32, r15)
    mov.l   @(32, r15), r4
    add     #-1, r12
    mov.w   .Lfe_c2000, r1
    cmp/hs  r1, r4
    bf      1f
    mov.l   r0, @(36, r15)
    bsr     wr_abs
    mov     r0, r1
    ! an NMI raised by the first write is polled before the last one, so it isn't delayed (see write_byte $2000)
    mov.l   @(4, r15), r1
    mov.b   @(REG_NMI, r1), r0
    cmp/eq  #2, r0
    bf      2f
    mov     #1, r0
    mov.b   r0, @(REG_NMI, r1)
2:  mov.l   @(32, r15), r4
    mov.l   @(36, r15), r0
1:  add     #-1, r12
    add     #1, r0
    extu.b  r0, r1
    bsr     wr_abs
    mov     r1, r7
    NEXT    1
    .align 1
.Lfe_c2000: .word 0x2000
op_24:  ! BIT zp: V from bit 6, N from bit 7, Z from A & value
    ZP_LOAD
    mov     #-65, r0
    and     r0, r6
    mov     r1, r0
    and     #0x40, r0
    or      r0, r6
    mov     r1, r0
    and     #0x80, r0
    mov     r0, r2
    mov     r1, r3
    and     r8, r3
    tst     r3, r3
    bt      1f
    add     #1, r2
    mov     r2, r7
    NEXT    3
1:  shll    r2
    mov     r2, r7
    NEXT    3
op_6C:  ! JMP (ind), the pointer's high byte doesn't carry into the page
    FETCH16
    add     #-3, r12
    bsr     rd_abs
    mov.l   r4, @(32, r15)
    mov.l   r0, @(36, r15)
    mov.l   @(32, r15), r4
    mov     r4, r0
    add     #1, r0
    extu.b  r0, r0
    shlr8   r4
    shll8   r4
    add     #-1, r12
    bsr     rd_abs
    or      r0, r4
    shll8   r0
    mov.l   @(36, r15), r2
    or      r2, r0
    bsr     set_region
    extu.w  r0, r4
    NEXT    1
op_08:  ! PHP, pushes P with B and the unused bit set
    STACK_SETUP
    ! compose P from r6 and r7
    mov     #0x7d, r3
    and     r6, r3
    mov     r0, r4
    mov     r7, r0
    shlr    r0
    tst     #0xc0, r0
    bt      1f
    mov     #-128, r0
    or      r0, r3
1:  mov     r7, r0
    tst     #0xff, r0
    bf      2f
    mov     #2, r0
    or      r0, r3
2:  mov     #0x30, r0
    or      r0, r3
    mov     r4, r0
    mov.b   r3, @(r0, r14)
    add     #-1, r2
    mov     r2, r0
    mov.b   r0, @(REG_SP, r1)
    NEXT    3

!=====================================================================================================================
! Dispatch table, read for every instruction: in the linker script's .hotdata block, so its operand cache lines stay
! put when other read-only data changes size
!=====================================================================================================================
    .section .frnes_hot_rodata, "a"
    .align 5
    .global _cpu_asm_dispatch_table
_cpu_asm_dispatch_table:
dispatch_table:
! opcodes $80-$FF, then $00-$7F: the dispatch indexes it with the sign extended opcode from dispatch_middle
    .long op_fallback, op_81      , op_fallback, op_fallback, op_84      , op_85      , op_86      , op_fallback  ! 80
    .long op_88      , op_fallback, op_8A      , op_fallback, op_8C      , op_8D      , op_8E      , op_fallback  ! 88
    .long op_90      , op_91      , op_fallback, op_fallback, op_94      , op_95      , op_96      , op_fallback  ! 90
    .long op_98      , op_99      , op_9A      , op_fallback, op_fallback, op_9D      , op_fallback, op_fallback  ! 98
    .long op_A0      , op_A1      , op_A2      , op_fallback, op_A4      , op_A5      , op_A6      , op_fallback  ! A0
    .long op_A8      , op_A9      , op_AA      , op_fallback, op_AC      , op_AD      , op_AE      , op_fallback  ! A8
    .long op_B0      , op_B1      , op_fallback, op_fallback, op_B4      , op_B5      , op_B6      , op_fallback  ! B0
    .long op_B8      , op_B9      , op_BA      , op_fallback, op_BC      , op_BD      , op_BE      , op_fallback  ! B8
    .long op_C0      , op_C1      , op_fallback, op_fallback, op_C4      , op_C5      , op_C6      , op_fallback  ! C0
    .long op_C8      , op_C9      , op_CA      , op_fallback, op_CC      , op_CD      , op_CE      , op_fallback  ! C8
    .long op_D0      , op_D1      , op_fallback, op_fallback, op_fallback, op_D5      , op_D6      , op_fallback  ! D0
    .long op_D8      , op_D9      , op_fallback, op_fallback, op_fallback, op_DD      , op_DE      , op_fallback  ! D8
    .long op_E0      , op_E1      , op_fallback, op_fallback, op_E4      , op_E5      , op_E6      , op_fallback  ! E0
    .long op_E8      , op_E9      , op_EA      , op_fallback, op_EC      , op_ED      , op_EE      , op_fallback  ! E8
    .long op_F0      , op_F1      , op_fallback, op_fallback, op_fallback, op_F5      , op_F6      , op_fallback  ! F0
    .long op_F8      , op_F9      , op_fallback, op_fallback, op_fallback, op_FD      , op_FE      , op_fallback  ! F8
dispatch_middle:
    .long op_fallback, op_01      , op_fallback, op_fallback, op_fallback, op_05      , op_06      , op_fallback  ! 00
    .long op_08      , op_09      , op_0A      , op_fallback, op_fallback, op_0D      , op_0E      , op_fallback  ! 08
    .long op_10      , op_11      , op_fallback, op_fallback, op_fallback, op_15      , op_16      , op_fallback  ! 10
    .long op_18      , op_19      , op_fallback, op_fallback, op_fallback, op_1D      , op_1E      , op_fallback  ! 18
    .long op_20      , op_21      , op_fallback, op_fallback, op_24      , op_25      , op_26      , op_fallback  ! 20
    .long op_28      , op_29      , op_2A      , op_fallback, op_2C      , op_2D      , op_2E      , op_fallback  ! 28
    .long op_30      , op_31      , op_fallback, op_fallback, op_fallback, op_35      , op_36      , op_fallback  ! 30
    .long op_38      , op_39      , op_fallback, op_fallback, op_fallback, op_3D      , op_3E      , op_fallback  ! 38
    .long op_40      , op_41      , op_fallback, op_fallback, op_fallback, op_45      , op_46      , op_fallback  ! 40
    .long op_48      , op_49      , op_4A      , op_fallback, op_4C      , op_4D      , op_4E      , op_fallback  ! 48
    .long op_50      , op_51      , op_fallback, op_fallback, op_fallback, op_55      , op_56      , op_fallback  ! 50
    .long op_58      , op_59      , op_fallback, op_fallback, op_fallback, op_5D      , op_5E      , op_fallback  ! 58
    .long op_60      , op_61      , op_fallback, op_fallback, op_fallback, op_65      , op_66      , op_fallback  ! 60
    .long op_68      , op_69      , op_6A      , op_fallback, op_6C      , op_6D      , op_6E      , op_fallback  ! 68
    .long op_70      , op_71      , op_fallback, op_fallback, op_fallback, op_75      , op_76      , op_fallback  ! 70
    .long op_78      , op_79      , op_fallback, op_fallback, op_fallback, op_7D      , op_7E      , op_fallback  ! 78

!=====================================================================================================================
! The rest of the official opcodes: (zp,X), BIT abs, BVC / BVS, the zp,X / abs / abs,X read-modify-writes, PLP, CLI,
! SEI and RTI. They are rare in the profiles, so they live outside the hot group (.text.hot.1_core keeps its size and
! layout) and reach dispatch and the access helpers through the trampolines below. CLI / SEI / PLP with an IRQ
! pending leave the poll to the C core (IRQ_Poll, the I flag from before them) and end the run, as does RTI when it
! unmasks a pending IRQ; BRK stays with the C core (its vector can be taken over by an NMI, which needs the devices
! brought up to that cycle).
!=====================================================================================================================
    .section .text.cpu_cold, "ax"
    .align 5

.equ REG_IRQ_POLL, 20

cold_next:
    mov.l   .Lcold_dispatch, r1
    jmp     @r1
    nop
cold_exit:
    mov.l   .Lcold_exit_asm, r1
    jmp     @r1
    nop
cold_rd_abs:
    mov.l   .Lcold_rd_abs, r2
    jmp     @r2
    nop
cold_wr_abs:
    mov.l   .Lcold_wr_abs, r2
    jmp     @r2
    nop
cold_set_region:
    mov.l   .Lcold_set_region, r2
    jmp     @r2
    nop
cold_dummy_io:
    mov.l   .Lcold_dummy_io, r2
    jmp     @r2
    nop
cold_branch_taken:
    mov.l   .Lcold_branch_taken, r2
    jmp     @r2
    nop
    .align 2
.Lcold_dispatch:        .long dispatch
.Lcold_exit_asm:        .long exit_asm
.Lcold_rd_abs:          .long rd_abs
.Lcold_wr_abs:          .long wr_abs
.Lcold_set_region:      .long set_region
.Lcold_dummy_io:        .long dummy_io
.Lcold_branch_taken:    .long branch_taken

.macro COLD_NEXT n
    bra     cold_next
    add     #-\n, r12
.endm

! DUMMY_RD through the trampoline
.macro COLD_DUMMY_RD
    mov     r3, r0
    shll8   r0
    mov     r4, r1
    sub     r0, r1
    mov     #0x20, r0
    shll8   r0
    sub     r0, r1
    mov     #0x60, r0
    shll8   r0
    cmp/hs  r0, r1
    bt      98f
    bsr     cold_dummy_io
    nop
98:
.endm

! (zp,X): the pointer at (operand + X) & $FF, both bytes in zero page; r4 = the address
.macro ADDR_IZX
    mov.b   @r11+, r0
    add     r9, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r4
    add     #1, r0
    extu.b  r0, r0
    mov.b   @(r0, r14), r0
    extu.b  r4, r4
    extu.b  r0, r0
    shll8   r0
    or      r0, r4
.endm

! (zp,X) read, 6 cycles: the read on the last one
.macro RD_IZX
    ADDR_IZX
    add     #-5, r12
    bsr     cold_rd_abs
    nop
.endm

! zp,X: r4 = the address, r1 = the value
.macro ZPX_LOAD
    mov.b   @r11+, r0
    add     r9, r0
    extu.b  r0, r0
    mov     r0, r4
    mov.b   @(r0, r14), r1
    extu.b  r1, r1
.endm

! shifts of r0 (0-255) into r1, C from the bit shifted out
.macro SH_ASL
    mov     r0, r1
    shll    r1
    mov     r1, r2
    shlr8   r2
    extu.b  r1, r1
    SET_C_R2
.endm
.macro SH_LSR
    mov     #1, r2
    and     r0, r2
    mov     r0, r1
    shlr    r1
    SET_C_R2
.endm
.macro SH_ROL
    mov     r0, r1
    shll    r1
    mov     r6, r0
    and     #1, r0
    or      r0, r1
    mov     r1, r2
    shlr8   r2
    extu.b  r1, r1
    SET_C_R2
.endm
.macro SH_ROR
    mov     r0, r1
    mov     #1, r2
    and     r1, r2
    mov     r6, r0
    and     #1, r0
    shll8   r0
    or      r0, r1
    shlr    r1
    SET_C_R2
.endm

! read-modify-write of memory once r4 holds the address and the cycles up to the read are taken (as RMW_INCDEC):
! read, write the old value back ($2000 up), write the result of \op (r0 -> r1)
.macro COLD_RMW op
    bsr     cold_rd_abs
    mov.l   r4, @(32, r15)
    mov.l   @(32, r15), r4
    add     #-1, r12
    mov.w   99f, r1
    cmp/hs  r1, r4
    bf      1f
    mov.l   r0, @(36, r15)
    bsr     cold_wr_abs
    mov     r0, r1
    ! an NMI raised by the first write is polled before the last one, so it isn't delayed (see write_byte $2000)
    mov.l   @(4, r15), r1
    mov.b   @(REG_NMI, r1), r0
    cmp/eq  #2, r0
    bf      2f
    mov     #1, r0
    mov.b   r0, @(REG_NMI, r1)
2:  mov.l   @(32, r15), r4
    mov.l   @(36, r15), r0
1:  add     #-1, r12
    \op
    bsr     cold_wr_abs
    mov     r1, r7
    COLD_NEXT 1
    .align 1
99: .word 0x2000
.endm

! zp,X read-modify-write, 6 cycles
.macro ZPX_RMW op
    ZPX_LOAD
    mov     r1, r0
    \op
    ZP_STORE_NZ
    COLD_NEXT 6
.endm

! P from r0 into r6 / r7 (as cpu_run_asm's entry)
.macro P_FROM_R0
    extu.b  r0, r6
    and     #0x80, r0
    mov     r0, r7
    mov     r6, r0
    tst     #2, r0
    bt      91f
    bra     92f
    shll    r7
91: add     #1, r7
92:
.endm

! with an IRQ pending, IRQ_Poll from the I flag before the instruction (r6): IRQ_POLL_I_SET or IRQ_POLL_I_CLEAR.
! r1 = cpu_registers, r3 = IRQ_Request (non-zero when an IRQ is pending)
.macro IRQ_POLL_BEFORE
    mov.l   @(4, r15), r1
    mov.b   @(REG_IRQ, r1), r0
    extu.b  r0, r3
    tst     r0, r0
    bt      93f
    mov     r6, r0
    tst     #4, r0
    movt    r0
    add     #1, r0
    mov     r1, r2
    add     #REG_IRQ_POLL, r2
    mov.b   r0, @r2
93:
.endm

! (zp,X)
op_01:  ! ORA (zp,X)
    RD_IZX
    OP_ORA
    COLD_NEXT 1
op_21:  ! AND (zp,X)
    RD_IZX
    OP_AND
    COLD_NEXT 1
op_41:  ! EOR (zp,X)
    RD_IZX
    OP_EOR
    COLD_NEXT 1
op_61:  ! ADC (zp,X)
    RD_IZX
    OP_ADC
    COLD_NEXT 1
op_A1:  ! LDA (zp,X)
    RD_IZX
    mov     r0, r8
    mov     r0, r7
    COLD_NEXT 1
op_C1:  ! CMP (zp,X)
    RD_IZX
    OP_CMP  r8
    COLD_NEXT 1
op_E1:  ! SBC (zp,X)
    RD_IZX
    OP_SBC
    COLD_NEXT 1
op_81:  ! STA (zp,X), the write on the last cycle
    ADDR_IZX
    add     #-5, r12
    bsr     cold_wr_abs
    mov     r8, r1
    COLD_NEXT 1

op_2C:  ! BIT abs: V from bit 6, N from bit 7, Z from A & value
    FETCH16
    add     #-3, r12
    bsr     cold_rd_abs
    nop
    mov     r0, r1
    mov     #-65, r0
    and     r0, r6
    mov     r1, r0
    and     #0x40, r0
    or      r0, r6
    mov     r1, r0
    and     #0x80, r0
    mov     r0, r2
    mov     r1, r3
    and     r8, r3
    tst     r3, r3
    bt      1f
    add     #1, r2
    mov     r2, r7
    COLD_NEXT 1
1:  shll    r2
    mov     r2, r7
    COLD_NEXT 1

op_50:  ! BVC (taken when V clear)
    mov.b   @r11+, r1
    mov     r6, r0
    tst     #0x40, r0
    bf      1f
    bra     cold_branch_taken
    nop
1:  COLD_NEXT 2
op_70:  ! BVS
    mov.b   @r11+, r1
    mov     r6, r0
    tst     #0x40, r0
    bt      1f
    bra     cold_branch_taken
    nop
1:  COLD_NEXT 2

! zp,X read-modify-writes
op_16:  ! ASL zp,X
    ZPX_RMW SH_ASL
op_36:  ! ROL zp,X
    ZPX_RMW SH_ROL
op_56:  ! LSR zp,X
    ZPX_RMW SH_LSR
op_76:  ! ROR zp,X
    ZPX_RMW SH_ROR
op_D6:  ! DEC zp,X
    ZPX_LOAD
    add     #-1, r1
    extu.b  r1, r1
    ZP_STORE_NZ
    COLD_NEXT 6
op_F6:  ! INC zp,X
    ZPX_LOAD
    add     #1, r1
    extu.b  r1, r1
    ZP_STORE_NZ
    COLD_NEXT 6

! abs read-modify-writes, 6 cycles
op_0E:  ! ASL abs
    FETCH16
    add     #-3, r12
    COLD_RMW SH_ASL
op_2E:  ! ROL abs
    FETCH16
    add     #-3, r12
    COLD_RMW SH_ROL
op_4E:  ! LSR abs
    FETCH16
    add     #-3, r12
    COLD_RMW SH_LSR
op_6E:  ! ROR abs
    FETCH16
    add     #-3, r12
    COLD_RMW SH_ROR

! abs,X read-modify-writes, 7 cycles: dummy read with only the low byte indexed first (as op_DE)
op_1E:  ! ASL abs,X
    INDEX_ABS r9
    add     #-3, r12
    COLD_DUMMY_RD
    add     #-1, r12
    COLD_RMW SH_ASL
op_3E:  ! ROL abs,X
    INDEX_ABS r9
    add     #-3, r12
    COLD_DUMMY_RD
    add     #-1, r12
    COLD_RMW SH_ROL
op_5E:  ! LSR abs,X
    INDEX_ABS r9
    add     #-3, r12
    COLD_DUMMY_RD
    add     #-1, r12
    COLD_RMW SH_LSR
op_7E:  ! ROR abs,X
    INDEX_ABS r9
    add     #-3, r12
    COLD_DUMMY_RD
    add     #-1, r12
    COLD_RMW SH_ROR

op_58:  ! CLI: the poll during it still sees I set
    IRQ_POLL_BEFORE
    mov     #-5, r0
    and     r0, r6
    tst     r3, r3
    bf      1f
    COLD_NEXT 2
1:  add     #-2, r12
    bra     cold_exit
    nop
op_78:  ! SEI: the poll during it still sees I clear
    IRQ_POLL_BEFORE
    mov     r6, r0
    or      #4, r0
    mov     r0, r6
    tst     r3, r3
    bf      1f
    COLD_NEXT 2
1:  add     #-2, r12
    bra     cold_exit
    nop

op_28:  ! PLP: P = (pulled & ~B) | R, the poll during it still sees the old I
    IRQ_POLL_BEFORE
    STACK_SETUP
    add     #1, r2
    extu.b  r2, r2
    mov     r2, r0
    mov.b   r0, @(REG_SP, r1)
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   @(r0, r14), r0
    and     #0xef, r0
    or      #0x20, r0
    P_FROM_R0
    tst     r3, r3
    bf      1f
    COLD_NEXT 4
1:  add     #-4, r12
    bra     cold_exit
    nop

op_40:  ! RTI: P, then PC; an IRQ it unmasks is taken by the C core before the next instruction
    STACK_SETUP
    add     #1, r2
    extu.b  r2, r2
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   @(r0, r14), r3
    add     #1, r2
    extu.b  r2, r2
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   @(r0, r14), r4
    extu.b  r4, r4
    add     #1, r2
    extu.b  r2, r2
    mov     #1, r0
    shll8   r0
    add     r2, r0
    mov.b   @(r0, r14), r0
    extu.b  r0, r0
    shll8   r0
    or      r0, r4
    mov     r2, r0
    mov.b   r0, @(REG_SP, r1)
    mov     r3, r0
    and     #0xef, r0
    or      #0x20, r0
    P_FROM_R0
    bsr     cold_set_region
    nop
    mov.l   @(4, r15), r1
    mov.b   @(REG_IRQ, r1), r0
    tst     r0, r0
    bt      1f
    mov     r6, r0
    tst     #4, r0
    bt      2f
1:  COLD_NEXT 6
2:  add     #-6, r12
    bra     cold_exit
    nop
