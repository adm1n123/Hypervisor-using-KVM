/* open/fcntl.  */
#define O_ACCMODE           0003
#define O_RDONLY             00
#define O_WRONLY             01
#define O_RDWR                     02
#ifndef O_CREAT
# define O_CREAT           0100        /* Not fcntl.  */
#endif
#ifndef O_EXCL
# define O_EXCL                   0200        /* Not fcntl.  */
#endif
#ifndef O_NOCTTY
# define O_NOCTTY           0400        /* Not fcntl.  */
#endif
#ifndef O_TRUNC
# define O_TRUNC          01000        /* Not fcntl.  */
#endif
#ifndef O_APPEND
# define O_APPEND          02000
#endif
#ifndef O_NONBLOCK
# define O_NONBLOCK          04000
#endif
#ifndef O_NDELAY
# define O_NDELAY        O_NONBLOCK
#endif
#ifndef O_SYNC
# define O_SYNC               04010000
#endif
#define O_FSYNC                O_SYNC
#ifndef O_ASYNC
# define O_ASYNC         020000
#endif
#ifndef __O_LARGEFILE
# define __O_LARGEFILE        0100000
#endif
#ifndef __O_DIRECTORY
# define __O_DIRECTORY        0200000
#endif
#ifndef __O_NOFOLLOW
# define __O_NOFOLLOW        0400000
#endif
#ifndef __O_CLOEXEC
# define __O_CLOEXEC   02000000
#endif
#ifndef __O_DIRECT
# define __O_DIRECT         040000
#endif
#ifndef __O_NOATIME
# define __O_NOATIME   01000000
#endif
#ifndef __O_PATH
# define __O_PATH     010000000
#endif
#ifndef __O_DSYNC
# define __O_DSYNC         010000
#endif
#ifndef __O_TMPFILE
# define __O_TMPFILE   (020000000 | __O_DIRECTORY)
#endif

// ******* for lseek **********************************************************
/* whence values for lseek(2) */
#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

#ifndef _POSIX_SOURCE
/* whence values for lseek(2); renamed by POSIX 1003.1 */
#define	L_SET		SEEK_SET
#define	L_INCR		SEEK_CUR
#define	L_XTND		SEEK_END
#endif
#ifdef __USE_GNU
# define SEEK_DATA      3       /* Seek to next data.  */
# define SEEK_HOLE      4       /* Seek to next hole.  */
#endif


//*********** for mode **********************************************************
#define	S_IRWXU	0000700			/* RWX mask for owner */
#define	S_IRUSR	0000400			/* R for owner */
#define	S_IWUSR	0000200			/* W for owner */
#define	S_IXUSR	0000100			/* X for owner */

// ********* for access ********************************************************
/* access function */
#define	F_OK		0	/* test for existence of file */
#define	X_OK		0x01	/* test for execute or search permission */
#define	W_OK		0x02	/* test for write permission */
#define	R_OK		0x04	/* test for read permission */