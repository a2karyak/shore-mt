/*<std-header orig-src='shore'>

 $Id: stcore.cpp,v 1.35 2001/06/20 03:22:58 bolo Exp $

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
 *   1998, 1999, 2000 by:
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

#ifdef Rs6000

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
#ifdef Rs6000
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

#ifdef Rs6000
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
	asm volatile("sw $17,4($sp);
                sw $18,8($sp);
                sw $19,12($sp);
                sw $20,16($sp);
                sw $21,20($sp);
                sw $22,24($sp);
                sw $23,28($sp);
                sw $fp,32($sp);
                sw $16,36($sp)");
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
	asm volatile("st.l r1, 4(sp);
                st.l r3, 8(sp);
                st.l r4, 12(sp);
                st.l r5, 16(sp);
                st.l r6, 20(sp);
                st.l r7, 24(sp);
                st.l r8, 28(sp);
                st.l r9, 32(sp);
                st.l r10, 36(sp);
                st.l r11, 40(sp);
                st.l r12, 44(sp);
                st.l r13, 48(sp);
                st.l r14, 52(sp);
                st.l r15, 56(sp)");
	if (old_core->use_float) {
		asm volatile("fst.d f2, 64(sp);
                  fst.d f4, 72(sp);
                  fst.d f6, 80(sp)");
	}
#endif /* I860 */

#ifdef I386
#ifdef _MSC_VER
        __asm {
		push ebp
		push ebx
		push edi
		push esi
		lea esp, dword ptr [esp-6Ch]
	}
#else
	asm volatile ("pushl %ebp
                 pushl %ebx
                 pushl %edi
                 pushl %esi
                 leal  -108(%esp), %esp");
#endif
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
	asm volatile ("
		ldo  %0(%%r30), %%r30" : : "i" (OURFRAME));

	/* store all the callee-save registers! */
	asm volatile ("
		ldo %0(%%r30), %%r31" : : "i" (-(MARKER+INTREGS)) : "r31");
	asm volatile ("
		stw %r3, 0(%r31);
		stw %r4, 4(%r31);
		stw %r5, 8(%r31);
		stw %r6, 0xc(%r31);
		stw %r7, 0x10(%r31);
		stw %r8, 0x14(%r31);
		stw %r9, 0x18(%r31);
		stw %r10, 0x1c(%r31);
		stw %r11, 0x20(%r31);
		stw %r12, 0x24(%r31);
		stw %r13, 0x28(%r31);
		stw %r14, 0x2c(%r31);
		stw %r15, 0x30(%r31);
		stw %r16, 0x34(%r31);
		stw %r17, 0x38(%r31);
		stw %r18, 0x3c(%r31);
	");

	if (old_core->use_float) {
		/* Shutdown fp operations */
		asm volatile ("
			ldo %0(%%r31),%%r31" : : "i" (-FPREGS));
		/*
		  Ack/ floating point load/store only has a 5 bit offset;
			we must move a pointer around to save / restore
			everything ( we could use a random register,
			but are not sure how to do that with gcc
		*/
		asm volatile ("
			fstds,ma %fr0, 8(%r31);
		");
		asm volatile ("
			fstds,ma %fr12, 8(%r31);
			fstds,ma %fr13, 8(%r31);
			fstds,ma %fr14, 8(%r31);
			fstds,ma %fr15, 8(%r31);
		");
#ifdef spectrum
		/* Later edition PA chips have more regs! */
		asm volatile ("
			fstds,ma %fr16, 8(%r31);
			fstds,ma %fr17, 8(%r31);
			fstds,ma %fr18, 8(%r31);
			fstds,ma %fr19, 8(%r31);
			fstds,ma %fr20, 8(%r31);
			fstds,ma %fr21, 8(%r31);
		");
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
	asm volatile ("
		t %0 ; 
		nop ;
		sub %%sp, %1, %%sp;
		mov %2, %%o7"
			: :	"i" (ST_FLUSH_WINDOWS), 
				"i" (SA(WINDOWSIZE)),
				"r" (new_core->save_sp));
#endif /* Sparc */

#ifdef Rs6000
	/* Make a new stack frame w/ space for save area + crud */
	asm volatile("stu 1, %0(1)" : :  "i" (-OURFRAME));

	/* r5 (caller saved) == &save area */
        asm volatile("ai 5, 1, %0" : : "i" (MINFRAME));

#if 1
	asm volatile("ai 6, 5, %0" : : "i" (INTREGS));
	asm volatile("stm 13, %0(6)" : : "i" (-INTREGS));
#else
	asm volatile("
		st 13, 0(5);
		st 14, 4(5);
		st 15, 8(5);
		st 16, 0xc(5);
		st 17, 0x10(5);
		st 18, 0x14(5);
		st 19, 0x18(5);
		st 20, 0x1c(5);
		st 21, 0x20(5);
		st 22, 0x24(5);
		st 23, 0x28(5);
		st 24, 0x2c(5);
		st 25, 0x30(5);
		st 26, 0x34(5);
		st 27, 0x38(5);
		st 28, 0x3c(5);
		st 29, 0x40(5);
		st 30, 0x44(5);
		st 31, 0x48(5);");
#endif

  	asm volatile("mfcr 12; st 12, %0(1)" : :
		     "i" (MINFRAME + INTREGS + FPREGS));

	if (old_core->use_float) {
		asm volatile("ai 5, 1, %0" : : "i" (MINFRAME + INTREGS));
		asm volatile("
			stfd 14, 0(5);
			stfd 15, 8(5);
			stfd 16, 0x10(5);
			stfd 17, 0x18(5);
			stfd 18, 0x20(5);
			stfd 19, 0x28(5);
			stfd 20, 0x30(5);
			stfd 21, 0x38(5);
			stfd 22, 0x40(5);
			stfd 23, 0x48(5);
			stfd 24, 0x50(5);
			stfd 25, 0x58(5);
			stfd 26, 0x60(5);
			stfd 27, 0x68(5);
			stfd 28, 0x70(5);
			stfd 29, 0x78(5);
			stfd 30, 0x80(5);
			stfd 31, 0x88(5);
			");
	}
#endif /* Rs6000 */
#ifdef Alpha
	asm volatile("lda $30, %0($30)" : : "i" (-OURFRAME));
	asm volatile("stq $9, 0($30);
		stq $10, 8($30);
		stq $11, 16($30);
		stq $12, 24($30);
		stq $13, 32($30);
		stq $14, 40($30);
		stq $15, 48($30);
		stq $26, 56($30);");

	if (old_core->use_float) {
		asm volatile("
			trapb;
			lda $28, 64($sp);
			stt $f2, 0($28);
			stt $f3, 8($28);
			stt $f4, 16($28);
			stt $f5, 24($28);
			stt $f6, 32($28);
			stt $f7, 40($28);
			stt $f8, 48($28);
			stt $f9, 56($28);
			trapb");
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
	asm volatile("movl %%esp, %0;
                movl %1, %%esp" 
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

#ifdef Alpha
	asm volatile("mb;
		addq $30,0, %0;" : "=r" (old_core->save_sp));
	asm volatile("mb;
		addq %0, 0, $30;" : : "r" (new_core->save_sp));
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
		asm volatile("
			andn %%o7, %0, %%o7;
			sub %%o7, %1, %%sp;
			mov %%o7, %%fp"
			: : 	"i" (STACK_ALIGN-1),
				"i" (SA(MINFRAME)));
#endif

#ifdef Hppa
		/* Align the stack pointer */
		asm volatile("
	              ldo %0(%%r30), %%r20;
	              ldi %1, %%r1;
		      and %%r1, %%r20, %%r20;
	              copy %%r20, %%r30"
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
		asm volatile ("
			stu 1, %0(1);
			oril 31,1, 0;"
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
	asm volatile("lw $17,4($sp);
                  lw $18,8($sp);
                  lw $19,12($sp);
                  lw $20,16($sp);
                  lw $21,20($sp);
                  lw $22,24($sp);
                  lw $23,28($sp);
                  lw $fp,32($sp);
                  lw $16,36($sp)");
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
	asm volatile("ld.l 4(sp), r1;
                ld.l 8(sp), r3;
                ld.l 12(sp), r4;
                ld.l 16(sp), r5;
                ld.l 20(sp), r6;
                ld.l 24(sp), r7;
                ld.l 28(sp), r8;
                ld.l 32(sp), r9;
                ld.l 36(sp), r10;
                ld.l 40(sp), r11;
                ld.l 44(sp), r12;
                ld.l 48(sp), r13;
                ld.l 52(sp), r14;
                ld.l 56(sp), r15");
	if (new_core->use_float) {
		asm volatile("fld.d 64(sp), f2;
                  fld.d 72(sp), f4;
                  fld.d 80(sp), f6");
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
	asm volatile("leal 108(%esp), %esp
                  popl %esi
                  popl %edi
                  popl %ebx
                  popl %ebp");
#endif
#endif /* I386 */

#ifdef Hppa
	asm volatile ("
		ldo %0(%%r30), %%r31" : : "i" (-(MARKER+INTREGS)));
	
	asm volatile ("
		ldw 0(%r31), %r3
		ldw 4(%r31), %r4
		ldw 8(%r31), %r5
		ldw 0xc(%r31), %r6
		ldw 0x10(%r31), %r7
		ldw 0x14(%r31), %r8
		ldw 0x18(%r31), %r9
		ldw 0x1c(%r31), %r10
		ldw 0x20(%r31), %r11
		ldw 0x24(%r31), %r12
		ldw 0x28(%r31), %r13
		ldw 0x2c(%r31), %r14
		ldw 0x30(%r31), %r15
		ldw 0x34(%r31), %r16
		ldw 0x38(%r31), %r17
		ldw 0x3c(%r31), %r18
	     ");
	
	if (new_core->use_float) {
		/* r31 already points at the correct place! */
#ifdef spectrum
		/* Later edition PA chips have more regs! */
		asm volatile ("
			fldds,mb -8(%r31), %fr21;
			fldds,mb -8(%r31), %fr20;
			fldds,mb -8(%r31), %fr19;
			fldds,mb -8(%r31), %fr18;
			fldds,mb -8(%r31), %fr17;
			fldds,mb -8(%r31), %fr16;
		");
#endif /* spectrum */
		asm volatile ("
			fldds,mb -8(%r31), %fr15;
			fldds,mb -8(%r31), %fr14;
			fldds,mb -8(%r31), %fr13;
			fldds,mb -8(%r31), %fr12;
		");
		/* and turn fp context back on again */
		asm volatile ("
			fldds,mb -8(%r31), %fr0;
		");
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
	asm volatile("
		ldd [%o7],%l0;
		ldd [%o7 +  0x8],%l2;
		ldd [%o7 + 0x10],%l4;
		ldd [%o7 + 0x18],%l6;

		ldd [%o7 + 0x20],%i0;
		ldd [%o7 + 0x28],%i2;
		ldd [%o7 + 0x30],%i4;
		ldd [%o7 + 0x38],%i6;
  	"); 
	
	/* The registers are all valid, so traps will not wipe out info.
	   NOW, we can set the new sp */
	asm volatile("nop; mov %o7, %sp; nop");
	
	/* The floating point registers are caller-save. */
#endif /* Sparc */

#ifdef Rs6000
	if (new_core->use_float) {
		asm volatile("ai 5, 1, %0" : : "i" (MINFRAME + INTREGS));
		asm volatile("
			lfd 14, 0(5);
			lfd 15, 8(5);
			lfd 16, 0x10(5);
			lfd 17, 0x18(5);
			lfd 18, 0x20(5);
			lfd 19, 0x28(5);
			lfd 20, 0x30(5);
			lfd 21, 0x38(5);
			lfd 22, 0x40(5);
			lfd 23, 0x48(5);
			lfd 24, 0x50(5);
			lfd 25, 0x58(5);
			lfd 26, 0x60(5);
			lfd 27, 0x68(5);
			lfd 28, 0x70(5);
			lfd 29, 0x78(5);
			lfd 30, 0x80(5);
			lfd 31, 0x88(5);
			");
	}
	/* r5 (caller saved) == &save area */
        asm volatile("ai 5, 1, %0" : : "i" (MINFRAME));

#if 1
	asm volatile("ai 6, 5, %0" : : "i" (INTREGS));
	asm volatile("lm 13, %0(6)" : : "i" (-INTREGS));
#else
	asm volatile("
		l 13, 0(5);
		l 14, 4(5);
		l 15, 8(5);
		l 16, 0xc(5);
		l 17, 0x10(5);
		l 18, 0x14(5);
		l 19, 0x18(5);
		l 20, 0x1c(5);
		l 21, 0x20(5);
		l 22, 0x24(5);
		l 23, 0x28(5);
		l 24, 0x2c(5);
		l 25, 0x30(5);
		l 26, 0x34(5);
		l 27, 0x38(5);
		l 28, 0x3c(5);
		l 29, 0x40(5);
		l 30, 0x44(5);
		l 31, 0x48(5);");
#endif

	/* restore only the ccrs that we need (2,3,4) */
  	asm volatile("l 12, %0(1); mtcrf %1,12" : :
		     "i" (MINFRAME + INTREGS + FPREGS),
		     "i" (CCMASK));

	/* restore the old stack pointer */
	asm volatile("l 1, 0(1)");
#endif /* Rs6000 */

#ifdef Alpha
	asm volatile("ldq $9, 0($30);
		ldq $10, 8($30);
		ldq $11, 16($30);
		ldq $12, 24($30);
		ldq $13, 32($30);
		ldq $14, 40($30);
		ldq $15, 48($30);
		ldq $26, 56($30);");

	if (new_core->use_float) {
		asm volatile("lda $28, 64($30);
			ldt $f2, 0($28);
			ldt $f3, 8($28);
			ldt $f4, 16($28);
			ldt $f5, 24($28);
			ldt $f6, 32($28);
			ldt $f7, 40($28);
			ldt $f8, 48($28);
			ldt $f9, 56($28);");
	}
	asm volatile("lda $30, %0($30)" : : "i" (OURFRAME));
#endif /* Alpha */

}

int stack_grows_up = StackGrowsUp;

