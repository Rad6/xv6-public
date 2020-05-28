// Priority locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "prioritylock.h"
#define NULL ((void*)0)

void
print_lock_queue(struct proc* p){
  cprintf("FROM %d QUEUE:\n", myproc()->pid);
  struct proc* cur = p;
  while(cur != 0){
    cprintf("%d, ", cur->pid);
    cur = cur->locked_next;
  }
  cprintf("\n");
}

void
initprioritylock(struct prioritylock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
  lk->locked_head = NULL;
}

void
pacquiresleep(struct prioritylock *lk)
{
  acquire(&lk->lk);
  // int gone = 0;
  
  if(lk->locked_head == NULL){
    lk->locked_head = myproc();
    myproc()->locked_in_queue = 1;
  }
  
  while (lk->locked || (lk->locked_head->pid != myproc()->pid)) {
    
    if(!myproc()->locked_in_queue){
      myproc()->locked_in_queue = 1;
      struct proc* cur  = lk->locked_head;
      struct proc* prev = NULL;
      while(1){
        if(cur->pid < myproc()->pid){
          if(prev)
            prev->locked_next = myproc();
          else
            lk->locked_head = myproc();
          myproc()->locked_next = cur; 
          break;
        }
        else{
          if(!cur->locked_next){
            cur->locked_next = myproc();
            break;
          }
          prev = cur;
          cur  = cur->locked_next;
        }
      }
    }
    print_lock_queue(lk->locked_head);
    sleep(lk, &lk->lk);
  }
  print_lock_queue(lk->locked_head);
  lk->locked_head = lk->locked_head->locked_next;
  myproc()->locked_in_queue = 0;
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

void
preleasesleep(struct prioritylock *lk)
{
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  // lk->locked_head->locked_next = NULL;
  wakeup(lk);
  release(&lk->lk);
}

int
pholdingsleep(struct prioritylock *lk) // plutocholia: idk what this is for!
{
  int r;
  
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}