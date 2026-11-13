/* sidcore: one block from address 0.  Linked with --emit-relocs; tools/blob_pack.py lists every
   absolute address in it, and the host adds its real base address to each.
   Order: entry, code, read-only data, init array, data, GOT, then bss (zeroed by the host). */
ENTRY(sidcoreEntry)
SECTIONS
{
	. = 0;
	.entry : { KEEP(*(.entry)) }
	.text : { *(.text .text.*) *(.glue_7 .glue_7t .vfp11_veneer .v4_bx) }
	.rodata : { *(.rodata .rodata.*) }
	.ARM.exidx : { *(.ARM.exidx*) }
	.init_array : {
		. = ALIGN(4);
		__init_array_start = .;
		KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array))
		__init_array_end = .;
	}
	.data : { . = ALIGN(4); *(.data .data.*) }
	.got : { *(.got.plt) *(.got) }
	. = ALIGN(4);
	__image_end = .;
	.bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) . = ALIGN(4); }
	__bss_end = .;
	/DISCARD/ : { *(.comment) *(.note*) *(.ARM.attributes) *(.interp) *(.dynsym) *(.dynstr) *(.hash) *(.gnu.hash) }
}
