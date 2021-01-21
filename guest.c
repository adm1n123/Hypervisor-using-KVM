#include <stddef.h>
#include <stdint.h>


#define STDOUT_PORT 0x01
#define VAL_32_OUT_PORT 0x3201
#define VAL_32_IN_PORT 0x3200
#define VAL_64_OUT_PORT 0x6401
#define VAL_64_IN_PORT 0x6400

static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}
static inline void outb_32(uint16_t port, uint32_t value) {
  asm("out %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}
static inline void outb_64(uint16_t port, uint64_t value) {
  asm("out %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}
static inline uint32_t inb(uint16_t port) {
	uint32_t ret;
	asm("in %1, %0" : "=a"(ret) : "Nd"(port) : "memory" );
	return ret;
}

static void display(char *p) {
	for (;*p; p += 1)
		outb(STDOUT_PORT, *p);
}

static inline void display_uint(uint32_t n) {
	char ch[20], c;
	int i = 0;
	if (n == 0) {
		ch[0] = '0'; ch[1] = '\0';
	} else {
		while(n > 0) {
			ch[i++] = '0' + n%10;
			n /= 10;
		}
		int l = 0, r = i-1;
		while(l<r) {
			c = ch[l]; ch[l] = ch[r]; ch[r] = c;
			l++; r--;
		}
		ch[i] = '\0';
	}
	for (int j = 0; ch[j]; j += 1)
		outb(STDOUT_PORT, ch[j]);
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	display("|-----------Starting the execution ----------|\n");
	const char *p;
	


	for (p = "Hello, world!\n"; *p; ++p)
		outb(0xE9, *p); // for each character (pointer address) KVM Exit for IO. and one char is passed at a time. here total 14 exits required including \n after \n there is \0 (NULL) then loop terminates.
	


	// getting 32 bit value
	display("Writing 32 bit value from guest\n");
	outb_32(VAL_32_OUT_PORT, 123456789);
	display("\n");

	display("Reading the 32 bit value in guest: ");
	// uint32_t val_32 = inb(VAL_32_IN_PORT);
	
	display("\n");






	display("|-----------Execution finished ----------|\n");
	*(long *) 0x400 = 42; // storing 42 at 0x400 pointer address. NOTE: for guest program it is his virtual address. pointer address is always virtual address.

	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");
}
