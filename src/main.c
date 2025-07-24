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
    // Test all log levels to demonstrate LOG control
    tiny_error("This is an ERROR message - always shown unless LOG=none\n");
    tiny_warn("This is a WARN message - shown when LOG=warn,info,debug,all\n");
    tiny_info("Hello, ARM Tiny VM [%s]! - shown when LOG=info,debug,all\n", VM_VERSION);
    tiny_debug("This is a DEBUG message - only shown when LOG=debug,all\n");
    tiny_log("This is a generic LOG message\n");

    tiny_info("LOG control system test completed!\n");
    system_shutdown();
    return 0;
}
