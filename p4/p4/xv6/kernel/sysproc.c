#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "sysfunc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since boot.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// MOD.9
int sys_clone(void)
{

  void *fnc, *arg, *stack;
  
  // Check the pointers
  if( argptr(0, (void*)&fnc, sizeof(void*)) < 0 )
  	return -1;

  if( argptr(1, (void*)&arg, sizeof(void*)) < 0 )
  	return -1;
  
  if( argptr(2, (void*)&stack, sizeof(void*)) < 0 )
  	return -1;
  
  // Sanity check on the page boundary
  if( (uint)stack % PGSIZE != 0 )
  	return -1;

  // Process cannot allocate a stack that will
  // extend beyond the boundary of its size
  if( (uint)proc->sz - (uint)stack == PGSIZE/2)
  	return -1;

  return clone(fnc, arg, stack);

}

// MOD.10
int sys_join(void)
{
  
  int pid;

  if(argint(0, &pid) < 0)
    return -1;

  return join(pid);

}

//====================================================//
//CONDITIONAL VARIABLE SYSCALLS
// MOD.P.12
int sys_cv_sleep(void)
{
	//expecting to get a lock and a conditional variable
	void * t_cond_t;
	void * t_lock_t;

	//retrieve the arguments
	if( argptr(0, (void*)&t_cond_t, sizeof(void*)) < 0 )
  	  return -1;

	if( argptr(1, (void*)&t_lock_t, sizeof(void*)) < 0 )
  	  return -1;

	//do proper casting
	cond_t * my_cv = (cond_t *)t_cond_t;
	lock_t * my_lock = (lock_t *)t_lock_t;

	//call the kernel side system function call

	cv_sleep(my_cv, my_lock);

	return 0;
}


// MOD.P.13
int sys_cv_wake(void)
{
	//only expecting to get a conditional variable
	void * t_cond_t;

	//retrieve the arguments
	if( argptr(0, (void*)&t_cond_t, sizeof(void*)) < 0 )
  	  return -1;

	//do proper casting
	cond_t * my_cv = (cond_t *)t_cond_t;

	//call the kernel side system function call
	//TODO

	cv_wake(my_cv);

	return 0;
	
}
//====================================================//

