#include "tinyio.h"
#include <config.h>
#include "tiny_types.h"

void handle_sync_exception(uint64_t *stack_pointer)
{
    tiny_error("Synchronous exception occurred!\n");
    while (1)
        ;
}

void handle_irq_exception(uint64_t *stack_pointer)
{
    tiny_error("IRQ exception occurred!\n");
    while (1)
        ;
}

void invalid_exception(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{
    tiny_error("Invalid exception occurred!\n");
    while (1)
        ;
}