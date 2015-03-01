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

//return the number of processes (iregardless of state)
//exist
int
sys_getprocs(void)
{

return num_procs();

}

//set tickets for process
//return 0 on succcess, else -1
int
sys_settickets(void)
{
	int tickets;
	long tix;
	//use accessory function to take in usermode passed in value
	//and assign it to tickets defined above
	if(argint(0, &tickets) < 0)
	{
		//error getting passed in value
		return -1;
	}

	tix = (long)tickets;

	//return -1 if ticket boundary cases are violated
	if((tix < MINTIX) || (tix > MAXTIX) || (tix % 10 != 0))
	{
		//error
		return -1;
	}

	//otherwise, ticket amount is fair
	//assign tickets to process currently running
	proc->tickets = tix;
	
	//set stride based on LCM division
	proc->stride = (LCM / tix);

	return 0;
}

//return statistical information
//regarding running processes
int
sys_getpinfo(void)
{
	//pointer to list of process statistic structures
	void *p;

	//user auxillary helper function to take in usermode input
	//and assign to p above
	if(argptr(0, (char **)&p, sizeof(p)) < 0)
	{
		//error in getting ptr
		return -1;
	}

	//fill in the pstat table with process info
	//return -1 on error
	if(fill_pstat(p) < 0)
	{
		return -1;
	}

	//return 0 on succes
	return 0;
}
