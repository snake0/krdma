#ifndef __CLIQUE_H__
#define __CLIQUE_H__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/hashtable.h>
#include <linux/sched.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/kvm_para.h>


#define NTHREADS_SHIFT 6UL
#define NTHREADS (1 << NTHREADS_SHIFT)
#define SUBSCRIPT(x,y) (((x) << NTHREADS_SHIFT) + (y))
#define C_PRINT
#define VALID_ONLY

#define PID_HASH_BITS 14UL
#define PID_HASH_SIZE (1UL << PID_HASH_BITS)
#define MEM_HASH_BITS 26UL
#define MEM_HASH_SIZE (1UL << MEM_HASH_BITS)
#define PN(addr) ((addr) >> 12UL)


//#define C_USEMAX

#define C_ASSERT(v) 										\
	{ 																		\
		if (unlikely(!(v))) {								\
			printk(KERN_ERR "C_ASSERT failed in %s at %d", __FUNCTION__, __LINE__);\
		}																		\
	}

struct clique {
    int pids[NTHREADS];
    int size;
    enum {
        C_VALID, C_REUSE, C_INVALID
    } flag;
};

struct mem_acc {
  short tids[2];
};

struct process_info {
	char comm[TASK_COMM_LEN];
	atomic_t nthreads;
	int matrix[NTHREADS * NTHREADS];
	int pids[NTHREADS];
	struct list_head list;
	struct mem_acc *mcs;
};

struct c_thread_info {
	struct process_info *pi;
	short tid; // for indexing matrix
};

extern struct c_thread_info thread_list[1UL << PID_HASH_BITS];
extern struct process_info process_list;

// static inline
// void *resize(void *old, unsigned long old_size, unsigned long new_size) {
// 	void *ret = kmalloc(new_size, GFP_KERNEL);
// 	memcpy(ret, old, old_size);
// 	kfree(old);
// 	memset(ret + old_size, 0, new_size - old_size);
// 	return ret;
// }

// called before insert_process & insert_thread
static inline
int check_name(char *comm) {
	int len = strlen(comm);
	if (unlikely(comm[len - 2] == '.' && comm[len - 1] == 'x'))
		return 1;
	return 0;
}


static inline
void reset_process_info(struct process_info *pi) {
	C_ASSERT(pi);
	memset(pi->matrix, 0, sizeof(pi->matrix));
	memset(pi->pids, -1, sizeof(pi->pids));
	memset(pi->mcs, -1, sizeof(struct mem_acc) << MEM_HASH_BITS);
	atomic_set(&pi->nthreads, 0);
}

static inline
struct process_info *search_process_info(char *comm) {
	struct process_info *pi = NULL;
	struct list_head *curr;
	
	list_for_each(curr, &process_list.list) {
		pi = list_entry(curr, struct process_info, list);
		if (!strcmp(pi->comm, comm)) {
			return pi;
		}
	}
	return NULL;
}

static inline
void insert_process(char *comm, int pid) {
	struct process_info *pi = search_process_info(comm);
	int h;
	
	if (pi) {
		if (atomic_read(&pi->nthreads)) {
			C_ASSERT(0);
			printk(KERN_ERR "Duplicate process %s, %d", comm, pid);
			return;
		} else {
			printk("Reusing process %s, %d", comm, pid);
			atomic_set(&pi->nthreads, 1);
			pi->pids[0] = pid;
			h = hash_32(pid, PID_HASH_BITS);
			C_ASSERT(thread_list[h].pi == NULL);
			thread_list[h].pi = pi;
			thread_list[h].tid = 0;
			return;
		}
	}

	pi = (struct process_info *)
		kmalloc(sizeof(struct process_info), GFP_KERNEL);
	C_ASSERT(pi != NULL);

	strcpy(pi->comm, comm);
	atomic_set(&pi->nthreads, 1);
	memset(pi->matrix, 0, sizeof(pi->matrix));
	memset(pi->pids, -1, sizeof(pi->pids));
	
	pi->pids[0] = pid;
	INIT_LIST_HEAD(&pi->list);
	list_add(&pi->list, &process_list.list);

	h = hash_32(pid, PID_HASH_BITS);
	C_ASSERT(thread_list[h].pi == NULL);
	thread_list[h].pi = pi;
	thread_list[h].tid = 0;

	// pi->mcs = (struct mem_acc *) vmalloc(sizeof(struct mem_acc) << MEM_HASH_BITS);
	C_ASSERT(pi->mcs);
	memset(pi->mcs, -1, sizeof(struct mem_acc) << MEM_HASH_BITS);
}

static inline
void insert_thread(char *comm, int pid) {
	int h = hash_32(pid, PID_HASH_BITS);
	short tid;
	struct process_info *pi = search_process_info(comm);

	if (!pi) {
		C_ASSERT(pi != NULL);
		printk(KERN_ERR "No process %s, %d", comm, pid);
		return;
	}

	if (thread_list[h].pi) {
		C_ASSERT(thread_list[h].pi == NULL);
		printk("Thread hash collision %s, %d", comm, pid);
		return;
	}

	thread_list[h].pi = pi;
	tid = atomic_inc_return(&pi->nthreads) - 1;

	pi->pids[tid] = pid;
	thread_list[h].tid = tid;
}

void remove_thread(char *comm, int pid) {
	int h = hash_32(pid, PID_HASH_BITS);
	short tid;
	struct process_info *pi = NULL;
	struct list_head *curr;
	
	list_for_each(curr, &process_list.list) {
		pi = list_entry(curr, struct process_info, list);
		if (!strcmp(pi->comm, comm)) {
			break;
		}
	}

	if (!pi) {
		C_ASSERT(pi);
		printk(KERN_ERR "No process %s, %d", comm, pid);
		return;
	}
	if (!thread_list[h].pi) {
		C_ASSERT(thread_list[h].pi);
		printk(KERN_ERR "No process info %s, %d", comm, pid);
		return;
	}
	
	thread_list[h].pi = NULL;
	thread_list[h].tid = -1;

	tid = atomic_dec_return(&pi->nthreads);
	if (unlikely(tid == 0)) {
		reset_process_info(pi);
	}
}


static inline
int get_nshare(struct mem_acc *mc) {
  if (mc->tids[0] == -1) {
    if (mc->tids[1] == -1) {
      return 0;
    } else {
      return 1;
    }
  } else {
    if (mc->tids[1] == -1) {
      return 1;
    } else {
      return 2;
    }
  }
}

static inline
void inc_matrix(int *matrix, int x, int y) {
  ++matrix[SUBSCRIPT(x, y)];
  matrix[SUBSCRIPT(x, y)] = matrix[SUBSCRIPT(y, x)];
}

static inline
void record_access(int pid, unsigned long address) {
  int h = hash_32(pid, PID_HASH_BITS);
  struct process_info *pi = thread_list[h].pi;
  short tid = thread_list[h].tid;
  struct mem_acc *mc;

  C_ASSERT(pi);
  mc = &pi->mcs[hash_32(PN(address), MEM_HASH_BITS)];

  switch(get_nshare(mc)) {
    case 0: {
      mc->tids[0] = tid;
      break;
    }

    case 1: {
      if (mc->tids[0] != tid) {
        mc->tids[1] = mc->tids[0];
        mc->tids[0] = tid;
        inc_matrix(pi->matrix, tid, mc->tids[1]);
      }
      break;
    }

    case 2: {
      if (mc->tids[0] != tid) {
        if (mc->tids[1] != tid) {
          inc_matrix(pi->matrix, tid, mc->tids[0]);
          inc_matrix(pi->matrix, tid, mc->tids[1]);
        } else {
          inc_matrix(pi->matrix, tid, mc->tids[0]);
          mc->tids[1] = mc->tids[0];
          mc->tids[0] = tid;
        }
      } else if (mc->tids[1] != tid) {
        inc_matrix(pi->matrix, tid, mc->tids[1]);
      }
    }
  } 
}


#endif