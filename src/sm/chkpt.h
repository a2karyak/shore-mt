/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: chkpt.h,v 1.15 1996/04/05 18:13:17 kupsch Exp $
 */
#ifndef CHKPT_H
#define CHKPT_H

#ifdef __GNUG__
#pragma interface
#endif

class chkpt_thread_t;

/*********************************************************************
 *
 *  class chkpt_m
 *
 *  Checkpoint Manager. User calls spawn_chkpt_thread() to fork
 *  a background thread to take checkpoint every now and then. 
 *  User calls take() to take a checkpoint immediately.
 *
 *  User calls wakeup_and_take() to wake up the checkpoint thread to checkpoint soon.
 *  (this is only for testing concurrent checkpoint/other-action.)
 *
 *********************************************************************/
class chkpt_m : public smlevel_3 {
public:
    NORET			chkpt_m();
    NORET			~chkpt_m();

    void 			take();
    void 			wakeup_and_take();
    void			spawn_chkpt_thread();
    void			retire_chkpt_thread();

private:
    chkpt_thread_t*		_chkpt_thread;

    // mutex for serializing prepares and checkpoints
    static smutex_t		_chkpt_mutex;

public:
    // These functions are for the use of chkpt -- to serialize
    // logging of chkpt and prepares 
    static void			chkpt_mutex_acquire();
    static void			chkpt_mutex_release();
};

#endif /*CHKPT_H*/
