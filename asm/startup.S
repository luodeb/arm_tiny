// startup.S
.section .text
.global _start

.section .text.startup, "x"
_start:
    mov x8, #97
    mov x9, #0x09000000 //串口地址，需要变化
    str x8, [x9]
    
    msr daifset, #2   // 关闭所有中断

    adrp    x0, exception_vector_base
    add     x0, x0, :lo12:exception_vector_base
    msr     vbar_el1, x0
    dsb     sy      // 确保所有内存访问完成
    isb             // 确保所有指令都执行完成

    // 4. 读取当前的SCTLR_EL1寄存器值
    mrs x0, sctlr_el1
    
    // 5. 清除MMU使能位 (M bit - bit 0)
    bic x0, x0, #0x1
    
    // 6. 清除数据缓存使能位 (C bit - bit 2)
    bic x0, x0, #0x4
    
    // 7. 清除指令缓存使能位 (I bit - bit 12)
    bic x0, x0, #0x1000
    
    // 8. 清除分支预测使能位 (Z bit - bit 11) [可选]
    bic x0, x0, #0x800
    
    // 9. 写回SCTLR_EL1寄存器
    msr sctlr_el1, x0
    
    // 10. 指令同步屏障，确保设置生效
    isb
    
    // 11. 数据同步屏障
    dsb sy


    // 设置栈指针
    ldr x0, =_stack_top
    mov sp, x0

    // 调用 C 语言的 main 函数
    bl kernel_main

    // 死循环，防止返回
1:  b 1b

// 栈空间
.section .bss
.align 12
.global _stack_top
_stack_top:
    .skip 0x8000  // 8KB 的栈空间
