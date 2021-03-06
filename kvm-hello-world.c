#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>
#include "kvm-header.h"

/* CR0 bits */
#define CR0_PE 1u
#define CR0_MP (1U << 1)
#define CR0_EM (1U << 2)
#define CR0_TS (1U << 3)
#define CR0_ET (1U << 4)
#define CR0_NE (1U << 5)
#define CR0_WP (1U << 16)
#define CR0_AM (1U << 18)
#define CR0_NW (1U << 29)
#define CR0_CD (1U << 30)
#define CR0_PG (1U << 31)

/* CR4 bits */
#define CR4_VME 1
#define CR4_PVI (1U << 1)
#define CR4_TSD (1U << 2)
#define CR4_DE (1U << 3)
#define CR4_PSE (1U << 4)
#define CR4_PAE (1U << 5)
#define CR4_MCE (1U << 6)
#define CR4_PGE (1U << 7)
#define CR4_PCE (1U << 8)
#define CR4_OSFXSR (1U << 8)
#define CR4_OSXMMEXCPT (1U << 10)
#define CR4_UMIP (1U << 11)
#define CR4_VMXE (1U << 13)
#define CR4_SMXE (1U << 14)
#define CR4_FSGSBASE (1U << 16)
#define CR4_PCIDE (1U << 17)
#define CR4_OSXSAVE (1U << 18)
#define CR4_SMEP (1U << 20)
#define CR4_SMAP (1U << 21)

#define EFER_SCE 1
#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)
#define EFER_NXE (1U << 11)

/* 32-bit page directory entry bits */
#define PDE32_PRESENT 1
#define PDE32_RW (1U << 1)
#define PDE32_USER (1U << 2)
#define PDE32_PS (1U << 7)

/* 64-bit page * entry bits */
#define PDE64_PRESENT 1
#define PDE64_RW (1U << 1)
#define PDE64_USER (1U << 2)
#define PDE64_ACCESSED (1U << 5)
#define PDE64_DIRTY (1U << 6)
#define PDE64_PS (1U << 7)
#define PDE64_G (1U << 8)


struct vm {
	int sys_fd;
	int fd;
	char *mem;
};

void vm_init(struct vm *vm, size_t mem_size)
{
	int api_ver;
	struct kvm_userspace_memory_region memreg; // it is virtual memory region of host which will be used by guest as RAM(Physical memory of guest).

	vm->sys_fd = open("/dev/kvm", O_RDWR); // kvm is file data will be read/written here. kvm is system for guest program.
	if (vm->sys_fd < 0) {
		perror("open /dev/kvm");
		exit(1);
	}

	api_ver = ioctl(vm->sys_fd, KVM_GET_API_VERSION, 0); // reading the API version of KVM.
	if (api_ver < 0) {
		perror("KVM_GET_API_VERSION");
		exit(1);
	}

	

	if (api_ver != KVM_API_VERSION) {
		fprintf(stderr, "Got KVM api version %d, expected %d\n",
			api_ver, KVM_API_VERSION);
		exit(1);
	}

	// comment this
	// printf("Printing KVM API version: %d, %d\n", api_ver, KVM_API_VERSION);

	vm->fd = ioctl(vm->sys_fd, KVM_CREATE_VM, 0); // VM is created and fd is returned.
	if (vm->fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

        if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
                perror("KVM_SET_TSS_ADDR");
		exit(1);
	}

	// creating the memory(RAM) for guest. mmap returns the virtual address of calling process. RAM of guest will be inside virtual memory of this process. 
	// This range of memory(0, 2<<20) is like physical address of guest so we need to setup instruction pointer, stack pointer to these addresses because IP,SP points to physical address and it is physical address for guest.
	// (It is not actual PA it will convert to VA of hypervisor then PA of hypervisor which is actual address of RAM).
	// NULL is the hint which is minimum virtual address to allocate if memory mapping already exists then kernel will allocate anywhere after this hint. since NULL is used it will allocate at any virtual address.
	// rest of parameters are for protection of allocated memory etc.
	vm->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);	
	if (vm->mem == MAP_FAILED) {
		perror("mmap mem");
		exit(1);
	}
	printf("Guest memory(RAM) allocated: %ld MB, at host virtual address from: %p,  to: %p\n", mem_size/(1024*1024), vm->mem, vm->mem+mem_size); // mmap do continuous allocation hence you can add to get last virtual address.

	// kernel should be configured with CONFIG_KSM to use madvice otherwise error is thrown at this line.
	madvise(vm->mem, mem_size, MADV_MERGEABLE);// telling kernel that pages in this range of memory are mergeable means if any page in this memory range has same content as any (same/other processes mergeable) page then merge the pages means leave only one copy of page and if any process want to modify then create the separate copy so that it will unmerged.
	// any two pages will be merged only if both are marked as mergeable.

	memreg.slot = 0;
	memreg.flags = 0;
	memreg.guest_phys_addr = 0; // physical address starts from 0 (guest should think that his RAM starts from address 0 you can set it to other value if you want) so in guest's page table physical address will be used from 0.
	memreg.memory_size = mem_size; // setting the memory size.
	memreg.userspace_addr = (unsigned long)vm->mem; // this is used by host only guest don't know about it. it is actually virtual address of host where guest is allocated.
        if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
                exit(1);
	}
}

struct vcpu {
	int fd;
	struct kvm_run *kvm_run;
};

void vcpu_init(struct vm *vm, struct vcpu *vcpu)
{
	int vcpu_mmap_size;

	vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0); // 0 here is VCPU index number, because we can create multiple VCPUs.
        if (vcpu->fd < 0) {
		perror("KVM_CREATE_VCPU");
                exit(1);
	}

	vcpu_mmap_size = ioctl(vm->sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0); // getting the size of vcpu, vcpu size includes registers size etc. this memory is shared with KVM. (I think when context switch will happen then vcpu use this space to store the registers)
        if (vcpu_mmap_size <= 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
                exit(1);
	}

	vcpu->kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE,
			     MAP_SHARED, vcpu->fd, 0);	// it map the memory in the vcpu->fd file with offset 0. means it will map from the beginning of file. This memory is used to communicate between vcpu & kvm.
	if (vcpu->kvm_run == MAP_FAILED) {
		perror("mmap kvm_run");
		exit(1);
	}
	// comment this
	printf("VCPU size allocated: %d KB, at virtual address of hypervisor(host): %p\n", vcpu_mmap_size/1024, vcpu->kvm_run);
}




/////////////////////////////////////////////  My CODE ////////////////////////////////////////////////////////////////////////////////////////
extern int errno;
int file_table_len;
size_t vm_size = 0x200000;

struct open_file_entry {
	int guest_fd;
	int fd;
	char pathname[MAX_PATHNAME];
	struct open_file_entry *next;
} *file;

struct open_file_entry* new_file_entry() {
	struct open_file_entry *ptr = malloc(sizeof(struct open_file_entry));
	ptr->guest_fd = file_table_len;
	ptr->fd = -1;
	ptr->next = NULL;
	file_table_len += 1;
	return ptr;
}

struct open_file_entry* make_entry() { // return the lowest unused fd.
	struct open_file_entry *ptr = file;
	if(ptr->fd == -1) return ptr;
	while(ptr->next != NULL) {
		if(ptr->next->fd == -1) return ptr->next;
		ptr = ptr->next;
	}
	ptr->next = new_file_entry();
	return ptr->next;
}

int is_valid_fd(int guest_fd) { // validate the fd from open file table.
	struct open_file_entry *ptr = file;
	do {
		if(ptr->guest_fd == guest_fd && ptr->fd != -1) return TRUE;
	} while(ptr->next != NULL && guest_fd < ptr->guest_fd);
	return FALSE;
}

struct open_file_entry* get_entry(int guest_fd) {
	struct open_file_entry *ptr = file;
	do {
		if(ptr->guest_fd == guest_fd && ptr->fd != -1) return ptr;
	} while(ptr->next != NULL && guest_fd < ptr->guest_fd);
	return NULL;
}

void fs_init() {
	file_table_len = 0;
	file = new_file_entry();
}

int validate_guest_addr(void *vm_mem, void *ptr, int offset) {
	char *p = (char *)ptr;
	if(p < (char *)vm_mem || p + offset > (char *)vm_mem + vm_size) {
		return FALSE;
	}
	return TRUE;
}

void print_entry(struct open_file_entry *eptr) {
	printf("Guest FD:%d,	Host FD:%d,	Pathname:%s\n", eptr->guest_fd, eptr->fd, eptr->pathname);
}

void print_file_table() {
	printf("\n******************** Open File Table ***********************\n");
	struct open_file_entry *ptr = file;
	while(ptr != NULL) {
		if(ptr->fd != -1) print_entry(ptr);
		ptr = ptr->next;
	}
	printf("************************************************************\n");
}

int get_open_flags(int gflags) {
	int flags = 0;
	if(gflags & OPN_RDONLY) flags |= O_RDONLY;
	if(gflags & OPN_WRONLY) flags |= O_WRONLY;
	if(gflags & OPN_RDWR) 	flags |= O_RDWR;
	if(flags == 0) return -1;

	if(gflags & OPN_CREAT) 	flags |= O_CREAT;
	if(gflags & OPN_TRUNC) 	flags |= O_TRUNC;
	if(gflags & OPN_APPEND) flags |= O_APPEND;
	return flags;
}

int get_open_mode(int gmode) {
	if(gmode & M_IRWXU)	return S_IRWXU;
	if(gmode & M_IRWXU)	return S_IRUSR;
	if(gmode & M_IRWXU)	return S_IWUSR;
	if(gmode & M_IRWXU)	return S_IXUSR;
	return -1;
}

int get_lseek_whence(int gflag) {
	if(gflag & LSEEK_SET) return SEEK_SET;
	if(gflag & LSEEK_CUR) return SEEK_CUR;
	if(gflag & LSEEK_END) return SEEK_END;
	return -1;
}

int run_vm(struct vm *vm, struct vcpu *vcpu, size_t sz) {
	struct kvm_regs regs;
	uint64_t memval = 0;
	uint32_t numExits = 0;
	fs_init(); // initializing my file system.
	for (;;) { // infinite loop of runnig guest. since OS runs forever

		if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) { // Hypervisor transfers control to guest
			perror("KVM_RUN");
			exit(1);
		}
		// control got back from guest to hypervisor.
		switch (vcpu->kvm_run->exit_reason) { // this is why we allocated memory for vcpu so that it can write exit reason and communicate with KVM.
		case KVM_EXIT_HLT:
			goto check;

		case KVM_EXIT_IO:
			numExits += 1;
			if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT
			    && vcpu->kvm_run->io.port == 0xE9) {	// this is 8 bits port number. see in guest.c data is written to this port number.
				char *p = (char *)vcpu->kvm_run;
				fwrite(p + vcpu->kvm_run->io.data_offset,	// data_offset is relative to kvm_run address. It kvm_run+data_offset is address of where data is stored.
				       vcpu->kvm_run->io.size, 1, stdout);	//io.size = 1. so 1*1 = 1 byte will be written fwrite(*ptr, size of one block to write, number of block, file stream). kvm->io.size is the size of data written(word size).
				fflush(stdout);	// character by character data is written and for each character KVM_EXIT_IO happens.
				continue;
			}
			
			if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT) {
				if (vcpu->kvm_run->io.port == STDOUT) {
					char *p = (char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset;
					uint32_t *ptr = (uint32_t *)p;
					p = (char *)vm->mem + *ptr;
					printf("%s", p);
					fflush(stdout);
					// printf("VAL_32_PORT data offset : %lld,  io.size: %d\n", vcpu->kvm_run->io.data_offset, vcpu->kvm_run->io.size);
					continue;
				}
				if (vcpu->kvm_run->io.port == OUT_PORT) {
					char *p = (char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset;
					uint32_t *ptr = (uint32_t *)p;
					printf("%u\n", *ptr);
					fflush(stdout);
					// printf("VAL_32_PORT data offset : %lld,  io.size: %d\n", vcpu->kvm_run->io.data_offset, vcpu->kvm_run->io.size);
					continue;
				}
				if (vcpu->kvm_run->io.port == FS_PORT) {
					uint32_t *ptr =(uint32_t *)((char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset);
					uint32_t guest_mem_addr = *ptr; // guest_mem_addr is offset of struct from guest memory.

					struct file_handler *fh_ptr = (struct file_handler *) ((char *)vm->mem + guest_mem_addr);
					if(validate_guest_addr(vm->mem, fh_ptr, sizeof(struct file_handler)) == FALSE) {// should not be more than allocated memory for guest.
						printf("Host: Invalid File Handler Memory Location\n");
						continue;
					}

					if(fh_ptr->op == FS_OPEN) {
						struct open_file *opn_ptr = (struct open_file *) ((char *)vm->mem + (uintptr_t)fh_ptr->op_struct);// fh_ptr->op_struct is logical address of guest means offset from vm->mem.
						if(validate_guest_addr(vm->mem, opn_ptr, sizeof(struct open_file)) == FALSE) {
							printf("Host: Invalid Open Struct Memory Location\n");
							continue;
						}
						char *pathname = (char *)vm->mem + (uintptr_t)opn_ptr->pathname;
						if(validate_guest_addr(vm->mem, pathname, strlen(pathname)) == FALSE) {
							printf("Host: Invalid Pathname Memory Location\n");
							opn_ptr->fd = -1;
							continue;
						}
						int fd, flags, mode;
						flags = get_open_flags(opn_ptr->flags);
						mode = get_open_mode(opn_ptr->mode);
						if(flags != -1 && opn_ptr->mode == -1) 
							fd = open(pathname, flags);
						else if(flags != -1 && mode != -1){
							fd = open(pathname, flags, mode);
						} else {
							opn_ptr->fd = -1;
							printf("Host: INVALID flags or mode\n");
							continue;
						}
						if(fd < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							opn_ptr->fd = -1;
							printf("Host: Stderror\n");
							continue;
						}

						struct open_file_entry *eptr = make_entry();
						eptr->fd = fd;
						strcpy(eptr->pathname, pathname);
						opn_ptr->fd = eptr->guest_fd;
						printf("\nHost: opening file with pathname:%s", eptr->pathname);
						print_file_table();
						continue;
					}
					if(fh_ptr->op == FS_READ) {
						struct read_file *rd_ptr = (struct read_file *) ((char *)vm->mem + (uintptr_t)fh_ptr->op_struct);
						if(validate_guest_addr(vm->mem, rd_ptr, sizeof(struct read_file)) == FALSE) {
							printf("Host: Invalid Read Struct Memory Location\n");
							continue;
						}
						struct open_file_entry *eptr = get_entry(rd_ptr->fd);
						if(eptr == NULL) {
							printf("Host: File is not open\n");
							rd_ptr->ssize = -1;
							continue;
						}
						char *buf = (char *)vm->mem + (uintptr_t)rd_ptr->buf;
						if(validate_guest_addr(vm->mem, buf, rd_ptr->size) == FALSE) { // entire buffer should be in guest memory no overflow.
							printf("Host: Invalid Read Buffer Memory Location\n");
							rd_ptr->ssize = -1;
							continue;
						}

						rd_ptr->ssize = read(eptr->fd, buf, rd_ptr->size);
						continue;
					}
					if(fh_ptr->op == FS_WRITE) {
						struct write_file *wr_ptr = (struct write_file *) ((char *)vm->mem + (uintptr_t)fh_ptr->op_struct);
						if(validate_guest_addr(vm->mem, wr_ptr, sizeof(struct write_file)) == FALSE) {
							printf("Host: Invalid Write Struct Memory Location\n");
							continue;
						}
						struct open_file_entry *eptr = get_entry(wr_ptr->fd);
						if(eptr == NULL) {
							printf("Host: File is not open\n");
							wr_ptr->ssize = -1;
							continue;
						}
						char *buf = (char *)vm->mem + (uintptr_t)wr_ptr->buf;
						if(validate_guest_addr(vm->mem, buf, wr_ptr->count) == FALSE) { // entire buffer should be in guest memory no overflow.
							printf("Host: Invalid Write Buffer Memory Location\n");
							wr_ptr->ssize = -1;
							continue;
						}
						// printf("count:%ld, buf is:%s\n", wr_ptr->count, buf);
						// char buff[MAX_DATA];
						// strcpy(buff, buf);
						// printf("%s\n", buff);
						if(strlen(buf) < wr_ptr->count) wr_ptr->count = strlen(buf);
						wr_ptr->ssize = write(eptr->fd, buf, wr_ptr->count); // if binary data is written in sublime try opening in default text editor.
						printf("Host: write ssize:%d\n", wr_ptr->ssize);
						continue;
					}
					if(fh_ptr->op == FS_CLOSE) {
						struct open_file_entry *eptr = get_entry(fh_ptr->fd);
						if(eptr == NULL) {
							printf("Host: File is not open\n");
							fh_ptr->flag = -1;
							continue;
						}
						fh_ptr->flag = close(eptr->fd);
						if(fh_ptr->flag == 0) eptr->fd = -1;

						printf("\nHost: closing file with pathname:%s", eptr->pathname);
						print_file_table();
						continue;
					}
					if(fh_ptr->op == FS_LSEEK) {
						struct lseek_file *lsk_ptr = (struct lseek_file *) ((char *)vm->mem + (uintptr_t)fh_ptr->op_struct);
						if(validate_guest_addr(vm->mem, lsk_ptr, sizeof(struct lseek_file)) == FALSE) {
							printf("Host: Invalid Lseek Struct Memory Location\n");
							continue;
						}
						struct open_file_entry *eptr = get_entry(lsk_ptr->fd);
						if(eptr == NULL) {
							printf("Host: File is not open\n");
							lsk_ptr->foffset = -1;
							continue;
						}
						int whence = get_lseek_whence(lsk_ptr->whence);
						lsk_ptr->foffset = lseek(eptr->fd, lsk_ptr->offset, whence);
						printf("Host: lseek foffset:%d\n", lsk_ptr->foffset);
						continue;
					}
					if(fh_ptr->op == FS_ISOPEN) {
						if(is_valid_fd(fh_ptr->fd) == TRUE) fh_ptr->flag = 1;
						else fh_ptr->flag = 0;
						continue;
					}

					printf("Host: INVALID FILE OPERATION\n");
				}
			}
			if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_IN) {
				if (vcpu->kvm_run->io.port == IN_PORT) {
					// we don't need io.size it is defined by assembly instruction in guest.c see there.
					char *p = (char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset;
					uint32_t *ptr = (uint32_t *)p;
					*ptr = numExits;
					continue;
				}
			}
			printf("Host: INVALID IO OPERATION\n");
			/* fall through */
		default:
			fprintf(stderr,	"Got exit_reason %d,"
				" expected KVM_EXIT_HLT (%d)\n",
				vcpu->kvm_run->exit_reason, KVM_EXIT_HLT);
			exit(1);
		}
	}

 check:
	if (ioctl(vcpu->fd, KVM_GET_REGS, &regs) < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	if (regs.rax != 42) {	// hlt instruction in guest.c set 42 so it goes into EAX register.
		printf("Wrong result: {E,R,}AX is %lld\n", regs.rax);
		return 0;
	}

	memcpy(&memval, &vm->mem[0x400], sz);	// vm->mem[0x400] = value(vm->mem + 0x400) physical address of guest. & references. actually we have written 42 at virtual address of guest so reading it using physical address of guest memory because guest VA = PA>
	if (memval != 42) {										// 42 value is set at 0x400 location in guest.c to verify that it reached halt statement or not.
		printf("Wrong result: memory at 0x400 is %lld\n",
		       (unsigned long long)memval);
		return 0;
	}

	return 1;
}

extern const unsigned char guest16[], guest16_end[];

int run_real_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	printf("Testing real mode\n");

        if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	//ME: setting the base of the code segment to 0. (it can get any instruction by adding offset(rip (instruction pointer register) is the offset from code segment register)).
	sregs.cs.selector = 0;
	sregs.cs.base = 0;

        if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest16, guest16_end-guest16);
	return run_vm(vm, vcpu, 2);
}

static void setup_protected_mode(struct kvm_sregs *sregs)
{
	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,
		.selector = 1 << 3,
		.present = 1,
		.type = 11, /* Code: execute, read, accessed */
		.dpl = 0,
		.db = 1,
		.s = 1, /* Code/data */
		.l = 0,
		.g = 1, /* 4KB granularity */
	};

	sregs->cr0 |= CR0_PE; /* enter protected mode */

	sregs->cs = seg;

	seg.type = 3; /* Data: read/write, accessed */
	seg.selector = 2 << 3;
	sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

extern const unsigned char guest32[], guest32_end[];

int run_protected_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	printf("Testing protected mode\n");

        if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_protected_mode(&sregs);

        if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest32, guest32_end-guest32);
	return run_vm(vm, vcpu, 4);
}

static void setup_paged_32bit_mode(struct vm *vm, struct kvm_sregs *sregs)
{
	uint32_t pd_addr = 0x2000;
	uint32_t *pd = (void *)(vm->mem + pd_addr);

	/* A single 4MB page to cover the memory region */
	pd[0] = PDE32_PRESENT | PDE32_RW | PDE32_USER | PDE32_PS;
	/* Other PDEs are left zeroed, meaning not present. */

	sregs->cr3 = pd_addr;
	sregs->cr4 = CR4_PSE;
	sregs->cr0
		= CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
	sregs->efer = 0;
}

int run_paged_32bit_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	printf("Testing 32-bit paging\n");

        if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_protected_mode(&sregs);
	setup_paged_32bit_mode(vm, &sregs);

        if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest32, guest32_end-guest32);
	return run_vm(vm, vcpu, 4);
}

extern const unsigned char guest64[], guest64_end[]; // guest64 is the beginning of the guest program code, and guest64_end is last address  we need to load this code to code segment of guest machine.

static void setup_64bit_code_segment(struct kvm_sregs *sregs)
{
	// this common value of segment registers like CS, DS(data segment).
	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,	// this is limit of this segment. should not exceed this in case of stack segment data segment etc.
		.selector = 1 << 3,
		.present = 1,
		.type = 11, /* Code: execute, read, accessed */	// it is for Code segment.
		.dpl = 0,	// privilege level 0(ring 0) DPT(descriptor privilege level) this bit in each segment represent the privilege level. If it is CS(code segment of user process) then it will be 3(ring3) and if this is CS (code segment of kernel) then 0(ring0).
		.db = 0,
		.s = 1, /* Code/data */
		.l = 1,
		.g = 1, /* 4KB granularity */
	};

	// setting the code segment register value.
	sregs->cs = seg;	//code segment register see above(.type) those flags.

	// modifying the few values of seg to use it for DS(data segment) registers value.
	seg.type = 3; /* Data: read/write, accessed */ // it is for data segment.
	seg.selector = 2 << 3;
	// actually in our simple example we are not using so many segments so only main segments like CS,DS is used(guest memory = CS+DS+stack stack is allocated somewhere else)so rest of the segments have been mapped to same address because we are not going to use them.
	// setting same value for different registers.
	sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

static void setup_long_mode(struct vm *vm, struct kvm_sregs *sregs)
{	
	// allocating virtual addresses to page tables of each level NOTE: virtual address of page table is fixed when process is loaded. physical address is changing because of swapping.
	// setting the base address for 4rth level page table. but we are using only 3 level but usually we use 4 levels.
	uint64_t pml4_addr = 0x2000;	//base address of pml4 table. offset from guest memory starting address. 0x2000 in hexadecimal = 0010,0000,0000,0000 = 2^13.
	uint64_t *pml4 = (void *)(vm->mem + pml4_addr); // absolute pointer to pml4 table.

	uint64_t pdpt_addr = 0x3000; // base address of pdpt_addr table
	uint64_t *pdpt = (void *)(vm->mem + pdpt_addr);// absolute pointer to pdpt table.

	uint64_t pd_addr = 0x4000; // base address of pd_addr table.
	uint64_t *pd = (void *)(vm->mem + pd_addr); // absolute pointer to pd_addr table.

	// we know that there is only one page in process hence all the level of page tables will have single PTE so only 1st PTE is set for each level.
	pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr; //pml4[0] is the first PTE of pml4 table it has some flag bits and pdpt_addr(guest memory address of pdpt_addr table).
	pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;   // pdpt[0] is the 1st PTE of pdpt table pd_addr is stored in PTE which is address of next page table. 
	pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;	// pd[0] is 1st PTE of pd table since guest memory is initialized with all zeros so no need to set the address of 1st(and only) page of process because guest physical address begin with all zeros.
	//PDE64_PS is page size bit which indicates page pointed by this PTE is 2M not 4k. 

	sregs->cr3 = pml4_addr;	// CR3 register is used to store the base address of highest level page table and we need to set it. because we can allocate pml4 table anywhere in guest memory.
	sregs->cr4 = CR4_PAE;	// CR4_PAE is 5th bit(1<<5). by setting it page size is treated as 2MB instead of 4KB(default). it is Physical Address Extension means it change page table layout to translate 32 bit virtual address to 36 bit physical address.
	sregs->cr0
		= CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
	sregs->efer = EFER_LME | EFER_LMA;

	setup_64bit_code_segment(sregs);
}

int run_long_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs; // special registers these will be store in vcpu memory.
	struct kvm_regs regs;	// IP register, SP register, flags etc. are stored in this.

	printf("Testing 64-bit mode\n");

        if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_long_mode(vm, &sregs);

        if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2; // 2 = 0..0010  only one bit is set. In x86 the 0x2 bit is always set. find out which is this bit what does it represent.
	regs.rip = 0;	// rip = register IP(instruction pointer) = 0. It points to physical address of guest, execute the code from beginning(we will load code segment in the beginning see memcpy() below). actually this points to beginning of the memory we allocated to guest.

	/* Create stack at top of 2 MB page and grow down. */
	// set the stack(kernel stack) pointer at 2<<20 address(physical address). we used 2MB RAM for guest(see in main() method) so 2<<20 = 2 * 2^20 = 2MB is actually end of guest memory so kernel stack is allocated in end and it will grow by decrementing guest virtual address range(0-2<<20).
	regs.rsp = 2 << 20;		// stack pointer(points to physical address of guest) will point to 1<<21 location and decrement from there. this address is for memory we allocated to guest.

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	// vm->mem is virtual address of hypervisor(host) which is beginning of guest memory we are copying the code(to be executed by guest) in this address(beginning of memory) from guest64 (guest64 is the location of compiled asembly code of guest program to be executed).
	memcpy(vm->mem, guest64, guest64_end-guest64); 
	// we allocated code segment at the beginning of guest memory. and set the rip (IP register) to point it.
	printf("code segment loaded at host VA from:%p,   to %p, size: %ld Bytes\n", vm->mem, vm->mem+(guest64_end-guest64), guest64_end-guest64); // hypervisors virtual address.
	printf("code segment loaded at guest PA from: %lld,  to %lld\n", sregs.cs.base, sregs.cs.base+(guest64_end-guest64)); // guest physical address. using cs.base here does not make sense because we are storing code at vm->mem but we have also made vm->mem as physical address 0. and set cs.base = 0. 
	return run_vm(vm, vcpu, 8);
}


int main(int argc, char **argv)
{
	struct vm vm;
	struct vcpu vcpu;
	enum {
		REAL_MODE,
		PROTECTED_MODE,
		PAGED_32BIT_MODE,
		LONG_MODE,
	} mode = REAL_MODE;
	int opt;

	// check the execution mode optional parameters in command line.
	while ((opt = getopt(argc, argv, "rspl")) != -1) { // ./kvm-hello-world -l   so it will check whether it is -l (64 bit mode) you can do by splitting on your own.
		switch (opt) {
		case 'r':
			mode = REAL_MODE;
			break;

		case 's':
			mode = PROTECTED_MODE;
			break;

		case 'p':
			mode = PAGED_32BIT_MODE;
			break;

		case 'l':
			mode = LONG_MODE;
			break;

		default:
			fprintf(stderr, "Usage: %s [ -r | -s | -p | -l ]\n",
				argv[0]);
			return 1;
		}
	}

	vm_init(&vm, 0x200000); // 0x200000 = 2 << 20 = 2MB this is the memory size (RAM) allocated for the virtual machine.(guest program) we are not running guest OS but we are running guest program because handling OS is big thing so just handle the simple guest program first.
	vcpu_init(&vm, &vcpu);

	switch (mode) {
	case REAL_MODE:
		return !run_real_mode(&vm, &vcpu);

	case PROTECTED_MODE:
		return !run_protected_mode(&vm, &vcpu);

	case PAGED_32BIT_MODE:
		return !run_paged_32bit_mode(&vm, &vcpu);

	case LONG_MODE:
		return !run_long_mode(&vm, &vcpu);
	}

	return 1;
}
