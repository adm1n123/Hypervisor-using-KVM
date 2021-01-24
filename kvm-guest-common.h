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
#define FS_SEEK 3
#define FS_CLOSE 4

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
