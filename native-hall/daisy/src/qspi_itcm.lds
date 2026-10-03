/* QSPI boot image for the public Daisy bootloader at 0x90040000.
 * Only the reset stub runs from QSPI; audio code and constants run in RAM.
 * Based on the BSD-licensed libDaisy linker layout. */
ENTRY(HallQspiBootReset)
MEMORY
{
    DTCMRAM (RWX) : ORIGIN = 0x20000000, LENGTH = 128K
    SRAM (RWX) : ORIGIN = 0x24000000, LENGTH = 480K
    RAM_D2_DMA (RWX) : ORIGIN = 0x30000000, LENGTH = 32K
    RAM_D2 (RWX) : ORIGIN = 0x30008000, LENGTH = 256K
    RAM_D3 (RWX) : ORIGIN = 0x38000000, LENGTH = 64K
    ITCMRAM (RWX) : ORIGIN = 0x00000000, LENGTH = 64K
    SDRAM (RWX) : ORIGIN = 0xc0000000, LENGTH = 64M
    BACKUP_SRAM (RWX) : ORIGIN = 0x38800000, LENGTH = 4K
    /* The bootloader occupies the first 256 KiB of the 8 MiB QSPI. */
    QSPIFLASH (RX) : ORIGIN = 0x90040000, LENGTH = 7936K
}
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);
SECTIONS
{
    .qspi_boot : { KEEP(*(.qspi_boot_vector)) KEEP(*(.qspi_boot_text*)) } > QSPIFLASH
    .isr_vector : ALIGN(32) {
        _hall_vector_start = .; KEEP(*(.isr_vector)) . = ALIGN(4); _hall_vector_end = .;
    } > RAM_D3 AT > QSPIFLASH
    _hall_vector_load = LOADADDR(.isr_vector);
    .itcm_text : ALIGN(4) {
        _hall_itcm_start = .;
        *hall.o(.text .text.*)
        *main.o(.text.*AudioCallback* .text.*Engine48*process* .text.*ProcessControls*)
        . = ALIGN(4); _hall_itcm_end = .;
    } > ITCMRAM AT > QSPIFLASH
    _hall_itcm_load = LOADADDR(.itcm_text);
    .text : ALIGN(4) {
        _hall_runtime_start = .; _stext = .;
        *(.text .text.* .rodata .rodata.* .glue_7 .glue_7t)
        KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .;
    } > SRAM AT > QSPIFLASH
    _hall_runtime_load = LOADADDR(.text);
    .ARM.extab : ALIGN(4) { *(.ARM.extab* .gnu.linkonce.armextab.*) } > SRAM AT > QSPIFLASH
    .exidx : ALIGN(4) {
        __exidx_start = .; *(.ARM.exidx*) __exidx_end = .;
    } > SRAM AT > QSPIFLASH
    .preinit_array : ALIGN(4) {
        __preinit_array_start = .; KEEP(*(.preinit_array*)) __preinit_array_end = .;
    } > SRAM AT > QSPIFLASH
    .init_array : ALIGN(4) {
        __init_array_start = .; KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) __init_array_end = .;
    } > SRAM AT > QSPIFLASH
    .fini_array : ALIGN(4) {
        __fini_array_start = .; KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) __fini_array_end = .;
        . = ALIGN(4); _hall_runtime_end = .;
    } > SRAM AT > QSPIFLASH
    .hall_bank_bss (NOLOAD) : ALIGN(4) {
        _hall_bank_start = .; *(.hall_bank_bss) . = ALIGN(4); _hall_bank_end = .;
    } > SRAM
    .data : ALIGN(4) {
        _sdata = .; *(.data .data.*) . = ALIGN(4); _edata = .;
    } > DTCMRAM AT > QSPIFLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) : ALIGN(4) {
        _sbss = .; *(.bss .bss.* COMMON) . = ALIGN(4); _ebss = .;
    } > DTCMRAM
    .sram1_bss (NOLOAD) : ALIGN(32) {
        _ssram1_bss = .; *(.sram1_bss*) . = ALIGN(32); _esram1_bss = .;
    } > RAM_D2_DMA
    .dtcmram_bss (NOLOAD) : ALIGN(4) { *(.dtcmram_bss*) } > DTCMRAM
    .sdram_bss (NOLOAD) : ALIGN(4) { *(.sdram_bss*) } > SDRAM
    .backup_sram (NOLOAD) : ALIGN(4) { *(.backup_sram*) } > BACKUP_SRAM
    .heap (NOLOAD) : ALIGN(8) {
        __heap_start__ = .; __heap_start = .; end = .; KEEP(*(.heap))
        __heap_end = .; __heap_end__ = .;
    } > RAM_D2
    .reserved_for_stack (NOLOAD) : ALIGN(4) { KEEP(*(.reserved_for_stack)) } > DTCMRAM
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
ASSERT(_ebss <= _estack - 16K, "Reserve at least 16 KiB of DTCM stack")
ASSERT(_hall_bank_end <= ORIGIN(SRAM) + LENGTH(SRAM), "Control bank exceeds AXI SRAM")
ASSERT(SIZEOF(.itcm_text) <= 64K, "Hall audio code exceeds ITCM")
ASSERT(LOADADDR(.fini_array) - ADDR(.fini_array) == _hall_runtime_load - _hall_runtime_start,
       "Runtime arrays must share the text copy's VMA/LMA offset")
