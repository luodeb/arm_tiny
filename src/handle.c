/*
 * exception.c
 *
 * Exception handling for ARM architecture.
 *
 * This file contains the implementation of exception handling functions
 * including synchronous exceptions, IRQ exceptions, and invalid exceptions.
 * It also includes the initialization function for setting up the exception
 * handling mechanism.
 *
 * Author: Debin
 * Date: 2024-11-15
 */

#include "tiny_types.h"
#include "handle.h"
#include "tiny_io.h"
#include "gicv2.h"

irq_handler_t g_handler_vec[512] = {0};

void irq_handle_register(int vector, void (*h)(uint64_t *))
{
    g_handler_vec[vector] = h;
}

void handle_sync_exception(uint64_t *stack_pointer)
{
    trap_frame_t *el1_ctx = (trap_frame_t *)stack_pointer;

    int el1_esr = read_esr_el1();

    int ec = ((el1_esr >> 26) & 0b111111);

    tiny_printf(INFO, "el1 esr: %d\n", el1_esr);
    tiny_printf(INFO, "ec: %x\n", ec);

    tiny_printf(INFO, "This is handle_sync_exception: \n");
    for (int i = 0; i < 31; i++)
    {
        uint64_t value = el1_ctx->r[i];
        tiny_printf(INFO, "General-purpose register: %d, value: %x\n", i, value);
    }

    uint64_t elr_el1_value = el1_ctx->elr;
    uint64_t usp_value = el1_ctx->usp;
    uint64_t spsr_value = el1_ctx->spsr;

    tiny_printf(INFO, "usp: %x, elr: %x, spsr: %x\n", usp_value, elr_el1_value, spsr_value);

    while(1) ;
}

void handle_irq_exception(uint64_t *stack_pointer)
{
    trap_frame_t *el1_ctx = (trap_frame_t *)stack_pointer;

    // uint64_t x1_value = el1_ctx->r[1];
    // uint64_t sp_el0_value = el1_ctx->usp;

    int iar = gic_read_iar();
    int vector = gic_iar_irqnr(iar);

    g_handler_vec[vector]((uint64_t *)el1_ctx); // arg not use

    gic_write_eoir(iar);
    gic_write_dir(iar);
}

void invalid_exception(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{
    // trap_frame_t *el1_ctx = (trap_frame_t *)stack_pointer;

    // uint64_t x2_value = el1_ctx->r[2];
    while (1)
        ;
}


void cntp_handler(uint64_t *)
{
    asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(50000000));
    tiny_printf(INFO, "irq %d\n", TIMER);
}

int interrupt_init(void)
{
    unsigned long value, cur_el;
     /*如果当前处于EL2级别，需要设置HCR_EL2寄存器，将IRQ路由到EL2*/
    asm volatile("mrs %0, CurrentEL" : "=r" (cur_el));
    if(cur_el == 0x8) {
	    asm volatile("mrs %0, HCR_EL2" : "=r" (value));
        value |= (1<<4);
	    asm volatile("msr HCR_EL2, %0" : : "r" (value));   
    }
    
	return 0;
}

void handle_init()
{
    // interrupt_init();
    irq_handle_register(TIMER, cntp_handler);
}