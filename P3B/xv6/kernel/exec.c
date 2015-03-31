#include "types.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, st, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;

  if((ip = namei(path)) == 0)
    return -1;
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  //sz = 0; *keep zero for static stack region as second page
  sz = 4096; //USE PAGE SIZE TO ALLOW NULL DEREFERENCE CRASH MECHANISM
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    /*
     *Alocuvm allocates free page for code, sz holds address of end of code region
     *that is, it holds end virtual address of code space
     */
    if((sz = allocuvm(pgdir, sz, ph.va + ph.memsz)) == 0)
      goto bad;

    /*
     *copy contents of file into the allocated page.
     *AKA:: copy code into the page allocated for code region
     */
    if(loaduvm(pgdir, (char*)ph.va, ip, ph.offset, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  ip = 0;

  /**PART B MOD.3*************************************************/
  /*we just allocated the code page, now the next page will be 
   the heap. Mark the top of the heap.
  */

  //round up to the nearest page value
  //THIS IS WHERE THE HEAP WILL START
  sz = PGROUNDUP(sz);

  //sz will refer to end of code/begining of heap
  proc->sz = sz;
  proc->code_top = sz;
  /*******************************************************************/
  
  /*
   *allocate new page right after code region (size of page)::OLD IMPLEMENTATION::IGNORE

  if((sz = allocuvm(pgdir, sz, sz + PGSIZE)) == 0)
    goto bad;
  */

  //****PART B MOD.1****************************************************************************
  //following code will instead put stack at end of user space
  
  //STACK TOP WILL NOT BE ABOVE WHERE STACK IS, STACK TOP WILL 
  //BE MARKED AS EXACTLY WHERE TOP STACK BOUND BEGINS
  //UNLIKE HEAP TOP, WHERE ITS MARKED WHERE THE NEW PAGE BEGINS *THIS IS IMPORTANT!!*
  st = USERTOP - PGSIZE;
  if((sz = allocuvm(pgdir, st, USERTOP)) == 0)
    goto bad;

  proc->stack_top = st;

  // Push argument strings, prepare rest of stack in ustack.
  sp = USERTOP; //stack is at bottom of user space, start the pointer there 
  //*******************************************************************************************//


  /*copy cmd line args into stack*/
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

  ustack[0] = 0xffffffff;  // fake return PC  <--specific to xv6 (dummy frame pointer)
  /*
   * by using 'return' natively, xv6 will crash because once main completes it will return this
   * popped return address, but xffffff... is not mapped to physical page (its -1) so you will page fault
   */
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(proc->name, last, sizeof(proc->name));

  // Commit to the user image.
  oldpgdir = proc->pgdir;
  proc->pgdir = pgdir;
  proc->tf->eip = elf.entry;  // main
  proc->tf->esp = sp;
  switchuvm(proc);
  freevm(oldpgdir);

  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip)
    iunlockput(ip);
  return -1;
}
