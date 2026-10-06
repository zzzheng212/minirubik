.data
# 練習用資料，不是真正的 cube heuristic。
dist_p:
    .byte 0, 2, 4, 1

dist_o:
    .byte 0, 3, 1, 2

.text
main:
    # 測試 dist_p[2] 與 dist_o[1]
    li a0, 2
    li a1, 1

    # 呼叫 heuristic；返回位址放在 ra
    jal ra, heuristic

    # 把回傳值留下來，方便在 Ripes 查看
    mv s0, a0

    # 結束程式
    li a7, 10
    ecall


heuristic:
    # 第一步：讀取 dist_p[a0]
    la t0, dist_p
    add t0, t0, a0 
    lbu t1, 0(t0) 

    # 第二步：讀取 dist_o[a1]
    la t0, dist_o
    add t0, t0, a1
    lbu t2, 0(t0)

    # 第三步：取較大值
    mv a0, t1
    bgeu t1, t2, heuristic_dones
    mv a0, t2
heuristic_done:
    ret