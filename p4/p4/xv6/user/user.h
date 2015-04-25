#ifndef _USER_H_
#define _USER_H_

struct stat;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char*, int);
int mknod(char*, short, short);
int unlink(char*);
int fstat(int fd, struct stat*);
int link(char*, char*);
int mkdir(char*);
int chdir(char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
// MOD.11 - THESE ARE ALSO DEFINED IN usys.S
int clone(void(void*), void*, void*);
// MOD.12
int join(int);

//================================================//
// MOD.P.5
//**SYS_CALLS FOR CONDITIONAL VARIABLES
void cv_sleep(cond_t * cond, lock_t * lock);
void cv_wake(cond_t * cond);
//================================================//

// user library functions (ulib.c)
int stat(char*, struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, char*, ...);
char* gets(char*, int max);
uint strlen(char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
// MOD.13
int thread_create(void(*start_routine)(void*), void*);
// MOD.14
int thread_join();

// MOD.P.20
void lock_init(lock_t * lock);
void lock_acquire(lock_t * lock);
void lock_release(lock_t * lock);
void cv_wait(cond_t * cond, lock_t * lock);
void cv_signal(cond_t * cond);

#endif // _USER_H_

