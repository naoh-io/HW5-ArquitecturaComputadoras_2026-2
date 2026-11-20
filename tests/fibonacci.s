
.global _start
_start:
    la   a0, msg          # a0 = direccion del string
    li   a7, 4            # ecall 4: print string
    ecall

    li   t0, 0            # a = 0
    li   t1, 1            # b = 1
    li   t2, 0            # i = 0
    li   t3, 10           # n = 10

loop:
    mv   a0, t0           # imprimir a
    li   a7, 1            # ecall 1: print int
    ecall

    li   a0, 32           # imprimir ' '
    li   a7, 11           # ecall 11: print char
    ecall

    add  t4, t0, t1       # tmp = a + b
    mv   t0, t1           # a = b
    mv   t1, t4           # b = tmp
    addi t2, t2, 1        # i++
    blt  t2, t3, loop     # while (i < n)

    li   a0, 10           # imprimir '\n'
    li   a7, 11
    ecall

    li   a0, 0            # codigo de salida 0
    li   a7, 10           # ecall 10: exit
    ecall

msg:
    .asciz "Fibonacci: "
