.global _init_6502
.align 2
_init_6502:
    mov.l printf_addr, r0
    mov.l hello_addr, r4
    mov.l name_addr, r5
    sts.l pr, @-r15
    jsr   @r0
    nop 

! we're back
    lds.l @r15+, pr
    rts
    mov #0, r0

.align 4
printf_addr: .long _printf
name_addr: .long name
hello_addr: .long hello
name: .ascii "Matt"
hello: .asciz "Hello World, %s\n"
