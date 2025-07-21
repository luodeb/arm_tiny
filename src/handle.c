#include "tiny_io.h"
#include <config.h>

void handle_sync_exception(uint64_t *stack_pointer)
{
}

void handle_irq_exception(uint64_t *stack_pointer)
{
}

void invalid_exception(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{
    tiny_log(ERROR, "Invalid exception occurred!\n");
    while (1)
        ;
}