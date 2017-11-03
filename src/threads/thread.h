#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <hash.h>
#include <list.h>
#include <stdint.h>
#include <threads/synch.h>
/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
truct child_thread
 71 {
  72   tid_t tid;
   73   struct thread* thr;
    74   struct thread* parent;
     75   struct list_elem ch_elem;
      76
       77 }
       (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int64_t allow_wakeuptick;           /* For sleep : Tick that allows wakeup */
    int org_priority;                   /* Original Priority before donation */
    struct list_elem sleep_elem;        /* list elem for sleep */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    bool donated;                       /* flag for donation */
    struct list locks;                  /* list of locks */
    struct lock *blocked;               /* the lock that blocks current thread*/

    uint32_t *pagedir;                  /* Page directory. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */

    int exit_status;
    int child_exec;                    /* child success to load exec file */
    bool wait_child;                     /* child wait status */
    int orphan;

    struct thread * parent;
    struct list child_list;
    struct list open_file;
    struct semaphore waitsema;          /* semaphore for wait child */
    struct semaphore loadsema;
    struct lock loadlock;             /* lock for load file of child */
    struct condition loadcond;        /* condvar for siganl load success */
    struct file * exec_file;

#endif

#ifdef VM
  struct lock pagedir_lock;
  struct lock supplementary_page_table_lock;
  struct hash supplementary_page_table;
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };


struct child_thread
  {
   tid_t tid;
   struct thread* thr;
   struct list_elem ch_elem;
   int exit_status;
   int dead;
  };


/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/*new ones*/
/*v1*/
void thread_sleep(int64_t awaketick);
static int64_t next_awakethread_tick;
void set_next_awakethread_tick(int64_t input_tick);
int64_t get_next_awakethread_tick(void);
void thread_awake(int64_t curr_tick);
bool cmp_thread_priority(const struct list_elem *a, const struct list_elem *b, void *aux);
//priority donation
void targeted_thread_set_priority(struct thread *target, int new_priority);
static struct thread *idle_thread;

#endif /* threads/thread.h */
