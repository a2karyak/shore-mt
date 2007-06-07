/*<std-header orig-src='shore'>

 $Id: stcore.cpp,v 1.37 2007/03/30 20:42:28 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997,
 *   1998, 1999, 2000, 2001, 2002, 2003, 2004 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *	Dylan McNamee	<dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */

#include "stcore.h"

#if (__GNUG__ >= 2) && (__GNUC_MINOR__ >= 91)
/* HACK for egcs */
#define volatile
#endif

/* A hack for visual c++ build environment? */
#if defined(_M_IX86) && !defined(I386)
#define	I386
#endif

#ifdef Ipsc2

#define I386      1
#define IntelNX   1

#undef Ipsc860
#undef Decstation
#undef DecstationNet

#endif

#ifdef Ipsc860

#define I860      1
#define IntelNX   1

#undef Decstation
#undef DecstationNet

#endif

#ifdef I860
/* #define I860 */
#define	OneNode	1
#endif

#if defined(Mips) || defined(Decstation)
#ifndef Decstation	/* backwards compatibility */
#define Decstation 1
#endif
#ifndef Mips		/* backwards compatibility */
#define Mips      1
#endif
#define OneNode   1

#undef Ipsc2
#undef Ipsc860
#undef DecstationNet

#endif

#ifdef DecstationNet

#define Mips      1
#define PVM       1

#undef Ipsc2
#undef Ipsc860
#undef Decstation

#endif

#ifdef Snake

#define	Hppa	1
#define OneNode   1

#undef Ipsc2
#undef Ipsc860
#undef Decstation

/*
 * KLUDGE XXX
 *
 * There are multiple precision-architecture versions; so far,
 * they differ only in the number of floating point registers.
 * Unfortunately, I do not know of any way to tell them apart
 * at compilation time.
 *
 * Until I discover how to tell them apart at runtime, just
 * assume that all hppa machines are the newer ones
 */
#ifndef spectrum
#define	spectrum	/* pa risc ver. >1.0 */
#endif

#endif /* Snake */

#ifdef Sparc

#undef Ipsc2
#undef Ipsc860
#undef Decstation

#define	OneNode	1
#endif /* Sparc */

#if defined(Rs6000) || defined(PowerPC)

#undef Ipsc2
#undef Ipsc860
#undef Decstation
#undef Snake
#undef Sparc

#define	OneNode	1
#endif /* rs6000 */

#ifdef Mc68000
#define	OneNode	1
#undef Ipsc2
#undef Ipsc860
#undef Decstation
#undef Snake
#undef Sparc
#undef Rs6000

#define	Mc68020		/* undef this if you do not have 680x0 >= 68020 */

#endif /* mc68000 */
#ifdef Mips
#define StackGrowsUp 0
#endif
#ifdef I860
#define StackGrowsUp 0
#endif
#ifdef I386
#define StackGrowsUp 0
#endif
#ifdef Hppa
#define StackGrowsUp 1
#endif
#ifdef Sparc
#define	StackGrowsUp 0
#endif
#if defined(Rs6000) || defined(PowerPC)
#define	StackGrowsUp	0
#endif
#ifdef Mc68000
#define StackGrowsUp	0
#endif
#ifdef Alpha
#define	StackGrowsUp	0
#endif


#ifdef Sparc
#ifdef SOLARIS2
#include <sys/asm_linkage.h>
#include <sys/trap.h>
#else
#include <sparc/asm_linkage.h>
#include <sparc/trap.h>
#endif
#endif /* Sparc */

#ifdef Hppa
#define	MARKER		32		/* frame marker */
#define	INTREGS		(sizeof(int) * 16)	/* saved gen registers */
#define	NFPREGS(n)	(n + 1)		/* account for status register */
#ifdef spectrum
#define	FPREGS	(NFPREGS(10) * sizeof(double))	/* saved fp registers */
#else
#define	FPREGS	(NFPREGS(4) * sizeof(double))
#endif
#define	FIXEDARGS	(sizeof(int) * 4)	/* for calling functions */
#ifdef spectrum
#define	STACK_ALIGN	64		/* byte alignment to use for frame */
#else
#define	STACK_ALIGN	8
#endif
	/* alignment for powers-of-two */
#define	ALIGNTO(x,align)	(((x) + (align) - 1) & ~((align) - 1))

#define	WENEED	(MARKER + INTREGS + FPREGS)
#define	OURFRAME	ALIGNTO(WENEED, STACK_ALIGN)

#define       MINFRAME        ALIGNTO((MARKER+FIXEDARGS), STACK_ALIGN) 
#endif /* Hppa */

#if defined(Rs6000) || defined(PowerPC)
/* The minimum stack frame we can create is 24 bytes + 8 args */
#define	MINFRAME	(24 + 8*32)
/*
 * Our stack frame has room for 19 regs, 18 fpregs, 1 cc
 *
 * The condition code registers and the link register COULD
 * be saved in their dedicated save area, but I will make room
 * for them in my save area for now.
 */
#define	INTREGS	(sizeof(int) * 19)
#define	FPREGS	(sizeof(double) * 18)
#define	CCREGS	(sizeof(int))
#define	CCMASK	(0x38)	/* only need to restore cr2, cr3, cr4 */
#define	WENEED	(MINFRAME + INTREGS + FPREGS + CCREGS)
#define	STACK_ALIGN	8
#define	OURFRAME	((WENEED + STACK_ALIGN-1) & ~(STACK_ALIGN-1))	

/* Select if store/load multiple instructions are used */
#ifndef PowerPC
#define	POWER_USE_MULTIPLE
#endif
#endif /* Rs6000 */

#ifdef Alpha
/* The trap and memory barriers in the context switcher
   assure that floating point and memory operations are synchronized...
   This guarantees that exceptions are delivered to the thread
   which created them.  They other also allow the context
   switcher to correctly switch context between multiple processors.
 */
/* regs r9 ... r15 and ra */
#define	INTREGS	(sizeof(long) * 8)
/* f2 ... f9 */
#define	FPREGS	(sizeof(double) * 8)
#define	WENEED	(INTREGS + FPREGS)
#define	STACK_ALIGN	64
#define	OURFRAME	((WENEED + STACK_ALIGN-1) & ~(STACK_ALIGN-1))	
#endif /* Alpha */


/*
 *  sthread_core_switch()
 *
 *  Hand off the CPU to another thread
 *
 *  do not mess with this code unless you know what you are doing!
 */

void sthread_core_switch(sthread_core_t* old, sthread_core_t* _new)
{
	static sthread_core_t *old_core;
	static sthread_core_t *new_core;

	old_core = old, new_core = _new;

  /* save registers */
#ifdef Mips
	asm volatile("subu  $sp, 128");
	asm volatile("sw $17,4($sp);");
                asm volatile("sw $18,8($sp);");
                asm volatile("sw $19,12($sp);");
                asm volatile("sw $20,16($sp);");
                asm volatile("sw $21,20($sp);");
                asm volatile("sw $22,24($sp);");
                asm volatile("sw $23,28($sp);");
                asm volatile("sw $fp,32($sp);");
                asm volatile("sw $16,36($sp)");
	if (old_core->use_float) {
		asm volatile("s.d $f20, 44($sp)");
		asm volatile("s.d $f22, 52($sp)");
		asm volatile("s.d $f24, 60($sp)");
		asm volatile("s.d $f26, 68($sp)");
		asm volatile("s.d $f28, 76($sp)");
		asm volatile("s.d $f30, 84($sp)");
		asm volatile("s.d $f4,  92($sp)");
	}
#endif  /* Mips */

#ifdef I860
	asm volatile("addu -128,sp,sp");	/* save space on stack */
	/* offsets left over from non-interrupt safe version */
	asm volatile("st.l r1, 4(sp);");
                asm volatile("st.l r3, 8(sp);");
                asm volatile("st.l r4, 12(sp);");
                asm volatile("st.l r5, 16(sp);");
                asm volatile("st.l r6, 20(sp);");
                asm volatile("st.l r7, 24(sp);");
                asm volatile("st.l r8, 28(sp);");
                asm volatile("st.l r9, 32(sp);");
                asm volatile("st.l r10, 36(sp);");
                asm volatile("st.l r11, 40(sp);");
                asm volatile("st.l r12, 44(sp);");
                asm volatile("st.l r13, 48(sp);");
                asm volatile("st.l r14, 52(sp);");
                asm volatile("st.l r15, 56(sp)");
	if (old_core->use_float) {
		asm volatile("fst.d f2, 64(sp);");
                  asm volatile("fst.d f4, 72(sp);");
                  asm volatile("fst.d f6, 80(sp)");
	}
#endif /* I860 */

#ifdef I386
	asm volatile ("pushl %ebp");
                 asm volatile ("pushl %ebx");
                 asm volatile ("pushl %edi");
                 asm volatile ("pushl %esi");
                 asm volatile ("leal  -108(%esp), %esp");
	if (old_core->use_float) {
#ifdef _MSC_VER
		__asm {
			fnsave dword ptr [esp]
		}
#else
		asm volatile("fsave (%esp)");
#endif
	}
#endif /* I386 */

#ifdef Hppa
	/* build a new stackframe */
	asm volatile ("ldo  %0(%%r30), %%r30" : : "i" (OURFRAME));

	/* store all the callee-save registers! */
	asm volatile ("ldo %0(%%r30), %%r31" 
			: : "i" (-(MARKER+INTREGS)) : "r31");
	asm volatile ("stw %r3, 0(%r31);");
	asm volatile ("stw %r4, 4(%r31);");
	asm volatile ("stw %r5, 8(%r31);");
	asm volatile ("stw %r6, 0xc(%r31);");
	asm volatile ("stw %r7, 0x10(%r31);");
	asm volatile ("stw %r8, 0x14(%r31);");
	asm volatile ("stw %r9, 0x18(%r31);");
	asm volatile ("stw %r10, 0x1c(%r31);");
	asm volatile ("stw %r11, 0x20(%r31);");
	asm volatile ("stw %r12, 0x24(%r31);");
	asm volatile ("stw %r13, 0x28(%r31);");
	asm volatile ("stw %r14, 0x2c(%r31);");
	asm volatile ("stw %r15, 0x30(%r31);");
	asm volatile ("stw %r16, 0x34(%r31);");
	asm volatile ("stw %r17, 0x38(%r31);");
	asm volatile ("stw %r18, 0x3c(%r31);");

	if (old_core->use_float) {
		/* Shutdown fp operations */
		asm volatile ("ldo %0(%%r31),%%r31" : : "i" (-FPREGS));
		/*
		  Ack/ floating point load/store only has a 5 bit offset;
			we must move a pointer around to save / restore
			everything ( we could use a random register,
			but are not sure how to do that with gcc
		*/
		asm volatile ("fstds,ma %fr0, 8(%r31); ");
		asm volatile ("	fstds,ma %fr12, 8(%r31);");
		asm volatile ("	fstds,ma %fr13, 8(%r31);");
		asm volatile ("fstds,ma %fr14, 8(%r31);");
		asm volatile ("fstds,ma %fr15, 8(%r31);");
#ifdef spectrum
		/* Later edition PA chips have more regs! */
		asm volatile ("fstds,ma %fr16, 8(%r31);");
		asm volatile ("	fstds,ma %fr17, 8(%r31);");
		asm volatile ("	fstds,ma %fr18, 8(%r31);");
		asm volatile ("	fstds,ma %fr19, 8(%r31);");
		asm volatile ("	fstds,ma %fr20, 8(%r31);");
		asm volatile ("	fstds,ma %fr21, 8(%r31);");
#endif /* spectrum */
	}
#endif /* Hppa */

#ifdef Sparc
	/* most of the context is saved courtesy of the reg. windows */

	/*
	 * the floating point registers are caller-save, so we ignore them
	 * However, we want to terminate all fp. activity, so the trap
	 * does not occur in the wrong thread.  We do this by storing
	 * the floating point status register to memory (I use
	 * the arg passing area of the save area for this scratch)
	 */
	if (old_core->use_float) {
		asm volatile("st %%fsr, [%%sp + %0]" : : "i" (SA(WINDOWSIZE)));
	}

	/*
	 * on the sparcs, the current "register window save area", pointed
	 * to by the SP, can pretty much be over-written ANYTIME by
	 * traps, interrupts, etc
	 *
	 * When we start to restore the new thread context, if we setup
	 * SP immediately, the machine could wipe out any saved values
	 * before we have a chance to restore them.
	 * And, if we left it pointed at the old area, the activity
	 * would wipe out the context we had just saved.
	 *
	 * So,... we create a new register save area 
	 * on the old thread stack to use in the interim.
	 *
	 * (we use o7 because the compiler does not; a better solution
	 * would be to use a register variable)
	 */
	asm volatile (" mov %%sp, %0;" : "=r" (old_core->save_sp));
	
	/* Flush register windows to the thread stack */
	asm volatile ("	t %0 ;  ");
	asm volatile ("	nop ; ");
	asm volatile ("	sub %%sp, %1, %%sp; ");
	asm volatile ("	mov %2, %%o7"
			: :	"i" (ST_FLUSH_WINDOWS), 
				"i" (SA(WINDOWSIZE)),
				"r" (new_core->save_sp));
#endif /* Sparc */

#ifdef Rs6000
	/* Make a new stack frame w/ space for save area + crud */
	asm volatile("stu 1, %0(1)" : :  "i" (-OURFRAME));

	/* r5 (caller saved) == &save area */
        asm volatile("ai 5, 1, %0" : : "i" (MINFRAME));

#ifdef POWER_USE_MULTIPLE
	asm volatile("ai 6, 5, %0" : : "i" (INTREGS));
	asm volatile("stm 13, %0(6)" : : "i" (-INTREGS));
#else
	asm volatile(" ");
	asm volatile ("	st 13, 0(5); ");
	asm volatile ("	st 14, 4(5); ");
	asm volatile ("	st 15, 8(5); ");
	asm volatile ("	st 16, 0xc(5); ");
	asm volatile ("	st 17, 0x10(5); ");
	asm volatile ("	st 18, 0x14(5); ");
	asm volatile ("	st 19, 0x18(5); ");
	asm volatile ("	st 20, 0x1c(5); ");
	asm volatile ("	st 21, 0x20(5); ");
	asm volatile ("	st 22, 0x24(5); ");
	asm volatile ("	st 23, 0x28(5); ");
	asm volatile ("	st 24, 0x2c(5); ");
	asm volatile ("	st 25, 0x30(5); ");
	asm volatile ("	st 26, 0x34(5); ");
	asm volatile ("	st 27, 0x38(5); ");
	asm volatile ("	st 28, 0x3c(5); ");
	asm volatile ("	st 29, 0x40(5); ");
	asm volatile ("	st 30, 0x44(5); ");
	asm volatile ("	st 31, 0x48(5); ");
#endif

  	asm volatile("mfcr 12; st 12, %0(1)" : :
		     "i" (MINFRAME + INTREGS + FPREGS));

	if (old_core->use_float) {
		asm volatile("ai 5, 1, %0" : : "i" (MINFRAME + INTREGS));
		asm volatile(" ");
		asm volatile ("	stfd 14, 0(5); ");
		asm volatile ("	stfd 15, 8(5); ");
		asm volatile ("	stfd 16, 0x10(5); ");
		asm volatile ("	stfd 17, 0x18(5); ");
		asm volatile ("	stfd 18, 0x20(5); ");
		asm volatile ("	stfd 19, 0x28(5); ");
		asm volatile ("	stfd 20, 0x30(5); ");
		asm volatile ("	stfd 21, 0x38(5); ");
		asm volatile ("	stfd 22, 0x40(5); ");
		asm volatile ("	stfd 23, 0x48(5); ");
		asm volatile ("	stfd 24, 0x50(5); ");
		asm volatile ("	stfd 25, 0x58(5); ");
		asm volatile ("	stfd 26, 0x60(5); ");
		asm volatile ("	stfd 27, 0x68(5); ");
		asm volatile ("	stfd 28, 0x70(5); ");
		asm volatile ("	stfd 29, 0x78(5); ");
		asm volatile ("	stfd 30, 0x80(5); ");
		asm volatile ("	stfd 31, 0x88(5); ");
	}
#endif /* Rs6000 */

#ifdef PowerPC
	/* Make a new stack frame w/ space for save area + crud */
	asm volatile("stwu r1, %0(r1)" : :  "i" (-OURFRAME));

	/* r5 (caller saved) == &save area */
        asm volatile("addi r5, r1, %0" : : "i" (MINFRAME));

#ifdef POWER_USE_MULTIPLE
	asm volatile("ai 6, 5, %0" : : "i" (INTREGS));
	asm volatile("stm 13, %0(6)" : : "i" (-INTREGS));
#else
	asm volatile(" ");
	asm volatile ("	stw r13, 0(r5); ");
	asm volatile ("	stw r14, 4(r5); ");
		ssm volatile(" stw r15, 8(r5); ");
		ssm volatile(" stw r16, 0xc(r5); ");
		ssm volatile(" stw r17, 0x10(r5); ");
		ssm volatile(" stw r18, 0x14(r5); ");
		ssm volatile(" stw r19, 0x18(r5); ");
		ssm volatile(" stw r20, 0x1c(r5); ");
		ssm volatile(" stw r21, 0x20(r5); ");
		ssm volatile(" stw r22, 0x24(r5); ");
		ssm volatile(" stw r23, 0x28(r5); ");
		ssm volatile(" stw r24, 0x2c(r5); ");
		ssm volatile(" stw r25, 0x30(r5); ");
		ssm volatile(" stw r26, 0x34(r5); ");
		ssm volatile(" stw r27, 0x38(r5); ");
		ssm volatile(" stw r28, 0x3c(r5); ");
		ssm volatile(" stw r29, 0x40(r5); ");
		ssm volatile(" stw r30, 0x44(r5); ");
		ssm volatile(" stw r31, 0x48(r5);");
#endif
  	asm volatile("mfcr r12; st r12, %0(r1)" : :
		     "i" (MINFRAME + INTREGS + FPREGS));

	if (old_core->use_float) {
		asm volatile("addi r5, r1, %0" : : "i" (MINFRAME + INTREGS));
		asm volatile(" "); 
			asm volatile("stfd f14, 0(r5);");
			asm volatile("stfd f15, 8(r5);");
			asm volatile("stfd f16, 0x10(r5);");
			asm volatile("stfd f17, 0x18(r5);");
			asm volatile("stfd f18, 0x20(r5);");
			asm volatile("stfd f19, 0x28(r5);");
			asm volatile("stfd f20, 0x30(r5);");
			asm volatile("stfd f21, 0x38(r5);");
			asm volatile("stfd f22, 0x40(r5);");
			asm volatile("stfd f23, 0x48(r5);");
			asm volatile("stfd f24, 0x50(r5);");
			asm volatile("stfd f25, 0x58(r5);");
			asm volatile("stfd f26, 0x60(r5);");
			asm volatile("stfd f27, 0x68(r5);");
			asm volatile("stfd f28, 0x70(r5);");
			asm volatile("stfd f29, 0x78(r5);");
			asm volatile("stfd f30, 0x80(r5);");
			asm volatile("stfd f31, 0x88(r5);");
	}
#endif /* PowerPC */

#ifdef Alpha
	asm volatile("lda $30, %0($30)" : : "i" (-OURFRAME));
	asm volatile("stq $9, 0($30); ");
		asm volatile ("stq $10, 8($30); ");
		asm volatile ("stq $11, 16($30); ");
		asm volatile ("stq $12, 24($30); ");
		asm volatile ("stq $13, 32($30); ");
		asm volatile ("stq $14, 40($30); ");
		asm volatile ("stq $15, 48($30); ");
		asm volatile ("stq $26, 56($30);");

	if (old_core->use_float) {
		asm volatile(" trapb; ");
			asm volatile ("lda $28, 64($sp); ");
			asm volatile ("stt $f2, 0($28); ");
			asm volatile ("stt $f3, 8($28); ");
			asm volatile ("stt $f4, 16($28); ");
			asm volatile ("stt $f5, 24($28); ");
			asm volatile ("stt $f6, 32($28); ");
			asm volatile ("stt $f7, 40($28); ");
			asm volatile ("stt $f8, 48($28); ");
			asm volatile ("stt $f9, 56($28); ");
			asm volatile ("trapb");
	}
#endif /* Alpha */

	/* switch stack-pointers */
#ifdef Mips
	asm volatile("add %0, $sp, $0"
		     : "=r" (old_core->save_sp));
	asm volatile("add $sp, %0, $0" 
		     : : "r" (new_core->save_sp));
#endif

#ifdef I860
	asm volatile("addu sp, r0, %0"
		     : "=r" (old_core->save_sp));
	asm volatile("addu r0, %0, sp" 
		     : : "r" (new_core->save_sp));
#endif

#ifdef I386
#ifdef _MSC_VER
	__asm {
		mov ebx, (old_core)
		mov [ebx].save_sp, esp
		mov ebx, (new_core)
		mov esp, [ebx].save_sp
	}
#else
	asm volatile ("movl %%esp, %0; movl %1, %%esp" 
			: "=&r" (old_core->save_sp)
			: "r" (new_core->save_sp));
#endif
#endif

#ifdef Hppa
	asm volatile("copy  %%r30, %0;" : "=r" (old_core->save_sp));
	asm volatile("copy  %0, %%r30;" : : "r" (new_core->save_sp));
#endif

#ifdef Sparc
  /* done above */
#endif 

#ifdef Rs6000
	asm volatile("oril %0, 1, 0;" : "=r" (old_core->save_sp));
	asm volatile("oril 1, %0, 0;" : : "r" (new_core->save_sp));
#endif

#ifdef PowerPC
	asm volatile("mr %0, r1" : "=r" (old_core->save_sp));
	asm volatile("mr r1, %0" : : "r" (new_core->save_sp));
#endif


#ifdef Alpha
	asm volatile("mb; addq $30,0, %0;" : "=r" (old_core->save_sp));
	asm volatile("mb; addq %0, 0, $30;" : : "r" (new_core->save_sp));
#endif /* Alpha */

	if (new_core->is_virgin) {
		/* first time --- call procedure directly */
		new_core->is_virgin = 0;
#ifdef Mips
		/* create a "mips" stackframe (room for arg regs) */
		asm volatile("subu $sp, 32");
#endif

#ifdef I386
		/* try to "end" the chain of stack frames for debuggers */
#ifdef _MSC_VER
		_asm {
			mov ebp, esp
		}
#else
		asm volatile("mov %esp, %ebp");
#endif
#endif

#ifdef I860
		/* align the stack pointer to multiple of 16 */
		asm volatile("andnot 15,sp,sp");
		/* fake frame */
		asm volatile("mov sp,fp");
		asm volatile("addu -128,sp,sp");
		/* new frame pointer */
		asm volatile("st.l fp, 0(fp)");
		asm volatile("addu 0,sp,fp");
	
#endif

#ifdef Sparc
		/*
		 * Ok, so we do not restore anything -- just setup the SP,
		 * which needs to have a register save  + args area!
		 * Also, set the FP for the new stack
		 * The 'nand' is to guarantee the stack pointer is 8 aligned!
		 */
		asm volatile("andn %%o7, %0, %%o7;");
			asm volatile("sub %%o7, %1, %%sp;");
			asm volatile("mov %%o7, %%fp"
			: : 	"i" (STACK_ALIGN-1),
				"i" (SA(MINFRAME)));
#endif

#ifdef Hppa
		/* Align the stack pointer */
	      asm volatile("ldo %0(%%r30), %%r20;");
	      asm volatile("ldi %1, %%r1;");
	      asm volatile("and %%r1, %%r20, %%r20;");
	      asm volatile("copy %%r20, %%r30"
	                     : : "i" (STACK_ALIGN-1), "i" (-STACK_ALIGN)
			     : "r20", "r1");
		/* provide a frame marker */
		asm volatile("ldo %0(%%r30), %%r30"
			     : : "i" (MINFRAME));
		/* and no previous frame */
		asm volatile("stw %r0, -4(%r30)");
#endif /* Hppa */

#ifdef Rs6000
		/*
		 * Create the "standard" stack frame, and setup sp and ap
		 * for it
		 */
		asm volatile ("stu 1, %0(1);");
		asm volatile ("oril 31,1, 0;"
			      : : "i" (-MINFRAME) );
#endif
#ifdef Alpha
		/* align the stack pointer */
		asm volatile("bic $30, %0, $30" : : "i" (STACK_ALIGN-1));
		/* new frame pointer */
		asm volatile("bis $30, $30, $15");
#endif /* Alpha */

		(new_core->start_proc)(new_core->start_arg);
		/* this should never be reached */
		sthread_core_fatal();
	}

	/* restore the registers of the new thread */
#ifdef Mips
	asm volatile("lw $17,4($sp);");
                  asm volatile ("lw $18,8($sp);");
                  asm volatile ("lw $19,12($sp);");
                  asm volatile ("lw $20,16($sp);");
                  asm volatile ("lw $21,20($sp);");
                  asm volatile ("lw $22,24($sp);");
                  asm volatile ("lw $23,28($sp);");
                  asm volatile ("lw $fp,32($sp);");
                  asm volatile ("lw $16,36($sp)");
	if (new_core->use_float) {
		asm volatile("l.d $f20, 44($sp)");
		asm volatile("l.d $f22, 52($sp)");
		asm volatile("l.d $f24, 60($sp)");
		asm volatile("l.d $f26, 68($sp)");
		asm volatile("l.d $f28, 76($sp)");
		asm volatile("l.d $f30, 84($sp)");
		asm volatile("l.d $f4, 92($sp)");
	}
	asm volatile("addu $sp, 128");
#endif /* Mips */

#ifdef I860
	asm volatile("ld.l 4(sp), r1;");
                asm volatile ("ld.l 8(sp), r3;");
                asm volatile ("ld.l 12(sp), r4;");
                asm volatile ("ld.l 16(sp), r5;");
                asm volatile ("ld.l 20(sp), r6;");
                asm volatile ("ld.l 24(sp), r7;");
                asm volatile ("ld.l 28(sp), r8;");
                asm volatile ("ld.l 32(sp), r9;");
                asm volatile ("ld.l 36(sp), r10;");
                asm volatile ("ld.l 40(sp), r11;");
                asm volatile ("ld.l 44(sp), r12;");
                asm volatile ("ld.l 48(sp), r13;");
                asm volatile ("ld.l 52(sp), r14;");
                asm volatile ("ld.l 56(sp), r15");
	if (new_core->use_float) {
		asm volatile("fld.d 64(sp), f2;");
                  asm volatile ("fld.d 72(sp), f4;");
                  asm volatile ("fld.d 80(sp), f6");
	}
	asm volatile("addu 128,sp,sp");	/* restore stack */
#endif /* I860 */

#ifdef I386
	if (new_core->use_float) {
#ifdef _MSC_VER
		__asm {
			frstor     dword ptr [esp]
		}
#else
		asm volatile ("frstor (%esp)");
#endif
	}
#ifdef _MSC_VER
	__asm {
		lea         esp,dword ptr [esp+6Ch]
		pop         esi
		pop         edi			
		pop         ebx
		pop         ebp
	}
#else
	  asm volatile("leal 108(%esp), %esp");
	  asm volatile ("popl %esi");
	  asm volatile ("popl %edi");
	  asm volatile ("popl %ebx");
	  asm volatile ("popl %ebp");
#endif
#endif /* I386 */

#ifdef Hppa
	asm volatile ("ldo %0(%%r30), %%r31" : : "i" (-(MARKER+INTREGS)));
	
	asm volatile ("ldw 0(%r31), %r3");
	asm volatile ("ldw 4(%r31), %r4");
	asm volatile ("ldw 8(%r31), %r5");
	asm volatile ("ldw 0xc(%r31), %r6");
	asm volatile ("ldw 0x10(%r31), %r7");
	asm volatile ("ldw 0x14(%r31), %r8");
	asm volatile ("ldw 0x18(%r31), %r9");
	asm volatile ("ldw 0x1c(%r31), %r10");
	asm volatile ("ldw 0x20(%r31), %r11");
	asm volatile ("ldw 0x24(%r31), %r12");
	asm volatile ("ldw 0x28(%r31), %r13");
	asm volatile ("ldw 0x2c(%r31), %r14");
	asm volatile ("ldw 0x30(%r31), %r15");
	asm volatile ("ldw 0x34(%r31), %r16");
	asm volatile ("ldw 0x38(%r31), %r17");
	asm volatile ("ldw 0x3c(%r31), %r18 ");
	
	if (new_core->use_float) {
		/* r31 already points at the correct place! */
#ifdef spectrum
		/* Later edition PA chips have more regs! */
		asm volatile ("fldds,mb -8(%r31), %fr21;");
		asm volatile ("fldds,mb -8(%r31), %fr20;");
		asm volatile ("fldds,mb -8(%r31), %fr19;");
		asm volatile ("fldds,mb -8(%r31), %fr18;");
		asm volatile ("fldds,mb -8(%r31), %fr17;");
		asm volatile ("fldds,mb -8(%r31), %fr16;");
#endif /* spectrum */
		asm volatile ("fldds,mb -8(%r31), %fr15;");
		asm volatile ("fldds,mb -8(%r31), %fr14;");
		asm volatile ("fldds,mb -8(%r31), %fr13;");
		asm volatile ("fldds,mb -8(%r31), %fr12;");
		/* and turn fp context back on again */
		asm volatile ("fldds,mb -8(%r31), %fr0;");
	}
	/* restore to the old stack marker */
	asm volatile ("ldo %0(%%r30), %%r30" : : "i" (-OURFRAME));
	
#endif /* Hppa */
#ifdef Sparc
	/*
	 * Ok, %o7 == &register save area, %sp==old threads save area
	 *
	 * Now, restore all registers (except the SP)
	 * from the new thread save area
	 */
	asm volatile("ldd [%o7],%l0;");
	asm volatile("ldd [%o7 +  0x8],%l2;");
	asm volatile("ldd [%o7 + 0x10],%l4;");
	asm volatile("ldd [%o7 + 0x18],%l6;");

	asm volatile("ldd [%o7 + 0x20],%i0;");
	asm volatile("ldd [%o7 + 0x28],%i2;");
	asm volatile("ldd [%o7 + 0x30],%i4;");
	asm volatile("ldd [%o7 + 0x38],%i6;");

	/* The registers are all valid, so traps will not wipe out info.
	   NOW, we can set the new sp */
	asm volatile("nop; mov %o7, %sp; nop");
	
	/* The floating point registers are caller-save. */
#endif /* Sparc */

#ifdef Rs6000
	if (new_core->use_float) {
		asm volatile("ai 5, 1, %0" : : "i" (MINFRAME + INTREGS));
		asm volatile("lfd 14, 0(5);");
		asm volatile ("lfd 15, 8(5);");
		asm volatile ("lfd 16, 0x10(5);");
		asm volatile ("lfd 17, 0x18(5);");
		asm volatile ("lfd 18, 0x20(5);");
		asm volatile ("lfd 19, 0x28(5);");
		asm volatile ("lfd 20, 0x30(5);");
		asm volatile ("lfd 21, 0x38(5);");
		asm volatile ("lfd 22, 0x40(5);");
		asm volatile ("lfd 23, 0x48(5);");
		asm volatile ("lfd 24, 0x50(5);");
		asm volatile ("lfd 25, 0x58(5);");
		asm volatile ("lfd 26, 0x60(5);");
		asm volatile ("lfd 27, 0x68(5);");
		asm volatile ("lfd 28, 0x70(5);");
		asm volatile ("lfd 29, 0x78(5);");
		asm volatile ("lfd 30, 0x80(5);");
		asm volatile ("lfd 31, 0x88(5);");
	}
	/* r5 (caller saved) == &save area */
        asm volatile("ai 5, 1, %0" : : "i" (MINFRAME));

#ifdef POWER_USE_MULTIPLE
	asm volatile("ai 6, 5, %0" : : "i" (INTREGS));
	asm volatile("lm 13, %0(6)" : : "i" (-INTREGS));
#else
	asm volatile("l 13, 0(5);");
	asm volatile("l 14, 4(5);");
	asm volatile("l 15, 8(5);");
	asm volatile("l 16, 0xc(5);");
	asm volatile("l 17, 0x10(5);");
	asm volatile("l 18, 0x14(5);");
	asm volatile("l 19, 0x18(5);");
	asm volatile("l 20, 0x1c(5);");
	asm volatile("l 21, 0x20(5);");
	asm volatile("l 22, 0x24(5);");
	asm volatile("l 23, 0x28(5);");
	asm volatile("l 24, 0x2c(5);");
	asm volatile("l 25, 0x30(5);");
	asm volatile("l 26, 0x34(5);");
	asm volatile("l 27, 0x38(5);");
	asm volatile("l 28, 0x3c(5);");
	asm volatile("l 29, 0x40(5);");
	asm volatile("l 30, 0x44(5);");
	asm volatile("l 31, 0x48(5);");
#endif

	/* restore only the ccrs that we need (2,3,4) */
  	asm volatile("l 12, %0(1); mtcrf %1,12" : :
		     "i" (MINFRAME + INTREGS + FPREGS),
		     "i" (CCMASK));

	/* restore the old stack pointer */
	asm volatile("l 1, 0(1)");
#endif /* Rs6000 */

#ifdef PowerPC
	if (new_core->use_float) {
		asm volatile("addi r5, r1, %0" : : "i" (MINFRAME + INTREGS));
		asm volatile("lfd f14, 0(r5);");
		asm volatile("lfd f15, 8(r5);");
		asm volatile("lfd f16, 0x10(r5);");
		asm volatile("lfd f17, 0x18(r5);");
		asm volatile("lfd f18, 0x20(r5);");
		asm volatile("lfd f19, 0x28(r5);");
		asm volatile("lfd f20, 0x30(r5);");
		asm volatile("lfd f21, 0x38(r5);");
		asm volatile("lfd f22, 0x40(r5);");
		asm volatile("lfd f23, 0x48(r5);");
		asm volatile("lfd f24, 0x50(r5);");
		asm volatile("lfd f25, 0x58(r5);");
		asm volatile("lfd f26, 0x60(r5);");
		asm volatile("lfd f27, 0x68(r5);");
		asm volatile("lfd f28, 0x70(r5);");
		asm volatile("lfd f29, 0x78(r5);");
		asm volatile("lfd f30, 0x80(r5);");
		asm volatile("lfd f31, 0x88(r5);");
}

	/* r5 (caller saved) == &save area */
        asm volatile("addi r5, r1, %0" : : "i" (MINFRAME));

#ifdef POWER_USE_MULTIPLE
	asm volatile("addi r6, r5, %0" : : "i" (INTREGS));
	asm volatile("lm r13, %0(r6)" : : "i" (-INTREGS));
#else
	asm volatile("lwz r13, 0(r5);");
		asm volatile("lwz r14, 4(r5);");
		asm volatile("lwz r15, 8(r5);");
		asm volatile("lwz r16, 0xc(r5);");
		asm volatile("lwz r17, 0x10(r5);");
		asm volatile("lwz r18, 0x14(r5);");
		asm volatile("lwz r19, 0x18(r5);");
		asm volatile("lwz r20, 0x1c(r5);");
		asm volatile("lwz r21, 0x20(r5);");
		asm volatile("lwz r22, 0x24(r5);");
		asm volatile("lwz r23, 0x28(r5);");
		asm volatile("lwz r24, 0x2c(r5);");
		asm volatile("lwz r25, 0x30(r5);");
		asm volatile("lwz r26, 0x34(r5);");
		asm volatile("lwz r27, 0x38(r5);");
		asm volatile("lwz r28, 0x3c(r5);");
		asm volatile("lwz r29, 0x40(r5);");
		asm volatile("lwz r30, 0x44(r5);");
		asm volatile("lwz r31, 0x48(r5);");
#endif

	/* restore only the ccrs that we need (2,3,4) */
  	asm volatile("lwz r12, %0(r1); mtcrf %1,r12" : :
		     "i" (MINFRAME + INTREGS + FPREGS),
		     "i" (CCMASK));

	/* restore the old stack pointer */
	asm volatile("lwz r1, 0(r1)");
#endif

#ifdef Alpha
	asm volatile("ldq $9, 0($30);");
		asm volatile("ldq $10, 8($30);");
		asm volatile("ldq $11, 16($30);");
		asm volatile("ldq $12, 24($30);");
		asm volatile("ldq $13, 32($30);");
		asm volatile("ldq $14, 40($30);");
		asm volatile("ldq $15, 48($30);");
		asm volatile("ldq $26, 56($30);");

	if (new_core->use_float) {
		asm volatile("lda $28, 64($30);");
			asm volatile("ldt $f2, 0($28);");
			asm volatile("ldt $f3, 8($28);");
			asm volatile("ldt $f4, 16($28);");
			asm volatile("ldt $f5, 24($28);");
			asm volatile("ldt $f6, 32($28);");
			asm volatile("ldt $f7, 40($28);");
			asm volatile("ldt $f8, 48($28);");
			asm volatile("ldt $f9, 56($28);");
	}
	asm volatile("lda $30, %0($30)" : : "i" (OURFRAME));
#endif /* Alpha */

}

int stack_grows_up = StackGrowsUp;

