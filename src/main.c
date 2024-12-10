/*
 * File: main.c
 * Author: Debin
 * Date: 2024-11-15
 * Description: Main file for the ARM Tiny project.
 */

#include "tiny_io.h"
#include "handle.h"
#include "gicv2.h"

void timer_gic_init(void)
{
    write32(GICD_CTRL_ENABLE_GROUP0 | GICD_CTRL_ENABLE_GROUP1, (void *)GICD_CTLR);

    // 允许所有优先级的中断
    write32(0xff - 7, (void *)GICC_PMR);
    write32(GICC_CTRL_ENABLE | (1 << 9), (void *)GICC_CTLR);

    uint32_t value;

    value = read32((void *)(uint32_t)GICD_ISENABLER(0));
    value |= (1 << 30);
    write32(value, (void *)(uint32_t)GICD_ISENABLER(0));
}

static int count = 0;
uint64_t timer_intgerval = 500000000;
void timer_handler(uint64_t *,uint64_t irq)
{
    asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(timer_intgerval));
    tiny_printf(INFO, "irq %d, count %d\n", TIMER, count++);
}

int irqs[512] = {0};
void irq_handler(uint64_t *,uint64_t irq)
{
    if (irqs[irq] == 0) {
        irqs[irq] = 1;
        tiny_printf(INFO, "HANDLE: irq %d\n", irq);
    }else{
        irqs[irq]++;
    }
}

// void trigger_irq(uint32_t irq_num) {
//     // 计算 GIC 分布寄存器的偏移量
//     int offset = irq_num / 32;
//     int bit_pos = 1 << (irq_num % 32);

//     // 设置 pending 位
//     write32(bit_pos, (void *)(uint64_t)GICD_ISPENDER(offset));
//     // *((volatile uint32_t *)(GICD_ISPENDER(offset))) |= (1 << bit_pos);
// }



int kernel_main(void)
{
    tiny_printf(INFO, "\nHello, ARM Tiny!\n");

    irq_handle_register(TIMER, timer_handler);
    timer_gic_init();

    asm volatile("msr daifclr, #2");
    asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(timer_intgerval));
    asm volatile("msr CNTP_CTL_EL0, %0" : : "r"(0));

    while (1)
    {
    }
    return 0;
}
