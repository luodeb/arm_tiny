/* link.ld */
ENTRY(_start)

SECTIONS
{
    /* 设置内核加载地址  */
    /* . = 0x70200000; */
    . = 0x40200000;  

    .text : {
        *(.text.startup)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}
