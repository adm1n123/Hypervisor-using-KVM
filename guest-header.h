#define MAX_PATHNAME 100
#define MAX_DATA 10000 // 10 KB
#define STDOUT 0x0001
#define OUT_PORT 0x3201
#define IN_PORT 0x3200
#define FS_PORT 0xFF00

#define TRUE 1
#define FALSE 0
#define FS_OPEN 0
#define FS_READ 1
#define FS_WRITE 2
#define FS_LSEEK 3
#define FS_CLOSE 4
#define FS_ISOPEN 5

// ****** for open ******
#define OPN_RDONLY	1<<0
#define OPN_WRONLY	1<<1
#define OPN_RDWR	1<<2
#define OPN_CREAT	1<<3
#define OPN_TRUNC	1<<4
#define OPN_APPEND	1<<5

//******* for mode ***
#define	M_IRWXU	1<<0			/* RWX mask for owner */
#define	M_IRUSR	1<<1			/* R for owner */
#define	M_IWUSR	1<<2			/* W for owner */
#define	M_IXUSR	1<<3			/* X for owner */

// ******* for lseek ***
/* whence values for lseek(2) */
#define	LSEEK_SET	1<<0	/* set file offset to offset */
#define	LSEEK_CUR	1<<1	/* set file offset to current plus offset */
#define	LSEEK_END	1<<2	/* set file offset to EOF plus offset */

// ****** for access *****
/* access function */
#define	F_OKAY		1<<0	/* test for existence of file */
#define	X_OKAY		1<<1	/* test for execute or search permission */
#define	W_OKAY		1<<2	/* test for write permission */
#define	R_OKAY		1<<3	/* test for read permission */


char data[MAX_DATA];

struct file_handler {
	int op;
	int fd;
	int flag;
	void *op_struct;
} fh;

struct open_file {
	// arguments
	int flags;
	char *pathname;
	int mode;
	// return
	int fd;
} opn;

struct read_file {
	int fd;
	size_t size;
	// return;
	char *buf;
	int ssize;
} rd;

struct write_file {
	int fd;
	char *buf;
	size_t count;
	// return
	int ssize;
} wr;

struct lseek_file {
	int fd;
	int offset;
	int whence;
	// return
	int foffset;
} lsk;