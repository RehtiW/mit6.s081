## part1
printf占位符
%s 输出字符串 传入参数是字符串地址，按照字符格式，直到读取到'\0'
%x 以十六进制形式输出整数
## part2_Backtrace
每调用一个函数，函数为自己在栈中创建一个栈帧(stack frame)
栈从高地址向低地址使用，栈总是向下增长
![[Pasted image 20250222111422.png]]
将图示三个栈帧从上往下命名为A,B,C
假设函数A是main函数，则函数A调用了B
B的fp-16指向A的fp
B的fp-8指向A调用B时的pc值

## part3_Alarm
### 时钟中断
注意要点:
user trapframe的还原
防止时钟中断在上一次sigreturn返回前，重入handler函数，打断上一次时钟中断
#### **1. 用户空间执行与中断触发**

- **用户程序运行**：例如执行`alarmtest.c`中的循环代码。
    
- **时钟中断发生**：当CPU的定时器计数器达到预设值（通过`CLINT`配置），触发时钟中断（timer interrupt）。
    

---

#### **2. 硬件自动切换到内核模式**

- **关键寄存器**：
    
    - **`stvec`**：陷阱处理入口地址，指向`trampoline.S`中的`uservec`。
        
    - **`scause`**：中断原因码，时钟中断对应值为`0x8000000000000005`（supervisor timer interrupt）。
        
- **流程**：
    
    1. 硬件保存当前PC到`sepc`寄存器。
        
    2. 关闭中断（`sstatus.SIE`清零）。
        
    3. 跳转到`stvec`指向的地址（即`uservec`）。
        

---

#### **3. 保存用户上下文到陷阱帧（`trapframe`）**
- **文件**：`kernel/trampoline.S`
#### **4. 内核陷阱处理（`usertrap`函数）**

- **文件**：`kernel/trap.c`
- - **Alarm机制触发**：当进程的`ticks`达到`interval`时，保存当前上下文到`trapframe_copy`，并设置`epc`为`handler`地址。
#### **5. 准备返回用户空间（`usertrapret`函数）**

- **文件**：`kernel/trap.c`
- **作用**：配置返回用户空间所需的寄存器，并调用`userret`。
#### **6. 恢复用户上下文并返回（`userret`函数）**

- **文件**：`kernel/trampoline.S`
- **关键操作**：
    - 恢复用户寄存器（包括修改后的`epc`）。
    - 执行`sret`指令，跳转到`epc`指向的地址（即`handler`函数）。
#### **7. handler函数执行与返回**

- **用户空间处理函数**：例如`alarmtest.c`中的`periodic`函数。
    
- **返回内核**：处理函数结束后，需调用`sigreturn`系统调用：
- **`sigreturn`系统调用**：
    - **作用**：将保存的`trapframe_copy`还原，确保用户程序从中断点继续执行。
