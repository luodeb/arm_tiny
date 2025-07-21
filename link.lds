/* link.ld */
ENTRY(_start)

SECTIONS
{
    /* 设置内核加载地址，4K 对齐 */
    /* . = 0x70200000; */
    /*. = 0x40200000;*/
    . = 0x40080000;

    /* 代码段，4K 对齐 */
    . = ALIGN(4096);
    .text : ALIGN(4096) {
        *(.text.startup)
        *(.text)
    }

    /* 只读数据段，4K 对齐 */
    . = ALIGN(4096);
    .rodata : ALIGN(4096) {
        *(.rodata)
    }

    /* 数据段，4K 对齐 */
    . = ALIGN(4096);
    .data : ALIGN(4096) {
        *(.data)
    }

    /* BSS段，4K 对齐 */
    . = ALIGN(4096);
    .bss : ALIGN(4096) {
        *(.bss)
    }

    /* 确保整个镜像结束时也是4K对齐 */
    . = ALIGN(4096);
}
