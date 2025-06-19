/*
 * File: main.c
 * Author: Debin
 * Date: 2024-11-15
 * Description: Main file for the ARM Tiny project.
 */

#include "tiny_io.h"
#include "handle.h"
#include "gicv2.h"

#ifndef VM_VERSION
#define VM_VERSION "null"
#endif

int timer_interval = 60000000;

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
    asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(timer_interval));
    tiny_printf(INFO, "irq %d, count %d\n\n", TIMER, count++);
}

int kernel_main(void)
{
    tiny_printf(INFO, "\nHello, ARM Tiny VM%s!\n", VM_VERSION);

    while (1)
    {
    }
    return 0;
}
