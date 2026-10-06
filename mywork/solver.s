.text
apply_move:
    # 本函式會呼叫 quarter_turn，必須保存 ra
    # s0 用來保存剩餘轉動次數，也必須保存原值
    addi sp, sp, -16
    sw   ra, 12(sp)
    sw   s0, 8(sp)

    # 分解 move：
    # t0 最後留下 move % 3
    # a2 最後得到 move / 3，也就是 face
    mv   t0, a2
    li   a2, 0
    li   t1, 3

move_decode_loop:
    bltu t0, t1, move_decode_done
    addi t0, t0, -3
    addi a2, a2, 1
    j    move_decode_loop

move_decode_done:
    addi s0, t0, 1           # turns = 餘數 + 1

move_turn_loop:
    jal  ra, quarter_turn

    addi s0, s0, -1
    bnez s0, move_turn_loop

    lw   s0, 8(sp)
    lw   ra, 12(sp)
    addi sp, sp, 16
    ret