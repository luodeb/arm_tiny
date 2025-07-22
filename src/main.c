/*
 * File: main.c
 * Author: Debin
 * Date: 2024-11-15
 * Description: Main file for the ARM Tiny project.
 */

#include "tinyio.h"
#include "tinystd.h"

#ifndef VM_VERSION
#define VM_VERSION "null"
#endif

int kernel_main(void)
{
    tiny_log("Hello, ARM Tiny VM [%s]!\n", VM_VERSION);

    system_shutdown();
    return 0;
}
