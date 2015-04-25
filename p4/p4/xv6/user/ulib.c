#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"

#define  NPROC  64
#define  PGSIZE 4096

static void* thread_table[NPROC];

char*
strcpy(char *s, char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, void *vsrc, int n)
{
  char *dst, *src;
  
  dst = vdst;
  src = vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}
// MOD.17
int
thread_create(void(*start_routine)(void*), void *arg) 
{

  int pid;
  void *SP;

  if( (SP = malloc(PGSIZE - 2*sizeof(void*))) == NULL )
  //if( (SP = malloc(2*PGSIZE)) == NULL )
  {
  	return -1;
  }
  //if( (uint)SP % PGSIZE )
  //{
  //	SP = SP + (PGSIZE - (uint)SP%PGSIZE);
  //}
  
  // Index the thread's stack for freeing
  pid = clone(start_routine, arg, SP);

  thread_table[pid] = SP;

  return pid;

}
// MOD.18
int
thread_join(int pid)
{
  void *SP;
  
  int ret = join(pid);

  SP = thread_table[ret];

  free(SP);
  
  return ret;
}
