#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "psched.h"

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
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint target_ticks;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  target_ticks = ticks + n;
  acquire_ptable_lock();
  myproc()->wakeup_ticks = target_ticks;
  release_ptable_lock();
  sleep(&ticks, &tickslock);
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_nice(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  if(n < 0 || n > 20)
    return -1;
  return nice(n);
}

int
sys_getschedstate(void)
{
  struct pschedinfo *psched;
  if(argptr(0, (char**)&psched, sizeof(struct pschedinfo)) < 0)
    return -1;
  if(!psched)
    return -1;
  getschedstate(psched);
  return 0;
}
