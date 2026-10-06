.text
.globl main
main:
    li t0, 0x10000000
    li t1, 0
    li t2, 4095
    li t3, 262144
    li t4, 0x12345678
    li s0, 0
write_loop:
    and t5, t1, t2
    add t5, t0, t5
    sw t4, 0(t5)
    addi t1, t1, 4
    addi t3, t3, -1
    bne t3, zero, write_loop
    lw t6, 0(t5)
    bne t6, t4, finish
    li s0, 1
finish:
    li a7, 10
    ecall
