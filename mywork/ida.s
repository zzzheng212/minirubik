.text
solve_ida:
    # 保存返回位址與我們要使用的 s 暫存器
    addi sp, sp, -32
    sw   ra, 28(sp)
    sw   s0, 0(sp)
    sw   s1, 4(sp)
    sw   s2, 8(sp)
    sw   s3, 12(sp)
    sw   s4, 16(sp)
    sw   s5, 20(sp)

    # 保存原始輸入，因為 heuristic 會改寫 a0
    mv   s4, a0
    mv   s5, a1

    # 第一輪 bound = heuristic(root)
    jal  ra, heuristic
    mv   s2, a0

    # 兩個 rank 都是 0，代表本來就 solved
    or   t0, s4, s5
    beqz t0, ida_already_solved

ida_iteration:
    # 超過最大允許深度，就回傳失敗
    li   t0, 11
    bltu t0, s2, ida_failed

    # 每輪從根節點重新開始
    la   s0, search_frames
    li   s1, 0               # depth = 0
    li   s3, 255             # next_bound 尚未找到

    sh   s4, 0(s0)           # 根節點排列 rank
    sh   s5, 2(s0)           # 根節點朝向 rank
    sh   s4, 4(s0)           # 初始化轉動暫存
    sh   s5, 6(s0)
    sb   zero, 8(s0)         # 從 move 0 開始

    li   t0, 3
    sb   t0, 9(s0)           # face 只有 0、1、2；3 表示沒有禁用面

ida_search_loop:
    ida_search_loop:
    # 取得這一層下一個要嘗試的 move
    lbu  t0, 8(s0)

    # move 已經試完 0..8，退回上一層
    li   t1, 9
    beq  t0, t1, ida_backtrack

    # 非 solved 狀態不繼續往深度 11 以下展開
    li   t1, 11
    beq  s1, t1, ida_backtrack

    # 將 move 拆成：
    # t1 = face，0=R、1=B、2=D
    # t2 = 同一面內的編號，0=90°、1=180°、2=270°
    li   t1, 0
    mv   t2, t0
    li   t3, 3

ida_decode_face:
    bltu t2, t3, ida_face_ready
    addi t2, t2, -3
    addi t1, t1, 1
    j    ida_decode_face

ida_face_ready:
    # 若和上一個 move 相同面，直接跳過整個面
    lbu  t3, 9(s0)
    beq  t1, t3, ida_skip_face

    # 先記住下次要試哪個 move
    addi t3, t0, 1
    sb   t3, 8(s0)

    # 同一面的第二、第三個 move，沿用前一次轉動結果
    bnez t2, ida_use_cached

    # 該面的第一個 move，要從本層原始狀態開始
    lhu  a0, 0(s0)
    lhu  a1, 2(s0)
    j    ida_select_table

ida_use_cached:
    lhu  a0, 4(s0)
    lhu  a1, 6(s0)

ida_select_table:
    beqz t1, ida_table_r

    li   t3, 1
    beq  t1, t3, ida_table_b

    # face = 2：D
    la   a2, perm_2
    la   a3, ori_2
    j    ida_turn

ida_table_r:
    la   a2, perm_0
    la   a3, ori_0
    j    ida_turn

ida_table_b:
    la   a2, perm_1
    la   a3, ori_1

ida_turn:
    jal  ra, quarter_step

    # 保存新 rank：
    # heuristic 會把 a0 改成距離，因此必須先保存
    sh   a0, 4(s0)
    sh   a1, 6(s0)

    jal  ra, heuristic

    # f = depth + 1 + heuristic(child)
    add  t0, s1, a0
    addi t0, t0, 1

    # f > bound，剪掉這條分支
    bltu s2, t0, ida_prune

    # 這個 child 可以繼續搜尋
    # next_move 已經加過 1，因此目前選中的 move 是它減 1
    lbu  t0, 8(s0)
    addi t0, t0, -1
    sb   t0, 10(s0)

    # 取回 child 的兩個 rank
    lhu  t1, 4(s0)
    lhu  t2, 6(s0)

    addi s1, s1, 1           # child 的深度

    # 兩個 rank 都是 0 → 找到 solved
    or   t3, t1, t2
    beqz t3, ida_found

    # 計算目前選中的 move 屬於哪個面
    # 這個面將成為 child 的禁止面
    li   t4, 0               # 預設 R
    li   t3, 3
    bltu t0, t3, ida_push

    li   t4, 1               # B
    li   t3, 6
    bltu t0, t3, ida_push

    li   t4, 2               # D

ida_push:
    # 進入下一層，每個 frame 占 16 bytes
    addi s0, s0, 16

    sh   t1, 0(s0)           # child 排列 rank
    sh   t2, 2(s0)           # child 朝向 rank
    sh   t1, 4(s0)           # 初始化轉動暫存
    sh   t2, 6(s0)
    sb   zero, 8(s0)         # 下一層從 move 0 開始
    sb   t4, 9(s0)           # 禁止接著轉同一面

    j    ida_search_loop

ida_prune:
    # next_bound = min(next_bound, f)
    bgeu t0, s3, ida_search_loop
    mv   s3, t0
    j    ida_search_loop

ida_skip_face:
    # t0 = move，t2 = move 在同一面內的編號
    # 下一面的第一個 move = move - t2 + 3
    sub  t0, t0, t2
    addi t0, t0, 3
    sb   t0, 8(s0)
    j    ida_search_loop

ida_backtrack:
    # 根節點也試完了 → 這一輪結束
    beqz s1, ida_next_iteration

    # 否則退回上一層
    addi s1, s1, -1
    addi s0, s0, -16
    j    ida_search_loop

ida_next_iteration:
    mv   s2, s3              # bound = next_bound
    j    ida_iteration

ida_found:
    # s1 現在是找到解答的深度，也就是解答長度
    mv   a0, s1

    # 將每一層保存的 selected_move 複製到 path
    la   t0, search_frames
    la   t1, path
    mv   t2, s1

ida_copy_path:
    beqz t2, ida_return

    lbu  t3, 10(t0)
    sb   t3, 0(t1)

    addi t0, t0, 16
    addi t1, t1, 1
    addi t2, t2, -1
    j    ida_copy_path

ida_already_solved:
    li   a0, 0               # 不需要任何 move
    j    ida_return

ida_failed:
    li   a0, -1

ida_return:
    lw   s0, 0(sp)
    lw   s1, 4(sp)
    lw   s2, 8(sp)
    lw   s3, 12(sp)
    lw   s4, 16(sp)
    lw   s5, 20(sp)
    lw   ra, 28(sp)
    addi sp, sp, 32
    ret