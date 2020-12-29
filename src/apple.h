// Make 64bit functions Apple compatible 

#ifdef __APPLE__

#define lseek64 lseek 
#define open64 open 
#define pread64 pread

#endif

