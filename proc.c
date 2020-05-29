#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "prioritylock.h"
#include "sleeplock.h"

#define ALARM_HANDLER "alarm_handler"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc* probin;
  int rindex;
} ptable;

struct{
  int cnt;
  struct spinlock lock;
} count;

struct {
  struct spinlock lock;
  struct proc proc[NAPROC];
  uint curr;
} hptable;

struct {
  struct spinlock lock;
  int seed;
} randnum;


static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

static char *states_str[] = {
    [UNUSED]    "UNUSED ",
    [EMBRYO]    "EMBRYO ",
    [SLEEPING]  "SLEEPING",
    [RUNNABLE]  "RUNNABLE",
    [RUNNING]   "RUNNING ",
    [ZOMBIE]    "ZOMBIE "
  };

unsigned int
rand()
{
  int num;
  acquire(&randnum.lock);
  randnum.seed = randnum.seed * 1664525 + 1013904223;
  num = randnum.seed;
  release(&randnum.lock);
  return num;
  // return ticks * 1664525 + 1013904223;
}

int pow(int x, int y) 
{ 
    if (y == 0) 
        return 1; 
    else if (y%2 == 0) 
        return pow(x, y/2)*pow(x, y/2); 
    else
        return x*pow(x, y/2)*pow(x, y/2); 
} 

void reverse(char* str, int len) 
{ 
    int i = 0, j = len - 1, temp; 
    while (i < j) { 
        temp = str[i]; 
        str[i] = str[j]; 
        str[j] = temp;
        i++;
        j--;
    }
}

int intToStr(int x, char str[], int d)
{
    int i = 0;
    while (x) {
        str[i++] = (x % 10) + '0';
        x = x / 10;
    }
  
    while (i < d)
        str[i++] = '0';
  
    reverse(str, i);
    str[i] = '\0';
    return i;
} 
  
void ftoa(float n, char* res, int afterpoint) 
{ 
    int ipart = (int)n; 
  
    float fpart = n - (float)ipart; 
  
    int i = intToStr(ipart, res, 0); 
  
    if(afterpoint != 0) {
        res[i] = '.';
        fpart = fpart * pow(10, afterpoint); 
        intToStr((int)fpart, res + i + 1, afterpoint); 
    }
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
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
  p->locked_in_queue = 0;
  p->locked_next = 0;
  p->level = 0;
  acquire(&tickslock);
  p->arrival_time = ticks;
  release(&tickslock);
  p->tickets = 13;
  p->cycle_num = 1;
  p->waited_cycles = 0;
  p->state = EMBRYO;
  p->pid = nextpid++;
  
  release(&ptable.lock);

  // Allocate kernel stack.
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

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
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

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
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
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  acquire(&hptable.lock);
  // cprintf("exit : %d %s -> %d\n", curproc->pid, curproc->syscalls_name[0], curproc->syscalls_out[0]);
  hptable.proc[hptable.curr++] = *curproc;
  // cprintf("exit2: %d %s -> %d\n", hptable.proc[hptable.curr-1].pid, hptable.proc[hptable.curr-1].syscalls_name[0], hptable.proc[hptable.curr-1].syscalls_out[0]);
  memset(curproc->syscalls_name, 0, sizeof(curproc->syscalls_name));
  memset(curproc->syscalls_out, 0, sizeof(curproc->syscalls_out));
  curproc->psys = 0;
  release(&hptable.lock);

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
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
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
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
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // int level0 = 0, level1 = 0, level2 = 0;
    // acquire(&ptable.lock);
    // for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    //   if(p->state != RUNNABLE) continue;
    //   else{
    //     if(p->level == 0) level0 = 1;
    //     if(p->level == 1) level1 = 1;
    //     if(p->level == 2) level2 = 1;
    //   }
    // }

    // if(level0)
    //   lottery_scheduling();
    // else if(level1)
    //   RR_scheduling();
    // else if(level2)
    //   HRRN_scheduling();
    
    // release(&ptable.lock);


    // Loop over process table looking for process to run. --------------- OLD BUT GOLD :/
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock); // ------------------------------------------- OLD BUT GOLD :/

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
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
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
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

//PAGEBREAK: 36
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

int
count_of_digit(int num){
  int count = 1;
  while ((num /= 10) != 0) 
    count += 1;
  return count;
}

int
print_syscalls(void){
  acquire(&hptable.lock);
  acquire(&ptable.lock);
  cprintf("####### DONE PROCESSES #######\n");
  for(int i = 0; i < hptable.curr; i++){
    struct proc* p = &hptable.proc[i];
    cprintf("Proc %d:\n", p->pid);
    for(int j = 0; j < p->psys; j++){
      cprintf("\t-%s:%d\n", p->syscalls_name[j], p->syscalls_out[j]);
    }
  }
  cprintf("##### RUNNING PROCESSES ######\n");
  for(int i = 0; ; i++){
    struct proc* p = &ptable.proc[i];
    if(p->pid == 0) break;
    cprintf("Proc %d:\n", p->pid);
    for(int j = 0; j < p->psys; j++){
      cprintf("\t-%s:%d\n", p->syscalls_name[j], p->syscalls_out[j]);
    }
  }
  release(&ptable.lock);
  release(&hptable.lock);
  return 0;
}


// handling incoming alarms
void
handle_alarms(void)
{
  uint currticks;
  struct proc *curproc = myproc();
  safestrcpy(curproc->name, ALARM_HANDLER, sizeof(ALARM_HANDLER));

  for(;;)
  {
    curproc = myproc();
    int pre_alarm_ticks;
    if (curproc->alarm_ticks > 0)
    {
    newone:
      acquire(&tickslock);
      currticks = ticks;
      release(&tickslock);
      pre_alarm_ticks = curproc->alarm_ticks;
      while (ticks - currticks < curproc->alarm_ticks || pre_alarm_ticks != curproc->alarm_ticks)
      {
        curproc = myproc();
        if (pre_alarm_ticks != curproc->alarm_ticks)
          goto newone;
      }
      cprintf("ALARM !!!\n");
      struct proc *p;
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (strncmp(p->name, ALARM_HANDLER, sizeof(p->name)) == 0)
          p->alarm_ticks = 0;
    }
  }
}


void
set_alarm(int msec)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (strncmp(p->name, ALARM_HANDLER, sizeof(p->name)) == 0)
    {
      acquire(&ptable.lock);
      p->alarm_ticks = msec/10;    
      release(&ptable.lock);
    }
}

void
set_edx(int value){
  // asm volatile(
  //   "movl %0, %%edx" 
  //   : // OUTPUT
  //   : "r"(num) // INPUT
  //   : "%edx"
  // );
  myproc()->tf->edx = value;
}

void
read_registers(void){
  struct proc* p = myproc();
  struct trapframe* tf = p->tf;
  cprintf("pid = %d\n", p->pid);
  cprintf("\
##### GP registes #####\n\
\teax = %d,\n\tebx = %d,\n\tecx = %d,\n\tedx = %d\n\tesi = %d\n\
\tedi = %d\n\tebp = %d\n\tesp = %d\n",
tf->eax, tf->ebx, tf->ecx, tf->edx, tf->esi, tf->edi, tf->ebp, tf->esp);

  cprintf("##### PC registers #####\n\teip = %d\n", tf->eip);
  cprintf("### Segment registers ##\n\
\tcs = %p\n\tds = %p\n\tes = %p\n\tfs = %p\n\tgs = %p\n\tss = %p\n", 
tf->cs, tf->ds, tf->es, tf->fs, tf->gs, tf->ss);
}

void print_proc_info()
{
  acquire(&tickslock);
  uint now = ticks;
  release(&tickslock);
  cprintf("Name\t\tPID\t\tState\t\tLevel\t\tTickets\t\tcycle numbers\t\tHRRN\t\twaited cycles\n");

  struct proc *p;
  static char hrrn_ratio_str[20];
  float hrrn_ratio;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    hrrn_ratio = (float)(now - p->arrival_time)/(float)p->cycle_num;
    ftoa(hrrn_ratio, hrrn_ratio_str, 3);
    cprintf("%s\t\t%d\t\t%s\t\t%d\t\t%d\t\t%d\t\t%s\t\t%d\n",
            p->name,
            p->pid,
            states_str[p->state],
            p->level,
            p->tickets,
            p->cycle_num,
            hrrn_ratio_str,
            p->waited_cycles);
  }
}

void 
lottery_scheduling(void){
  struct proc *p;
  struct cpu *c = mycpu();
  int total_tickets = 0;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->level == 0)
      total_tickets += p->tickets;
  int random_ticket = rand() % (total_tickets) + 1;
  int cnt = 0;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->level == 0)
    {
      cnt += p->tickets;
      if (random_ticket <= cnt)
      {
        context_switch(p, c);
        break;
      }
    }
}

void
RR_scheduling(void){
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  // version 3 (Works Well)
  if((ptable.rindex == 0) || (ptable.rindex == NPROC))
    ptable.rindex= 0;

  for(; ptable.rindex < NPROC; ptable.rindex++){
    p = &ptable.proc[ptable.rindex];
    // if(p->pid != 0) cprintf("\t%s %d\n", p->name, ptable.rindex);
    if(p->state == RUNNABLE && p->level == 1){
      context_switch(p, c);
      ptable.rindex++;
      return;
    }
  }

  // version 2(Does not Work)
  // if((!ptable.probin) || (ptable.probin == &ptable.proc[NPROC]))
  //   ptable.probin = ptable.proc;

  // p = ptable.probin;
  // for(; p < &ptable.proc[NPROC]; p++){
  //   if(p->state == RUNNABLE && p->level == 1){
  //     context_switch(p, c);
  //     p++;
  //     return;
  //   }
  // }
}

void
HRRN_scheduling(void){
  struct proc *p;
  struct cpu *c = mycpu();
  float max_hrrn = -1;
  int max_hrrn_pid = -1;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->level == 2 && p->state == RUNNABLE)
    {
      acquire(&tickslock);
      float waiting = (float)(ticks - p->arrival_time);
      release(&tickslock);
      float curr_hrrn = waiting / (float)(p->cycle_num);
      if (curr_hrrn > max_hrrn)
      {
        max_hrrn = curr_hrrn;
        max_hrrn_pid = p->pid;
      }
    }
  }
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->level == 2 && p->state == RUNNABLE && p->pid == max_hrrn_pid)
    {
      context_switch(p, c);
      break;
    }
  }
}

void
set_proc_tickets(int pid, int ticket_len){
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->pid == pid)
    {
      p->tickets = ticket_len;
      break;
    }
}

void
set_proc_level(int pid, int level){
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->pid == pid)
    {
      p->level = level;
      break;
    }
}

void
context_switch(struct proc* p, struct cpu* c){
  if(p->state != RUNNABLE) return;
  p->cycle_num++;
  c->proc = p;
  switchuvm(p);
  p->state = RUNNING;
  swtch(&(c->scheduler), p->context);
  switchkvm();
  c->proc = 0;
  age_processes(p->pid);
}

void age_processes(int runner_pid) {
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state == UNUSED)
      continue;
    if (p->pid != runner_pid)
      if(++p->waited_cycles >= 2500)
        set_proc_level(p->pid, 0);
  }
}

struct{
  struct prioritylock lock;
  int stuff;
} testlock;

void
check_lock(void){
    // sleep lock
    // acquiresleep(&testlock.lock);
    // testlock.stuff += 1;
    // cprintf("proc, %d:\n", myproc()->pid);
    // for(int i = 0; i < 100090000; i++)
    //   testlock.stuff *= 138957;
    // cprintf("stuff for pid %d is %d\n", myproc()->pid, testlock.stuff);
    // releasesleep(&testlock.lock);

    // priority lock
    pacquiresleep(&testlock.lock);
    testlock.stuff += 1;
    cprintf("proc, %d:\n", myproc()->pid);
    for(int i = 0; i < 100090000; i++)
      testlock.stuff *= 138957;
    cprintf("stuff for pid %d is %d #DONE\n", myproc()->pid, testlock.stuff);
    preleasesleep(&testlock.lock);

    // spinlock
    // acquire(&testlock.lock);
    // testlock.stuff += 1;
    // cprintf("proc, %d\n", myproc()->pid);
    // for(int i = 0; i < 100090000; i++)
    //   testlock.stuff *= 138957;
    // cprintf("stuff for pid %d is %d\n", myproc()->pid, testlock.stuff);
    // release(&testlock.lock);
}

void
sys_count()
{
  acquire(&count.lock);
  cprintf("Total count: %d\n", count.cnt);
  release(&count.lock);

  for (int i = 0; i < ncpu; ++i) 
    cprintf("CPU count: %d \n", cpus[i].sys_counter);
}

void
increment_syscount(){
  acquire(&count.lock);
  count.cnt++;
  release(&count.lock);
}

void increment_cpu_syscount() {

  // 1
  pushcli();
  struct cpu *c = mycpu();
  popcli();
  cpuacquire(&(c->lock));
  c->sys_counter++;
  cpurelease(&c->lock);

  // 2
  // Envolving incrementation (++) inside the pushcli and popcli area (with interrupts disabled) :
  // pushcli();
  // struct cpu *c = mycpu();
  // cpuacquire(&(c->lock));
  // c->sys_counter++;
  // cpurelease(&c->lock);
  // popcli();

  // 3
  // Doing it without Lock at all! No preemption will happen by disabling interrupts :
  // pushcli();
  // mycpu()->sys_counter++;
  // popcli();
  
}