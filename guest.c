#include <stddef.h>
#include <stdint.h>
#include "guest-header.h"


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

int valid_size(char *p) {
	for(int i = 0; i < MAX_PATHNAME; i++) {
		if(p[i] == '\0') return TRUE;
	}
	return FALSE;
}

////////////////////////////////////////////////////////////////////// File System ////////////////////////
int open(char *pathname, int flags);
int open2(char *pathname, int flags, int mode);
int creat(char *pathname, int mode);
int read(int fd, char *buf, size_t size);
int write(int fd, char *buf, size_t count);
int close(int fd);
int lseek(int fd, int offset, int whence);
int get_cursor(int fd);
int is_open(int fd);



int open(char *pathname, int flags) {
	if(valid_size(pathname) == FALSE) {
		display("Guest: Invalid pathname max limit 1000\n");
		return -1;
	}

	opn.flags = flags;
	opn.pathname = pathname;
	opn.mode = -1;

	fh.op = FS_OPEN;
	fh.op_struct = &opn;
	out(FS_PORT, (uintptr_t)&fh);
	return opn.fd;
}

int open2(char *pathname, int flags, int mode) {
	if(valid_size(pathname) == FALSE) {
		display("Guest: Invalid pathname max limit 1000\n");
		return -1;
	}
	
	opn.pathname = pathname;
	opn.flags = flags;
	opn.mode = mode;

	fh.op = FS_OPEN;
	fh.op_struct = &opn;
	out(FS_PORT, (uintptr_t)&fh);
	return opn.fd;
}

int creat(char *pathname, int mode) {
	return open2(pathname, OPN_CREAT|OPN_WRONLY|OPN_TRUNC, mode);
}

int read(int fd, char *buf, size_t size) {
	if(size + 1 > MAX_DATA) { // because last char null will be extra and also required.
		display("Guest: Max Reading Limit is:");
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

int lseek(int fd, int offset, int whence) {
	lsk.fd = fd;
	lsk.offset = offset;
	lsk.whence = whence;

	fh.op = FS_LSEEK;
	fh.op_struct = &lsk;
	out(FS_PORT, (uintptr_t)&fh);
	return lsk.foffset;
}

int get_cursor(int fd) {
	return lseek(fd, 0, LSEEK_CUR);
}

int is_open(int fd) {
	fh.op = FS_ISOPEN;
	fh.fd = fd;

	out(FS_PORT, (uintptr_t)&fh);
	return fh.flag;
}

int rename();
int copy();
int remove();
int dup();
int dup2();
int access(); // https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c
void errorno(); // use errorno for printing the file error use extern errorno variable and print the error. in host. -1 is not sufficient to catch error.
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
	display("GUEST: Writing 32 bit value:");
	printVal(val);
	uint32_t numExits = getNumExits();
	display("GUEST: printing exit count:");
	printVal(numExits);
	display("\n");

	display("|-----------Leaving Part B ----------|\n");
}

void test_read() {
	int fd = open("test-files/myfile.txt", OPN_RDWR|OPN_APPEND);
	if(fd < 0) {
		display("GUEST: Error opening file\n");
		return;
	}
	display("GUEST: open file fd:");
	printVal(fd);

	char *buf = data;
	size_t size = 100;
	int ssize = read(fd, buf, size);
	if(ssize < 0) {
		display("GUEST: Error reading the file\n");
		return;
	}
	display("GUEST: printing the read data: ");
	display(buf);
	display("\n");

	int foffset = lseek(fd, 2, LSEEK_SET);
	if(foffset < 0) {
		display("GUEST: Error while seeking\n");
	}

	ssize = read(fd, buf, size);
	if(ssize < 0) {
		display("GUEST: Error reading the file\n");
		return;
	}
	display("GUEST: printing the read data: ");
	display(buf);
	display("\n");

	if(close(fd) != 0) {
		display("GUEST: Error while closing file\n");
	}
}

void test_write() {
	int fd = open("test-files/w_myfile.txt", OPN_RDWR);
	if(fd < 0) {
		display("GUEST: Error opening file\n");
		return;
	}
	char *buf = "Hi I am deepak i am working on virtualization assignment";
	int ssize = write(fd, buf, 40);
	if(ssize < 0) {
		display("GUEST: Error writing on file\n");
		return;
	}

	int foffset = lseek(fd, 2, LSEEK_SET); // if file is open in append mode then seek will not work.
	if(foffset < 0) {
		display("GUEST: Error while seeking\n");
	}

	buf = "SEEK DATA SEEK DATA SEEK DATA SEEK";
	ssize = write(fd, buf, 30);
	if(ssize < 0) {
		display("GUEST: Error writing on file\n");
		return;
	}

	if(close(fd) != 0) {
		display("GUEST: Error while closing file\n");
	}

}

void part_C() {
	display("|-----------Inside Part C ----------|\n");
	
	test_read();
	test_write();

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
