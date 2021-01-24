#include <stddef.h>
#include <stdint.h>
#include "filesystem.h"
#include "kvm-guest-common.h"


static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}
static inline void out(uint16_t port, uint32_t value) {
  asm("out %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}
static inline uint32_t in(uint16_t port) {
	uint32_t ret;
	asm("in %1, %0" : "=a"(ret) : "Nd"(port) : "memory" );
	return ret;
}

void display(char *p) {
		out(STDOUT, (uintptr_t)p); // NOTE: uintptr_t is 64 bit and our vcpu is also 64 bit but we are using 32bit IO. try to find out 64bit assembly code for this. it is working because virtual address range is very small hence even truncating 64bit to 32 bit doesn't change the address. and in hypervisor we are using this virtual address as offset.
}
void printVal(uint32_t val) {
	out(OUT_PORT, val);
}
uint32_t getNumExits() {
	uint32_t exits = in(IN_PORT);
	return exits;
}

////////////////////////////////////////////////////////////////////// File System ////////////////////////


int valid_size(char *p) {
	for(int i = 0; i < MAX_PATHNAME; i++) {
		if(p[i] == '\0') return TRUE;
	}
	return FALSE;
}

int open(char *pathname, int flags) {
	if(valid_size(pathname) == FALSE) {
		display("Invalid pathname max limit 1000\n");
		return -1;
	}

	opn.flags = flags;
	opn.pathname = pathname;

	fh.op = FS_OPEN;
	fh.op_struct = &opn;
	out(FS_PORT, (uintptr_t)&fh);
	return opn.fd;
}

int read(int fd, char *buf, size_t size) {
	if(size + 1 > MAX_DATA) { // because last char null will be extra and also required.
		display("Max Reading Limit is:");
		printVal(MAX_DATA);
		display(" Bytes\n");
		return -1;
	}
	rd.fd = fd;
	rd.buf = buf;
	rd.size = size;

	fh.op = FS_READ;
	fh.op_struct = &rd;
	out(FS_PORT, (uintptr_t)&fh);
	return rd.ssize;
}

int write(int fd, char *buf, size_t count) {
	wr.fd = fd;
	wr.buf = buf;
	wr.count = count;

	fh.op = FS_WRITE;
	fh.op_struct = &wr;
	out(FS_PORT, (uintptr_t)&fh);
	return wr.ssize;
}

int close(int fd) {
	fh.op = FS_CLOSE;
	fh.fd = fd;
	out(FS_PORT, (uintptr_t)&fh);
	return fh.flag;
}

int create();
int close();
int read();
int write();
int lseek();
int rename();
int copy();
int remove();
int dup();
int dup2();
int isopen();
int get_seek_pointer_offset(); // change this name;
int access(); // https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c

////////////////////////////////////////////////////////////////////// File System ////////////////////////

void part_A() {
	const char *p;
	
	for (p = "Hello, world!\n"; *p; ++p)
		outb(0xE9, *p); // for each character (pointer address) KVM Exit for IO. and one char is passed at a time. here total 14 exits required including \n after \n there is \0 (NULL) then loop terminates.
	
}

void part_B() {
	display("|-----------Inside Part B ----------|\n");
	uint32_t val;

	char *ptr = "admin testing code\n";
	display(ptr);

	val = 1<<31;
	// getting 32 bit value
	display("Writing 32 bit value from guest:");
	printVal(val);
	uint32_t numExits = getNumExits();
	display("printing exit count:");
	printVal(numExits);
	display("\n");

	display("|-----------Leaving Part B ----------|\n");
}

void part_C() {
	display("|-----------Inside Part C ----------|\n");

	int fd = open("test-files/myfile.txt", O_RDWR|O_APPEND);
	if(fd < 0) {
		display("Error opening file\n");
		return;
	}
	display("open file guest fd:");
	printVal(fd);

	char *buf = data;
	size_t size = 100;
	int ssize = read(fd, buf, size);
	if(ssize < 0) {
		display("Error reading the file\n");
		return;
	}
	display("printing the read data: ");
	display(buf);
	display("\n");
	if(close(fd) != 0) {
		display("Error while closing file\n");
	}

	fd = open("test-files/w_myfile.txt", O_WRONLY|O_APPEND);
	if(fd < 0) {
		display("Error opening file\n");
		return;
	}
	buf = "Hi I am deepak i am working on virtualization assignment";
	size_t count = 500;
	ssize = write(fd, buf, count);
	if(ssize < 0) {
		display("Error writing on file\n");
		return;
	}

	if(close(fd) != 0) {
		display("Error while closing file\n");
	}

	display("\n|-----------Leaving Part C ----------|\n");
}





void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	
	part_A();
	part_B();
	part_C();

	*(long *) 0x400 = 42; // storing 42 at 0x400 pointer address. NOTE: for guest program it is his virtual address. pointer address is always virtual address.

	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");
}
