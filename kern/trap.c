#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < ARRAY_SIZE(excnames))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];
	extern long entry_data[][3];//entry_data全局数组有三个变量，1：name 2:num 3: user 
	// 1为处理程序的名字，2为中断向量号，3为特权等级
	int i=0;
	for(i=0;entry_data[i][0]!=0;++i){
		SETGATE(idt[entry_data[i][1]],0,GD_KT,entry_data[i][0],entry_data[i][2]*3);
	}

	// LAB 3: Your code here.
	//初始化中断向量表，插入中断描述符，将中断向量号和中断处理代码关联起来
	// void divide_entry();
	// SETGATE(idt[T_DIVIDE],0,GD_KT,divide_entry,0);

	// void debug_entry();
	// SETGATE(idt[T_DEBUG],0,GD_KT,debug_entry,0);

	// void non_maskable_entry();
	// SETGATE(idt[T_NMI],0,GD_KT,non_maskable_entry,0);

	// void breakpoint_entry();
	// SETGATE(idt[T_BRKPT],0,GD_KT,breakpoint_entry,3);

	// void overflow_entry();
	// SETGATE(idt[T_OFLOW],0,GD_KT,overflow_entry,0);

	// void bounds_check_entry();
	// SETGATE(idt[T_BOUND],0,GD_KT,bounds_check_entry,0);

	// void invalid_opcode_entry();
	// SETGATE(idt[T_ILLOP],0,GD_KT,invalid_opcode_entry,0);

	// void device_entry();
	// SETGATE(idt[T_DEVICE],0,GD_KT,device_entry,0);

	// void double_fault_entry();
	// SETGATE(idt[T_DBLFLT],0,GD_KT,double_fault_entry,0);

	// void invalid_TSS_entry();
	// SETGATE(idt[T_TSS],0,GD_KT,invalid_TSS_entry,0);

	// void segment_not_present_entry();
	// SETGATE(idt[T_SEGNP],0,GD_KT,segment_not_present_entry,0);

	// void stack_exception_entry();
	// SETGATE(idt[T_STACK],0,GD_KT,stack_exception_entry,0);

	// void gplft_entry();
	// SETGATE(idt[T_GPFLT],0,GD_KT,gplft_entry,0);

	// void pgflt_entry();
	// SETGATE(idt[T_PGFLT],0,GD_KT,pgflt_entry,0);

	// void FPERR_entry();
	// SETGATE(idt[T_FPERR],0,GD_KT,FPERR_entry,0);

	// void align_entry();
	// SETGATE(idt[T_ALIGN],0,GD_KT,align_entry,0);

	// void mchk_entry();
	// SETGATE(idt[T_MCHK],0,GD_KT,mchk_entry,0);

	// void SIMDerr_entry();
	// SETGATE(idt[T_SIMDERR],0,GD_KT,SIMDerr_entry,0);

	// void syscall_entry();
	// SETGATE(idt[T_SYSCALL],0,GD_KT,syscall_entry,3);



	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;
	ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	switch (tf->tf_trapno)
	{
	case T_PGFLT:
		page_fault_handler(tf);
		return;
	case T_BRKPT:
		monitor(tf);
		return;
	case T_SYSCALL:
		/*
		一次傻吊的报错：应该将系统调用的返回值存放到eax寄存器中，被下一步的程序调用，
		忘记将结果保存到eax寄存器中，导致下一步使用了当前eax寄存器的值，在此例子中
		为系统调用类型列举号
		*/
		tf->tf_regs.reg_eax=syscall(tf->tf_regs.reg_eax,
		tf->tf_regs.reg_edx,
		tf->tf_regs.reg_ecx,
		tf->tf_regs.reg_ebx,
		tf->tf_regs.reg_edi,
		tf->tf_regs.reg_esi);
		return;	
	default:
		break;
	}
	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	cprintf("Incoming TRAP frame at %p\n", tf);

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		assert(curenv);

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// Return to the current environment, which should be running.
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

