.text
clear_led:
    li   t0, LED_MATRIX_0_BASE
    li   t1, LED_MATRIX_0_HEIGHT
    li   t2, LED_MATRIX_0_WIDTH

clear_led_row:
    mv   t3, t2              # 這一列還有 WIDTH 顆 LED

clear_led_column:
    sw   zero, 0(t0)         # RGB = 0，黑色
    addi t0, t0, 4           # 下一顆 LED

    addi t3, t3, -1
    bnez t3, clear_led_column

    addi t1, t1, -1
    bnez t1, clear_led_row

    ret

draw_facelet:
    li   t0, LED_MATRIX_0_BASE
    li   t1, LED_MATRIX_0_WIDTH

    # LED 索引 = y * WIDTH + x
    mv   t2, a0              # 先放入 x
    mv   t3, a1              # 要累加 y 次 WIDTH

facelet_y_loop:
    beqz t3, facelet_address_ready

    add  t2, t2, t1
    addi t3, t3, -1
    j    facelet_y_loop

facelet_address_ready:
    slli t2, t2, 2           # LED 索引 × 4 bytes
    add  t0, t0, t2          # 色塊左上角位址

    slli t1, t1, 2           # 一整列占 WIDTH × 4 bytes
    li   t4, 3               # 畫三列

facelet_row_loop:
    sw   a2, 0(t0)
    sw   a2, 4(t0)
    sw   a2, 8(t0)
    sw   a2, 12(t0)          # 每列畫四顆

    add  t0, t0, t1          # 往下移一列
    addi t4, t4, -1
    bnez t4, facelet_row_loop

    ret
render_cube:
    # 本函式會呼叫 clear_led、draw_facelet
    addi sp, sp, -32
    sw   ra, 28(sp)
    sw   s0, 0(sp)
    sw   s1, 4(sp)
    sw   s2, 8(sp)
    sw   s3, 12(sp)

    mv   s0, a0              # cube_p 位址
    mv   s1, a1              # cube_o 位址
    la   s2, led_facelets    # 目前這筆繪圖資料
    li   s3, 24             # 剩餘色塊數

    jal  ra, clear_led

render_facelet_loop:
    # 每筆資料：[position, slot, x, y]
    lbu  t0, 0(s2)           # position

    li   t1, 7
    beq  t0, t1, render_fixed_corner

    # 一般位置：從目前方塊狀態讀取
    add  t1, s0, t0
    lbu  t2, 0(t1)           # t2 = cubie

    add  t1, s1, t0
    lbu  t3, 0(t1)           # t3 = orientation
    j    render_choose_color

render_fixed_corner:
    # 固定角塊不在 cube_p、cube_o 裡
    li   t2, 7               # 固定角塊的繪圖編號
    li   t3, 0               # 朝向固定為 0

render_choose_color:
    lbu  t4, 1(s2)           # t4 = slot
    add  t3, t3, t4          # orientation + slot

    # 兩者都在 0..2，總和最多 4
    # 求 (orientation + slot) % 3
    addi t3, t3, -3
    bgez t3, render_color_index_ready
    addi t3, t3, 3

render_color_index_ready:
    # 每個 cubie 有三個顏色
    # 位移 = cubie * 3 + 顏色索引
    slli t4, t2, 1
    add  t4, t4, t2
    add  t4, t4, t3

    la   t5, led_cubie_colors
    add  t5, t5, t4
    lbu  t4, 0(t5)           # t4 = 顏色編號 0..5

    # 從 palette 取得 32-bit RGB
    slli t4, t4, 2
    la   t5, led_palette
    add  t5, t5, t4
    lw   a2, 0(t5)           # a2 = RGB

    # 呼叫 draw_facelet(x, y, RGB)
    lbu  a0, 2(s2)           # x
    lbu  a1, 3(s2)           # y
    jal  ra, draw_facelet

    # 下一筆繪圖資料
    addi s2, s2, 4
    addi s3, s3, -1
    bnez s3, render_facelet_loop

    lw   s0, 0(sp)
    lw   s1, 4(sp)
    lw   s2, 8(sp)
    lw   s3, 12(sp)
    lw   ra, 28(sp)
    addi sp, sp, 32
    ret

.data
.align 4
led_palette:
    .word 0xFFFFFF           # 0：U 白
    .word 0xFFFF00           # 1：D 黃
    .word 0x00BB00           # 2：F 綠
    .word 0x0044FF           # 3：B 藍
    .word 0xFF0000           # 4：R 紅
    .word 0xFF8800           # 5：L 橘

led_cubie_colors:
    .byte 0, 4, 2            # cubie 0：U R F
    .byte 1, 2, 4            # cubie 1：D F R
    .byte 1, 5, 2            # cubie 2：D L F
    .byte 0, 3, 4            # cubie 3：U B R
    .byte 1, 4, 3            # cubie 4：D R B
    .byte 1, 3, 5            # cubie 5：D B L
    .byte 0, 5, 3            # cubie 6：U L B
    .byte 0, 2, 5            # cubie 7：U F L，固定角塊

led_facelets:
    # U 面
    .byte 6, 0,  9,  0
    .byte 3, 0, 13,  0
    .byte 7, 0,  9,  3
    .byte 0, 0, 13,  3

    # L 面
    .byte 6, 1,  0,  7
    .byte 7, 2,  4,  7
    .byte 5, 2,  0, 10
    .byte 2, 1,  4, 10

    # F 面
    .byte 7, 1,  9,  7
    .byte 0, 2, 13,  7
    .byte 2, 2,  9, 10
    .byte 1, 1, 13, 10

    # R 面
    .byte 0, 1, 18,  7
    .byte 3, 2, 22,  7
    .byte 1, 2, 18, 10
    .byte 4, 1, 22, 10

    # B 面
    .byte 3, 1, 27,  7
    .byte 6, 2, 31,  7
    .byte 4, 2, 27, 10
    .byte 5, 1, 31, 10

    # D 面
    .byte 2, 0,  9, 14
    .byte 1, 0, 13, 14
    .byte 5, 0,  9, 17
    .byte 4, 0, 13, 17