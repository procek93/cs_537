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
  if( (SP = malloc(2*PGSIZE)) == NULL )
  {
  	return -1;
  }
  if( (uint)SP % PGSIZE )
  {
  	SP = SP + (PGSIZE - (uint)SP%PGSIZE);
  }
  
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


//==================================================================================//
// MOD.P.1
//**BIG PETES LOCK SHOP, GET YE'R LOCKS HERE FRESH AND HOT**//
//**********************************************************//
//**WE GOT EM ALL: MUTUAL EXCLUSIVE, AND OFCOURSE FAIR!*****//
//**********************************************************//
//**ORDER NOW AND GET A BONUS INTIALIZE AND UNLOCK! WOW!  **//
//**WHAT A DEAL, 3 FUNCTIONS FOR THE PRICE OF ONE!        **//
//**********************************************************//
//**ORDER NOW AND WE'll THROW IN THE BONUS FETCH-AND-ADD! **//
//**********************************************************//
//**NOTE:: PG.303 OF TXT (LOCKS S.28.11 ON FETCH & ADD)   **//
//**********************************************************//

//**FUNCTION USED TO INITIALIZE LOCK**//
void lock_init(lock_t * lock)
{
	lock->ticket = 0;
	lock->turn = 0;
}

//**FUNCTION USED TO ACQUIRE A LOCK FOR A THREAD**//
void lock_acquire(lock_t * lock)
{
    volatile uint myturn = ulib_xchg(&lock->ticket, 1);
    while( myturn != (lock->turn))
      ; //spin
}

//**FUNCTION USED TO RELINQUISH A LOCK FOR A THREAD**//
void lock_release(lock_t * lock)
{
    ulib_xchg(&(lock->turn), 1);
}
//==================================================================================//
//==================================================================================//


//==================================================================================//
// MOD.P.4
/*
      .--..--..--..--..--..--.           _ - - - - - - - _
    .' \  (`._   (_)     _   \         -                   \         
  .'    |  '._)         (_)  |       /                       \ 
  \ _.')\      .----..---.   /     /                           \
  |(_.'  |    /    .-\-.  \  |   / Can you feel the conditional |            
  \     0|    |   ( O| O) | o|  /     Variables, Mr. Krabs?     /
   |  _  |  .--.____.'._.-.  | <                              /
   \ (_) | o         -` .-`  |  \                           _ 
    |    \   |`-._ _ _ _ _\ /     \                       _
    \    |   |  `. |_||_|   |       - _ _ _ _ _ _ _ _ _ /
    | o  |    \_      \     |     -.   .-.
    |.-.  \     `--..-'   O |     `.`-' .'
  _.'  .' |     `-.-'      /-.__   ' .-'
.' `-.` '.|='=.='=.='=.='=|._/_ `-'.'
`-._  `.  |________/\_____|    `-.'
   .'   ).| '=' '='\/ '=' |
   `._.`  '---------------'
           //___\   //___\
             ||       ||
             ||_.-.   ||_.-.
            (_.--__) (_.--__)

*/

/**function flow** (and assumed intended use)::
  -user thread will obtain lock 
  -lock will wait until lock available
  -once lock is available thread can proceed to critical section
  -critical section contains call to cv_wait
  -cv_wait takes in the conditional variable and lock
  -the current thread is added to the conditional variable queue
  -the lock is release, it is now free for another thread to take
  -current thread is put to sleep

example for multiple threads (queue use)::

  -**ASSUME OTHER THREAD STILL IN QUEUE
  -a different thread has been waiting for a lock
  -the previous thread went to sleep and released its lock
  -this new current (different) thread obtains lock, goes into crit section
  -crit section contains cv_wait
  -take in CV and lock
  -unlock the thread
  -put the thread to sleep
  -NOW 2 THREADS SLEEPING
  -this point is dependent on how the user implemented CVS and LOCKS
    ->signal will always wake up the first thread put to sleep (FIFO)
    ->but order is abstracted to us, its users job to ensure proper
    ->ordering

==NOTE==
  generally a parent will call cv_wait, put itself to sleep, and let
  the child process run. The child in turn will run cv_signal to 
  wake up the parent process.
========
*/
void cv_wait(cond_t * cond, lock_t * lock)
{
	//initilialize cv if not done already
	if(cond->init_fl != 1)
	{
		cond->init_fl = 1;
		cond->head = NULL;
	}
	
	//system call is in charge of queueing thread in sleep queue,
	//as well as releasing the lock and 
	cv_sleep(cond, lock);
}

void cv_signal(cond_t * cond)
{
	//initilialize cv if not done already
	if(cond->init_fl != 1)
	{
		cond->init_fl = 1;
		cond->head = NULL;
	}

	//system call is in charge of dequeueing first entry of wait queue
	//then wake that thread
	cv_wake(cond);
}

//==================================================================================//
//==================================================================================//

