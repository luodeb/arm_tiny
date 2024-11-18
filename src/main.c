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
void timer_handler(uint64_t *)
{
    asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(50000000));
    tiny_printf("irq %d, count %d\n", TIMER, count++);
}

int kernel_main(void)
{
    tiny_hello();

    // tiny_io_init();
    irq_handle_register(TIMER, timer_handler);
    // handle_init();
    // gic_init();
    timer_gic_init();
    // enable_interrupts();
    asm volatile("msr daifclr, #2" : : : "memory");
    asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(50000000));
    asm volatile("msr CNTP_CTL_EL0, %0" : : "r"(1));

    while (1)
    {
    }
    return 0;
}
