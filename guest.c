#include <stddef.h>
#include <stdint.h>

static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	const char *p;

	for (p = "Hello, world!\n"; *p; ++p)
		outb(0xE9, *p); // for each character (pointer address) KVM Exit for IO. and one char is passed at a time. here total 14 exits required including \n after \n there is \0 (NULL) then loop terminates.

	*(long *) 0x400 = 42; // storing 42 at 0x400 pointer address. NOTE: for guest program it is his virtual address. pointer address is always virtual address.

	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");
}
