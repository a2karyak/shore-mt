/*<std-header orig-src='shore'>

 $Id: thread1.cpp,v 1.42 2001/11/30 01:28:26 bolo Exp $

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

#include <w_debug.h>
#include <w_stream.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <memory.h>

#include <w.h>
#include <w_statistics.h>
#include <sthread.h>
#include <sthread_stats.h>

#include <getopt.h>


#define DefaultNumThreads 2

#define DefaultPongTimes  10000
#define LocalMsgPongTimes 1000000
#define RemoteMsgPongTimes 100000


class worker_thread_t : public sthread_t {
public:
    worker_thread_t(int id);
protected:
    virtual void run();
private:
    int work_id;
};


class pong_thread_t;

struct ping_pong_t {
	smutex_t	theBall;
	int		whoseShot;
	scond_t		paddle[2];

	pong_thread_t	*ping;
	pong_thread_t	*pong;
	
	ping_pong_t() : whoseShot(0), ping(0), pong(0) { }
};


class wait_for_t {
	smutex_t	_lock;
	scond_t		_done;

	int	expected;
	int	have;

public:
	wait_for_t(int expecting) : expected(expecting), have(0) { }
	void	wait();
	void	done();
};


class pong_thread_t : public sthread_t {
public:
	pong_thread_t(ping_pong_t &game, int _id, wait_for_t &note);

protected:
	void run();

private:
	ping_pong_t	&game;
	int		id;
	wait_for_t	&note;
};


class timer_thread_t : public sthread_t {
public:
	timer_thread_t();

protected:
	void run();
};


class overflow_thread_t : public sthread_t {
public:
	overflow_thread_t() : sthread_t(t_regular, "overflow")
	{ }

protected:
	void run();
	void	recurse(unsigned overflow, char *sp0, char *last);

	enum	{ overflowFrameSize = 8192 };
};

class error_thread_t : public sthread_t {
public:
	error_thread_t() : sthread_t(t_regular, "error")
	{ }

protected:
	void run();
};


void msgPongFunc(void*);
void remotePongFunc(void*);


/* Configurable parameters */
int	NumThreads = DefaultNumThreads;
int	PongTimes = DefaultPongTimes;
int	SleepTime = 10000;		/* 10 seconds */
int	PongGames = 1;
int	StackOverflow = 0;	/* check stack overflow by allocatings
				   this much on the stack */
bool	ThreadExit = false;	/* exit main via thread package */
bool	DumpThreads = false;
bool	TestAssert = false;
bool	TestFatal = false;
bool	TestErrorInThread = false;
bool	verbose = false;
bool	WorkerThreadExit = true;	/* default is exit via longjmp */

worker_thread_t		**worker;
int			*ack; 


int	parse_args(int argc, char **argv)
{
	int	errors = 0;
	int	c;

	while ((c = getopt(argc, argv, "afn:p:s:g:o:xXdTv")) != EOF) {
		switch (c) {
		case 'n':
			NumThreads = atoi(optarg);
			break;
		case 'p':
			PongTimes = atoi(optarg);
			break;
		case 's':
			SleepTime = atoi(optarg);
			break;
		case 'g':
			PongGames = atoi(optarg);
			break;
		case 'o':
			StackOverflow = atoi(optarg);
			break;
		case 'x':
			ThreadExit = true;
			break;
		case 'X':
			WorkerThreadExit = false;
			break;
		case 'd':
			DumpThreads = true;
			break;
		case 'a':
			TestAssert = true;
			break;
		case 'f':
			TestFatal = true;
			break;
		case 'T':
			TestErrorInThread = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			errors++;
			break;
		}
	}
	if (errors) {
		cerr 
			<< "usage: "
			<< argv[0]
			<< " [-n threads]"
			<< " [-p pongs]"
			<< " [-g pong_games]"
			<< " [-s sleep_time]"
			<< " [-o overflow_alloc]"
			<< " [-a]"
			<< " [-f]"
			<< " [-T]"
			<< endl;
	}

	return errors ? -1 : optind;
}


void playPong()
{
	int	i;
	stime_t	startTime, endTime;
	ping_pong_t	*games;

	games = new ping_pong_t[PongGames];
	if (!games)
		W_FATAL(fcOUTOFMEMORY);
	
	wait_for_t	imdone(PongGames * 2);

	for (i = 0; i < PongGames; i++) {
		/* pongs wait for pings */
		games[i].pong = new pong_thread_t(games[i], 1, imdone);
		w_assert1(games[i].pong);

		games[i].ping = new pong_thread_t(games[i], 0, imdone);
		w_assert1(games[i].ping);

		W_COERCE(games[i].pong->fork());
		W_COERCE(games[i].ping->fork());
	}

	/* and this starts it all :-) */
	startTime = stime_t::now();
	imdone.wait();
	endTime = stime_t::now();

	cout << "idle_cnt: " << SthreadStats.idle << endl;
	cout << (sinterval_t)((endTime-startTime) / (PongGames*PongTimes))
		<< " per ping." << endl;

	for (i = 0; i < PongGames; i++) {
		W_COERCE(games[i].pong->wait());
		if (DumpThreads)
			cout << "Pong Thread Done:" << endl
				<< *games[i].pong << endl;
		delete games[i].pong;
		games[i].pong = 0;

		W_COERCE(games[i].ping->wait());
		if (DumpThreads)
			cout << "Ping Thread Done:" << endl
				<< *games[i].ping << endl;
		delete games[i].ping;
		games[i].ping = 0;
	}
	delete [] games;
}

void doErrorTests()
{
	if (TestAssert) {
		cout << endl << "** Generating an assertion failure." << endl;
		w_assert1(false);
	}

	if (TestFatal) {
		cout << endl << "** Generating a fatal error." << endl;
		W_FATAL(fcINTERNAL);
	}
}

int	main(int argc, char* argv[])
{
    int i;

    int next_arg = parse_args(argc, argv);
    if (next_arg < 0)
	    return 1;

    if (NumThreads) {
	    ack = new int[NumThreads];
	    if (!ack)
		    W_FATAL(fcOUTOFMEMORY);

	    worker = new worker_thread_t *[NumThreads];
	    if (!worker)
		    W_FATAL(fcOUTOFMEMORY);
    
	    /* print some stuff */
	    for(i=0; i<NumThreads; ++i) {
		    ack[i] = 0;
		    worker[i] = new worker_thread_t(i);
		    w_assert1(worker[i]);
		    W_COERCE(worker[i]->fork());
	    }

#if 0
	    if (DumpThreads)
		    sthread_t::dump("dump", cout);
#endif
    
	    for(i=0; i<NumThreads; ++i) {
		    W_COERCE( worker[i]->wait() );
		    w_assert1(ack[i]);
		    if (DumpThreads)
			    cout << "Thread Done:"
				    <<  endl << *worker[i] << endl;
		    delete worker[i];
		    worker[i] = 0;
	    }

	    delete [] worker;
	    delete [] ack;
    }

    if (PongTimes || PongGames)
	    playPong();

    if (SleepTime) {
	    timer_thread_t* timer_thread = new timer_thread_t;
	    w_assert1(timer_thread);
	    W_COERCE( timer_thread->fork() );
	    W_COERCE( timer_thread->wait() );
	    if (DumpThreads)
		    cout << "Thread Done:" << endl
			    << *timer_thread << endl;
	    delete timer_thread;
    }

    if (StackOverflow) {
	    overflow_thread_t *overflow = new overflow_thread_t;
	    w_assert1(overflow);
	    W_COERCE(overflow->fork());
	    W_COERCE(overflow->wait());
	    delete overflow;
    }

    if (TestAssert || TestFatal) {
	if (TestErrorInThread) {
		error_thread_t *error = new error_thread_t;
		if (!error)
		W_FATAL(fcOUTOFMEMORY);
		W_COERCE(error->fork());
		W_COERCE(error->wait());
		delete error;
	}
	else {
		cout << "Errors testing in main thread" << endl;
		doErrorTests();
	}
    }

    if (verbose)
    	sthread_t::dump_stats(cout);

    if (ThreadExit) 
	sthread_t::exit();

    return 0;
}

    

worker_thread_t::worker_thread_t(int id)
    : work_id(id)
{
    char b[40];
    ostrstream s(b, sizeof(b));
    s << "worker[" << id << "]" << ends;
    rename(b);
}

void worker_thread_t::run()
{
    cout << "Hello, world from " << work_id << endl;
    ack[work_id] = 1;

    /* test the return-through stack mechanism */
    if (WorkerThreadExit)
        sthread_t::end();
}


pong_thread_t::pong_thread_t(ping_pong_t &which_game,
			     int _id,
			     wait_for_t &notify_me)
: game(which_game), id(_id), note(notify_me)
{
	char	buf[128];
	ostrstream	s(buf, sizeof(buf));
	W_FORM2(s,("pong[%d]", id));
	s << ends;
	rename(s.str());
}


void pong_thread_t::run()
{
    int i;
    int	self = id;
	

    W_COERCE( sthread_t::me()->set_use_float(0) );
    W_COERCE( game.theBall.acquire() );
    for(i=0; i<PongTimes; ++i){
	while(game.whoseShot != self){
	    W_COERCE( game.paddle[self].wait(game.theBall) );
	}
	game.whoseShot = 1-self;
	game.paddle[1-self].signal();

    }
    game.theBall.release();

    // cout.form("pong(%#lx) id=%d done\n", (long)this, id);
    note.done();
}


timer_thread_t::timer_thread_t()
    : sthread_t(t_regular, "timer")
{
}


void timer_thread_t::run()
{
    W_FORM2(cout,("timeThread going to sleep for %d ms\n", SleepTime));
    sthread_t::sleep(SleepTime);
    cout << "timeThread awakened and die" << endl;
}


/* To do a decent job of stack overflow, it may be appropriate
   to recursively call run with a fixed size object, instead of
   using the gcc-specific dynamic array size hack. */

void overflow_thread_t::recurse(unsigned overflow, char *sp0, char *last)
{
	/* XXX This is supposed to be a huge stack allocation, don't
	   get rid of it */
	char	on_stack[overflowFrameSize];

	last = last;	/* create variable use so compilers don't complain */

	int	i;

	i = on_stack[0];	// save it from the optimizer

	/* make sure the context switch checks can catch it */
	for (i = 0; i < 10; i++)
		yield();

	ptrdiff_t	depth = (char*) sp0 - on_stack;
	if (depth < 0)
		depth = -depth;

	cout << "Recurse to " << depth << endl << flush;

	if ((unsigned)depth < overflow) {
		bool	ok = isStackFrameOK(overflowFrameSize);
		if (!ok)
			cout << "will_overflow says yeah!" << endl;

		recurse(overflow, sp0, on_stack);
	}
}


void overflow_thread_t::run()
{
#if 0 && defined(__GNUG__)
	/* XXX This is supposed to be a huge stack allocation, don't
	   get rid of it */
	char	on_stack[StackOverflow];
	int	i;

	i = on_stack[0];	// save it from the optimizer
	
	/* make sure the context switch checks can catch it */
	for (i = 0; i < 100; i++)
		yield();
#else
	char	sp0;
	recurse(StackOverflow, &sp0, &sp0);
#endif
}


void error_thread_t::run()
{
	cout << "Error Testing Thread Running" << endl;
	doErrorTests();
}


void	wait_for_t::wait()
{
	W_COERCE(_lock.acquire());
	while (have < expected)
		W_COERCE(_done.wait(_lock));
	_lock.release();
}

void	wait_for_t::done()
{
	W_COERCE(_lock.acquire());
	have++;
	if (have >= expected)
		_done.signal();
	_lock.release();
}


/*
  void msgPongFunc(void* arg)
  {
  int i;
  int ret;
  int me = (int)arg;
  int inBuf, outBuf;

  threadUsesFloatingPoint(0);
  outBuf = me;
  for(i=0; i<LocalMsgPongTimes; ++i){
  Tsend(&outBuf, sizeof(int), MyNodeNum, 1-me);
  inBuf = -1;
  ret = Trecv(&inBuf, sizeof(int), me);
  assert(ret == sizeof(int));
  assert(inBuf == 1-me);
  }
  }

  void remotePongFunc(void* ignoredArg)
  {
  int i;
  int ret;
  int me = MyNodeNum;
  int inBuf, outBuf;

  threadUsesFloatingPoint(0);
  outBuf = me;
  for(i=0; i<RemoteMsgPongTimes; ++i){
  Tsend(&outBuf, sizeof(int), 1-me, 2);
  inBuf = -1;
  ret = Trecv(&inBuf, sizeof(int), 2);
  assert(ret == sizeof(int));
  assert(inBuf == 1-me);
  }
  }

  */

