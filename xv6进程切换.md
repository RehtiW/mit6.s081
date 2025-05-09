当CPU设置完成后，调用scheduler函数（视为调度线程），该函数无限循环，寻找可运行的进程
当找到时，将当前CPU调度线程的上下文保存在cpu->context，将可运行进程的上下文加载到寄存器，
直到下一次时钟中断，通过**中断处理的swich()** 再将当前进程的上下文保存在当前进程结构体proc->context，
将当前寄存器**重新设置为CPU调度线程的上下文**，**从调度进程调用的swich()下一条语句继续执行**，继续循环寻找可运行进程

```c
void

scheduler(void)

{

  struct proc *p;

  struct cpu *c = mycpu();

  c->proc = 0;

  for(;;){

    // Avoid deadlock by ensuring that devices can interrupt.

    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {

      acquire(&p->lock);

      if(p->state == RUNNABLE) {

        // Switch to chosen process.  It is the process's job

        // to release its lock and then reacquire it

        // before jumping back to us.

        p->state = RUNNING;

        c->proc = p;

        swtch(&c->context, &p->context);
        // ...

        // Process is done running for now.

        // It should have changed its p->state before coming back.

        c->proc = 0;

      }

      release(&p->lock);

    }

  }

}
```
时钟中断->trap.c->yield()->sched()->swch.s
yield将进程状态设置为RUNNABLE，调用sched
sched检查一系列状态，调用swtch
swtch保存ra,sp及所有callee saved regs(被调用者保证函数运行前后不改变其状态，即保存入栈后加载出栈)到进程上下文，加载CPU调度进程上下文到寄存器
不需要保存pc是因为没有必要，
由加载的新ra决定程序计数器
```asm
# Context switch
#   void swtch(struct context *old, struct context *new);
# Save current registers in old. Load from new.
swtch:

        sd ra, 0(a0)
        ....
  

        ld ra, 0(a1)
        ....

        ret

```