#ifndef CONFIG_H
#define CONFIG_H

// Configuration settings for the ARM Tiny project

#define CNTP_TIMER   30
#define CNTV_TIMER   27

#define GICD_BASE_ADDR  0x8000000
#define GICC_BASE_ADDR  0x8010000

#define UART_BASE_ADDR  0x09000000
#endif // CONFIG_H
