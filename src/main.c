/*
 * File: main.c
 * Author: Debin
 * Date: 2024-11-15
 * Description: Main file for the ARM Tiny project.
 */

#include "tiny_io.h"

#ifndef VM_VERSION
#define VM_VERSION "null"
#endif

void test_extended_printf();
int kernel_main(void)
{
    tiny_log(INFO, "Hello, ARM Tiny VM [%s]!\n", VM_VERSION);

    test_extended_printf();

    system_shutdown();
    return 0;
}
