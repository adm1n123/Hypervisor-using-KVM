#include <stddef.h>
#include <stdint.h>
#include "filesystem.h"

#define STDOUT 0x0001
#define UINT32_OUT_PORT 0x3201
#define UINT32_IN_PORT 0x3200


static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}
static inline void outb_32(uint16_t port, uint32_t value) {
  asm("out %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}
static inline uint32_t inb(uint16_t port) {
	uint32_t ret;
	asm("in %1, %0" : "=a"(ret) : "Nd"(port) : "memory" );
	return ret;
}

static void display(char *p) {
		outb_32(STDOUT, (uintptr_t)p); // NOTE: uintptr_t is 64 bit and our vcpu is also 64 bit but we are using 32bit IO. try to find out 64bit assembly code for this. it is working because virtual address range is very small hence even truncating 64bit to 32 bit doesn't change the address. and in hypervisor we are using this virtual address as offset.
}
static void printVal(uint32_t val) {
	outb_32(UINT32_OUT_PORT, val);
}
static uint32_t getNumExits() {
	uint32_t exits = inb(UINT32_IN_PORT);
	return exits;
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	
	display("|-----------Starting the execution ----------|\n");
	const char *p;
	
	uint32_t val;

	for (p = "Hello, world!\n"; *p; ++p)
		outb(0xE9, *p); // for each character (pointer address) KVM Exit for IO. and one char is passed at a time. here total 14 exits required including \n after \n there is \0 (NULL) then loop terminates.
	
	char *ptr = "admin testing code\n";
	display(ptr);

	val = sizeof(char *);
	// getting 32 bit value
	display("Writing 32 bit value from guest\n");
	printVal(val);
	uint32_t numExits = getNumExits();
	display("printing exit count\n");
	printVal(numExits);
	display("\n");







	display("|-----------Execution finished ----------|\n");
	*(long *) 0x400 = 42; // storing 42 at 0x400 pointer address. NOTE: for guest program it is his virtual address. pointer address is always virtual address.

	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");
}
