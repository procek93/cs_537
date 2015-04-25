#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

struct spinlock sbrk;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p;
  pde_t *curr_page;
  
  // Acquire the sbrk lock for thread safety
  //acquire(&sbrk);
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
//  proc->sz = sz;
//  switchuvm(proc);

  // Set the page variable to the current processes page directory
  curr_page = proc->pgdir;

  // Iterate over the processes in the page table and update the
  // sizes of all procs that share the same page directory as the
  // process that originally called sbrk
  acquire(&ptable.lock);
  for(p = ptable.proc ; p <&ptable.proc[NPROC] ; p++)
  {
    if(p->pgdir == curr_page)
    {
      p->sz = sz;
      acquire(&sbrk);
      switchuvm(p);
      release(&sbrk);
    }
  }
  release(&ptable.lock);

  //release(&sbrk);

  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  np->thread = 0;	// MOD.19
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");
/*
  // Carry on if the exiting proc is a thread
  if(proc->thread == 1)
  {
    proc->state = ZOMBIE;
    sched();
  }
*/
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  if(proc->thread != 1)
  {
    // Pass abandoned children to init
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if(p->parent == proc)
      {
        if(p->thread == 1)
        {
          p->state = ZOMBIE;
	  kill(p->pid);
	  join(p->pid);
        }

        else
        {
          p->parent = initproc;
          if(p->state == ZOMBIE)
          {
   	    wakeup1(initproc);
          }
        }
      }
    }
  }
  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if( p->parent != proc ||
	  p->pgdir == proc->pgdir ||
          p->thread == 1 )
        continue;

      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// Create a kernal thread - MOD.1
int
clone(void(*fnc)(void*), void *arg, void *stack)
{
  int i, pid;
  struct proc *thread;

  // Allocate a slot in the proc table for the new thread
  if( (thread = allocproc()) == 0 )
  	return -1;
  
  // Set up the thread's fields so that it shares resources 
  // with the calling process while maintaining its own stack
  thread->sz 	 = proc->sz;
  thread->pgdir  = proc->pgdir;
  thread->stack  = stack;	/* This will be the location
  				   of the thread's stack, which
				   was added to proc.h */
  
  thread->thread = 1;		// Mark this proc as a thread
  thread->state  = RUNNABLE;
  
  // Set the parents accordingly
  if(proc->thread == 1)
    thread->parent = proc->parent;
  else
    thread->parent = proc;

  *thread->tf 	 = *proc->tf;	// Fields of tf set below
 
  // Copy the file descriptors of the parent process.
  // Code taken from system call fork().             
  for(i = 0; i < NOFILE; i++)
  {
  	if(proc->ofile[i])
	{
      		thread->ofile[i] = filedup(proc->ofile[i]);
	}
  }

  // Thread will have the same directory as its parent
  thread->cwd = idup(proc->cwd);

  void *stackArgs;
  void *stackPtr;
/*
////////////////////////////////////////////////////////////////////////
  // Push argument strings, prepare rest of stack in ustack.
  sp = sz;
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp &= ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
////////////////////////////////////////////////////////////////////////
*/
  // A page minus the header of said page,
  // allocated with the arg parameter
  stackPtr = stack + PGSIZE - (2 * sizeof(void*));
  *(uint *)stackPtr = 0xffffffff; // Fake return PC
  
  stackArgs = stack + PGSIZE -  sizeof(void*);
  *(uint *)stackArgs = (uint)arg;

  thread->tf->esp = (int)stack;
  
  // Move the newly allocated thread stack to the thread's
  // trap frame stack pointer
  memmove((void*)thread->tf->esp, stack, PGSIZE);

  // Set the trap frame parameters of the new thread, specifically
  // register EIP as the specified entry point of the fnc parameter
  thread->tf->esp += PGSIZE - (2 * sizeof(void*));
  thread->tf->ebp = thread->tf->esp;
  thread->tf->eip = (uint)(fnc);

  // This code was in fork, so I yoinked it
  safestrcpy(thread->name, proc->name, sizeof(proc->name));
  
  // Set up the thread's process id just before return
  pid = thread->pid;
  
  // Go onward little one, make us proud
  return pid;
}

// Kill the thread by joining it back with its parent process - MOD.2
int
join(int pid)
{

  struct proc *thread;
  int havekids, ret_pid;

/*
  if(proc->thread != 1)
    return -1;
*/
  acquire(&ptable.lock);
  for(;;){
    
    // Scan through table looking for zombie children.
    havekids = 0;

    if(pid == -1)
    {

      for(thread = ptable.proc; thread < &ptable.proc[NPROC]; thread++)
      {
        if( (thread->thread != 1) ||
	    (thread->parent != proc) ||
      	    (thread->pgdir  != proc->pgdir) )
           continue;
 	
	// The process has atleast one child thread
        havekids = 1;

        if(thread->state == ZOMBIE)
	{

	   //void* stackPtr = (void *)thread->parent->tf->esp + 7*sizeof(void*);
   	   //(uint *)stackPtr = thread->tf->ebp;
 	   //(uint *)stackPtr += 3*sizeof(void *) - PGSIZE;

           kfree(thread->kstack);
           thread->kstack = 0;
           thread->state = UNUSED;

 	   // Save the pid for returning
	   ret_pid = thread->pid;

           thread->pid = 0;
           thread->parent = 0;
           thread->name[0] = 0;
           thread->killed = 0;

	   // Release the lock so livelock doesn't occur
           release(&ptable.lock);
           return ret_pid;
	}
      }
    }
    
    else
    {
     
      for(thread = ptable.proc; thread < &ptable.proc[NPROC]; thread++)
      {
        if( thread->pid == pid )
	{
          if( (thread->thread != 1) ||
	      (thread->parent != proc) ||
      	      (thread->pgdir  != proc->pgdir) )
             continue;

	  // The process has at least one child thread
      	  havekids = 1;

          if(thread->state == ZOMBIE)
	  {

	     //void* stackPtr = (void *)thread->parent->tf->esp + 7*sizeof(void*);
   	     //(uint *)stackPtr = thread->tf->ebp;
 	     //(uint *)stackPtr += 3*sizeof(void *) - PGSIZE;

             kfree(thread->kstack);
             thread->kstack = 0;
             thread->state = UNUSED;

             thread->pid = 0;
             thread->parent = 0;
             thread->name[0] = 0;
             thread->killed = 0;

	     // Release the lock so livelock doesn't occur
             release(&ptable.lock);
             return pid;
	  }
	}
      }
    }
  
    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
  return -1;
}
