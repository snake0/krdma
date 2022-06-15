#include "clique.h"
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/jhash.h>


// static int num_nodes = 0, num_cpus = 0, 
//     num_cores = 0, num_threads = 0,
//     cores_per_node;

static int num_nodes = 2, cores_per_node = 8;

struct c_thread_info thread_list[1UL << PID_HASH_BITS];
struct process_info process_list;


int *cpu_state = NULL;
int threads_chosen[NTHREADS];
int cliques_size;
static struct clique cliques[NTHREADS];

// communication rates between threads
int default_matrix[NTHREADS * NTHREADS] = {
    0,  12, 5,  3,  0,  1,  1,  1,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  1, 0, 0, 0, 1,  0,  0,  0,  0,  1,  0,  1,  4,  11,
    12, 0,  12, 2,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  1,  0,  1,  0,
    5,  12, 0,  12, 1,  4,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  1,  1,
    3,  2,  12, 0,  5,  1,  1,  1,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  1,  0,  0,  1,
    0,  0,  1,  5,  0,  10, 0,  1,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  0,  4,  1,  10, 0,  15, 3,  1, 0,  2,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  0,  0,  1,  0,  15, 0,  18, 0, 1,  1,  0,  0,  1,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  0,  0,  1,  1,  3,  18, 0,  8, 3,  2,  2,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  1,  0,  8,  0, 6,  3,  2,  0,  0,  0, 1,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  1,  3,  6, 0,  12, 2,  1,  2,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  2,  1,  2,  3, 12, 0,  10, 1,  1,  0, 1,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  2,  2, 2,  10, 0,  8,  3,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 1,  1,  8,  0,  16, 1, 3,  3,  0,  1, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  1,  0,  0, 2,  1,  3,  16, 0,  6, 2,  3,  1,  0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  1,  6,  0, 8,  0,  0,  2, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  1, 0,  1,  0,  3,  2,  8, 0,  11, 1,  0, 0, 0, 1, 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  3,  3,  0, 11, 0,  12, 5, 1, 2, 0, 0,  0,  1,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  1,  0, 1,  12, 0,  9, 0, 1, 1, 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  1,  0,  2, 0,  5,  9,  0, 7, 5, 0, 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  1,  0,  7, 0, 7, 1, 0,  0,  3,  0,  0,  0,  1,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  2,  1,  5, 7, 0, 9, 1,  2,  2,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 1,  0,  1,  0, 1, 9, 0, 8,  0,  2,  0,  0,  0,  0,  0,  0,  0,
    1,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 1,  0,  1,  1, 0, 1, 8, 0,  10, 3,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 2, 0, 10, 0,  14, 2,  0,  1,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  1,  0,  0, 3, 2, 2, 3,  14, 0,  12, 0,  1,  2,  2,  0,  2,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  2,  12, 0,  16, 2,  2,  2,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  16, 0,  5,  1,  1,  0,  0,
    1,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  1,  1,  2,  5,  0,  10, 4,  4,  1,
    0,  1,  0,  1,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 1, 0, 0, 0,  0,  2,  2,  1,  10, 0,  10, 1,  1,
    1,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  2,  2,  1,  4,  10, 0,  15, 2,
    4,  1,  1,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  0,  0,  0,  4,  1,  15, 0,  13,
    11, 0,  1,  1,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0, 0,  0,  0,  0, 0, 0, 0, 0,  0,  2,  0,  0,  1,  1,  2,  13, 0
};

// Processor topology
// static void detect_topology(void) {
//   int node, cpu, core, thread;
//   if (num_nodes)
//     return;

//   for_each_online_node(node) {
//     ++num_nodes;
//     for_each_node_cpu(cpu, node) {
//       ++num_cpus;
//       for_each_core(core, cpu) {
//         ++num_cores;
//         for_each_sibling(thread, core) {
//           ++num_threads;
//         }
//       }
//     }
//   }
// #ifdef C_PRINT
//   printk(KERN_INFO
//            "topology: %d nodes, %d cpus, %d cores, %d threads",
//            num_nodes, num_cpus, num_cores, num_threads);
// #endif
//   cores_per_node = num_threads / num_nodes;*/
// }

void init_scheduler(void) {
    // detect_topology();
    INIT_LIST_HEAD(&process_list.list);
    process_list.comm[0] = '\0';
    memset(thread_list, 0, sizeof(thread_list));
}

void exit_scheduler(void) {
    // we do cleanup here
    struct process_info *pi;
	struct list_head *curr, *q;
    list_for_each_safe(curr, q, &process_list.list) {
        pi = list_entry(curr, struct process_info, list);
        printk(KERN_ERR "Process %s exit", pi->comm);
        // vfree(pi->mcs);
        list_del(&pi->list);
        kfree(pi);
    }
}

static inline
void set_affinity(int pid, int core) {
    struct cpumask mask;
    cpumask_clear(&mask);
    cpumask_set_cpu(core, &mask);
    sched_setaffinity(pid, &mask);
}

#ifdef C_PRINT

static void print_matrix(int *k, int size) {
    int i, j;
    printk(KERN_ERR "------[ matrix ]------\n");
    for (i = 0; i < size; ++i) {
        for (j = 0; j < size; ++j) {
            printk(KERN_CONT "%2d ", k[SUBSCRIPT(i, j)]);
        }
        printk(KERN_CONT "\n");
    }
}

static void print_array(int *k, int size) {
    int i;
    printk(KERN_ERR "------[ array ]------\n");
    for (i = 0; i < size; ++i) {
        printk(KERN_CONT "%2d ", k[i]);
    }
    printk(KERN_CONT "\n");
}

static void print_clique_sizes(void) {
    int i;
    printk(KERN_ERR "");
    for (i = 0; i < NTHREADS; ++i) {
        if (cliques[i].flag == C_VALID) {
            printk(KERN_CONT "%d ", cliques[i].size);
        }
    }
    printk(KERN_CONT"\n");
}

int print_clique(struct clique *c) {
    int j;
#ifdef VALID_ONLY
    if (c->flag == C_VALID) {
        printk(KERN_ERR "{");
        for (j = 0; j < c->size; ++j) {
            printk(KERN_CONT "%d ", c->pids[j]);
        }
        printk(KERN_CONT "}");
        return 0;
    }
    return -1;
#endif
    switch (c->flag) {
        case C_VALID:
            printk("VALID ");
            break;
        case C_REUSE:
            printk("REUSE ");
            break;
        case C_INVALID:
            printk("INVALID ");
            break;
    }
    printk("{");
    for (j = 0; j < c->size; ++j) {
        printk("%d ", c->pids[j]);
    }
    printk("}");
    return 0;
}

void print_cliques(void) {
    int i, r;
    printk(KERN_ERR "-------------------------------------------------------\n");
    for (i = 0; i < NTHREADS; ++i) {
        r = print_clique(cliques + i);
        if (r == 0) {
            printk(KERN_CONT"\n");
        }
    }
}

void print_processes(void) {
    struct process_info *pi;
	struct list_head *curr;
    int n;

    printk(KERN_ERR "print_processes: start");
    list_for_each(curr, &process_list.list) {
        pi = list_entry(curr, struct process_info, list);
        printk(KERN_ERR "print_processes: Process %s", pi->comm);
        n = atomic_read(&pi->nthreads);
        if (n) {
            printk(KERN_ERR "print_processes: Threads of %s", pi->comm);
            print_array(pi->pids, n);
            // printk(KERN_ERR "Matrix of %s", pi->comm);
            // print_matrix(pi->matrix, n);
        } else {
            printk(KERN_ERR "print_processes: Empty process %s", pi->comm);
        }
    }
    printk(KERN_ERR "print_processes: end");
}

#endif // C_PRINT

int clique_distance(struct clique *c1, struct clique *c2, int *matrix) {
    int ret = 0, i, j;
    if (c1 && c2) {
#ifndef C_USEMAX
        for (i = 0; i < c1->size; ++i) {
            for (j = 0; j < c2->size; ++j) {
                ret += matrix[SUBSCRIPT(c1->pids[i], c2->pids[j])];
            }
        }
#else
        for (i = 0; i < c1->size; ++i) {
            for (j = 0; j < c2->size; ++j) {
                if (ret < matrix[SUBSCRIPT(c1->pids[i], c2->pids[j])]) {
                    ret = matrix[SUBSCRIPT(c1->pids[i], c2->pids[j])];
                }
            }
        }
#endif
    } else {
        printk("clique_distance: NULL pointer\n");
        ret = -1;
    }
    return ret;
}

void merge_clique(struct clique *c1, struct clique *c2) {
    if (c1 && c2) {
#ifdef C_PRINT
        printk("Merging: ");
        print_clique(c1);
        print_clique(c2);
        printk("\n");
#endif
        if (c1->flag != C_VALID) {
            if (c2->flag == C_VALID) {
                c2->flag = C_REUSE;
                cliques_size--;
            }
        } else {
            if (c2->flag != C_VALID) {
                c1->flag = C_REUSE;
                cliques_size--;
            } else {
                memcpy(c1->pids + c1->size, c2->pids, sizeof(int) * c2->size);
                c1->size = c1->size + c2->size;
                c1->flag = C_REUSE;
                c2->flag = C_INVALID;
                cliques_size -= 2;
            }
        }
    } else {
        printk("merge_clique: NULL pointer\n");
    }
}

struct clique *get_first_valid(void) {
    struct clique *ret = cliques;
    while (ret->flag != C_VALID) {
        ret++;
        if (ret == cliques + NTHREADS) {
            printk("get_first_valid: NO valid clique\n");
            return NULL;
        }
    }
    return ret;
}

struct clique *find_neighbor(struct clique *c1) {
    struct clique *c2, *temp = cliques;
    int distance = -1, temp_int;
    while (temp < cliques + NTHREADS) {
        if (temp != c1 && temp->flag == C_VALID) {
            temp_int = clique_distance(c1, temp, default_matrix);
            if (temp_int > distance) {
                distance = temp_int;
                c2 = temp;
            }
        }
        temp++;
    }
    return c2;
}

void reset_cliques(void) {
    struct clique *temp = cliques;
    while (temp < cliques + NTHREADS) {
        if (temp->flag == C_REUSE) {
            temp->flag = C_VALID;
            ++cliques_size;
        }
        temp++;
    }
}

void init_cliques(void) {
    int i;
    for (i = 0; i < NTHREADS; ++i) {
        cliques[i].pids[0] = i;
        cliques[i].size = 1;
        cliques[i].flag = C_VALID;
    }
    cliques_size = NTHREADS;
}

void init_matrix(int *matrix) {
    memcpy(matrix, default_matrix, sizeof(default_matrix));
}

void assign_cpus_for_clique(struct clique *c, int node) {
    int cpu_curr = cores_per_node * node, size = c->size, *pids = c->pids, i;
    for (i = 0; i < size; ++i) {
        threads_chosen[pids[i]] = cpu_curr++;
        if (cpu_curr == (node + 1) * cores_per_node) {
            cpu_curr = cores_per_node * node;
        }
    }
}

void calculate_threads_chosen(void) {
    int i, node = 0;
    for (i = 0; i < NTHREADS; ++i) {
        if (cliques[i].flag == C_VALID) {
            assign_cpus_for_clique(cliques + i, node++);
        }
    }
#ifdef C_PRINT
    printk("Threads chosen:\n");
    for (i = 0; i < NTHREADS; ++i) {
        printk("%d -> %d\n", i, threads_chosen[i]);
    }
#endif
}

void clique_analysis(void) {
    struct clique *c1, *c2;
    init_cliques();
#ifdef C_PRINT
    print_matrix(default_matrix, NTHREADS);
    print_cliques();
#endif
    while (cliques_size > num_nodes) {
        while (cliques_size > 0) {
            c1 = get_first_valid();
            c2 = find_neighbor(c1);
            merge_clique(c1, c2);
        }
        reset_cliques();
#ifdef C_PRINT
        printk(KERN_ERR "cliques:");
        print_cliques();
        printk(KERN_ERR "cliques_size: %d, with ", cliques_size);
        print_clique_sizes();
#endif
    }
    calculate_threads_chosen();
}



int init_module(void) {
    // init_scheduler();
    // insert_process("stress-ng", 1112);
    // insert_thread("stress-ng", 1113);
    // insert_thread("stress-ng", 1114);

    // print_processes();

    // insert_process("sysbench", 1120);
    // insert_thread("sysbench", 1121);
    // insert_thread("sysbench", 1122);

    // print_processes();

    // remove_thread("sysbench", 1120);
    // remove_thread("sysbench", 1121);
    // remove_thread("sysbench", 1122);

    // print_processes();

    // insert_process("sysbench", 1120);
    // insert_thread("sysbench", 1121);
    // insert_thread("sysbench", 1122);

    // print_processes();
    // exit_scheduler();
    
    return 0;
}

void cleanup_module(void) {}

MODULE_LICENSE("GPL");