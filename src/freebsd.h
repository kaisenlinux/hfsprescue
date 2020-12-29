// Make 64bit functions FreeBSD compatible 

#ifdef __FreeBSD__

#define lseek64 lseek 
#define open64 open 
#define pread64 pread

#endif

