/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/* Stub definitions to satisfy your linker when you're not using purify. */
/* When you are using purify these calls are intercepted. */
/* Thanks to Neil Hunt for this idea. */
/* compile with 'cc -c -O purify_stubs.c' */
/* archive with 'ar cr purify_stubs.a stub.o; ranlib purify_stubs.a' */

#if defined(__STDC__)
#define P(args) args
#else
#define P(args) ()
#define const
#endif

unsigned long purify_report_address;
unsigned long purify_report_number;
unsigned long purify_report_type;
unsigned long purify_report_result;

int purify_newallocated P(())			{ return 0; }
int purify_allallocated P(())			{ return 0; }
int purify_clear_newallocated P(())		{ return 0; }
int purify_newleaks P(())			{ return 0; }
int purify_allleaks P(())			{ return 0; }
int purify_clear_newleaks P(())			{ return 0; }

int purify_new_inuse P(())			{ return 0; }
int purify_all_inuse P(())			{ return 0; }
int purify_clear_inuse P(())			{ return 0; }
int purify_new_leaks P(())			{ return 0; }
int purify_all_leaks P(())			{ return 0; }
int purify_clear_leaks P(())			{ return 0; }

int purify_new_reports P(())			{ return 0; }
int purify_all_reports P(())			{ return 0; }
int purify_clear_new_reports P(())		{ return 0; }
int purify_start_batch P(())			{ return 0; }
int purify_start_batch_show_first P(())		{ return 0; }
int purify_stop_batch P(())			{ return 0; }
int purify_print P((void *addr))		{ return 0; }
int purify_describe P((void *addr))		{ return 0; }
int purify_what_colors P((void *addr, int len)) { return 0; }
int purify_watch P((const char *addr))		{ return 0; }
int purify_watch_1 P((const char *addr))	{ return 0; }
int purify_watch_2 P((const char *addr))	{ return 0; }
int purify_watch_4 P((const char *addr))	{ return 0; }
int purify_watch_8 P((const char *addr))	{ return 0; }
int purify_watch_w_1 P((const char *addr))	{ return 0; }
int purify_watch_w_2 P((const char *addr))	{ return 0; }
int purify_watch_w_4 P((const char *addr))	{ return 0; }
int purify_watch_w_8 P((const char *addr))	{ return 0; }
int purify_watch_rw_1 P((const char *addr))	{ return 0; }
int purify_watch_rw_2 P((const char *addr))	{ return 0; }
int purify_watch_rw_4 P((const char *addr))	{ return 0; }
int purify_watch_rw_8 P((const char *addr))	{ return 0; }
int purify_watch_r_1 P((const char *addr))	{ return 0; }
int purify_watch_r_2 P((const char *addr))	{ return 0; }
int purify_watch_r_4 P((const char *addr))	{ return 0; }
int purify_watch_r_8 P((const char *addr))	{ return 0; }
int purify_watch_ast P((const char *addr, int size, int type)) { return 0; }
int purify_watch_n P((const char *addr, int size, int type)) { return 0; }
int purify_watch_info P(())			{ return 0; }
int purify_watch_remove P((int watchno))	{ return 0; }
int purify_watch_remove_all P(( ))		{ return 0; }

int purify_printf() 				{ return 0; }
int purify_printf_with_call_chain()		{ return 0; }
int purify_logfile_printf() 			{ return 0; }

/* void *purify_call_original P((...))		{ return 0; } */

int
purify_process_file (in, out)
  char *in, *out;
{
    strcpy(out, in);
    return 0; /* Purify not running */
}

void
purify_note_loaded_file P((char* filename, unsigned int where_loaded)) { }

int purify_is_running() { return 0; } /* version in interface.c return TRUE */

extern char *memcpy();
char *
purify_unsafe_memcpy(dst, src, n)
  char *dst;
  char *src;
  int n;
{
    return memcpy(dst, src, n);
}

int
purify_assert_is_readable(src, n)
  const char *src;
  int n;
{
    return 1;
}

int
purify_assert_is_writable(dst, n)
  const char *dst;
  int n;
{
    return 1;
}

int
_p_is_readable(src, n)
  char *src;
  int n;
{
    return 1;
}

int
_p_is_writeable(src, n)
  char *src;
  int n;
{
    return 1;
}

int
_p_is_initialized(src, n)
  char *src;
  int n;
{
    return 1;
}

/*
 * Pool Allocator functionality
 * ============================
 */

void
purify_set_pool_id(mem, id)
  char *mem;
  int id;
{
}

int
purify_get_pool_id(mem)
  char *mem;
{
    return 0;
}

void
purify_set_user_data(mem, data)
  char *mem;
  void *data;
{
}

void *
purify_get_user_data(mem)
  char *mem;
{
    return 0;
}

void
purify_map_pool(id, fn)
  int id;
  void (*fn)();
{
}

void
purify_map_pool_id(fn)
  void (*fn)();
{
}

char *
purify_start_of_block(mem)
  char *mem;
{
    return (char *)0;
}

int
purify_size_of_block(mem)
  char *mem;
{
    return 0;
}

/*
 * Raw malloc interface.
 * ====================
 */

int
purify_add_malloc_marker_size(size)
  int size;
{
    return ((size + 7) & ~7);
}

int
purify_malloc_header_size(size)
  int size;
{
    return 0;
}

extern char *malloc();
char *
purify_malloc(size)
  int size;
{
    return malloc(size);
}

extern int free();
int
purify_free(p)
  char *p;
{
    return free(p);
}

extern char *realloc();
char *
purify_realloc(p, size)
  char *p;
  int size;
{
    return realloc(p, size);
}

extern char *calloc();
char *
purify_calloc(nelem, size)
  int nelem;
  int size;
{
    return calloc(nelem, size);
}

#if !defined(HPUX8)

extern int cfree();
int
purify_cfree(ptr)
  char *ptr;
{
    return cfree(ptr);
}

extern char *memalign();
char*
purify_memalign(alignment, len)
  int alignment;
  int len;
{
    return memalign(alignment, len);
}

extern char *valloc();
char*
purify_valloc(size)
  unsigned size;
{
    return valloc(size);
}

#endif /* !HPUX8 */

extern int brk();
int
purify_brk(limit)
  char *limit;
{
    return brk(limit);
}

extern char *sbrk();
char *
purify_sbrk(incr)
  int incr;
{
    return sbrk(incr);
}

char *
purify_mark_as_malloc(block, user_size, skip_stack_frames)
  char *block;
  int user_size;
  int skip_stack_frames;
{
    return block;
}

char *
purify_mark_as_free(user_block, skip_stack_frames)
  char *user_block;
  int skip_stack_frames;
{
    return user_block;
}

int
purify_clear_malloc_markers(block, size)
  char *block;
  int size;
{
    return 0;
}

void
purify_list_malloc_blocks(mem, size)
  char *mem;
  int size;
{
}

void
_p_red(addr, len)
  void *addr;
  int len;
{
}

void
_p_green(addr, len)
  void *addr;
  int len;
{
}

void
_p_yellow(addr, len)
  void *addr;
  int len;
{
}

void
_p_blue(addr, len)
  void *addr;
  int len;
{
}

void
_p_mark_as_initialized(addr, len)
  void *addr;
  int len;
{
}

void
_p_mark_as_uninitialized(addr, len)
  void *addr;
  int len;
{
}

void
_p_mark_for_trap(addr, len)
  void* addr;
  int len;
{
}

void
_p_mark_for_no_trap(addr, len)
  void* addr;
  int len;
{
}

int
_p_lift_watchpoints(marked, marked_len)
  char *marked;
  int marked_len;
{
    return 0;
}

void
_p_restore_watchpoints(marked, marked_len)
  char *marked;
  int marked_len;
{
}

void
purify_stop_here_internal()
{
}
