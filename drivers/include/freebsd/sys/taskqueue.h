/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/taskqueue.h shim.
 *
 * Task queues schedule deferred work items.  In the polled Unikraft shim
 * all task operations are no-ops: card-insertion/removal tasks are never
 * enqueued because we bypass the mmc.c driver stack entirely.
 */
#pragma once
#include <stddef.h>
#include <sys/callout.h>

struct task {
    int _ta_priority;
    void (*_ta_func)(void *, int);
    void *_ta_context;
};

struct timeout_task {
    struct task     t;
    struct callout  c;
};

struct taskqueue;

/* The global bus taskqueue (used by sdhci.c via taskqueue_enqueue etc.) */
extern struct taskqueue *taskqueue_bus;

#define TASK_INIT(task, priority, func, context) \
    do { \
        (task)->_ta_priority = (priority); \
        (task)->_ta_func     = (func); \
        (task)->_ta_context  = (context); \
    } while (0)

#define TIMEOUT_TASK_INIT(tq, timeout_task, priority, func, context) \
    TASK_INIT(&(timeout_task)->t, priority, func, context)

static inline int
taskqueue_enqueue(struct taskqueue *tq __attribute__((unused)),
                  struct task *task __attribute__((unused)))
{ return 0; }

static inline int
taskqueue_enqueue_timeout(struct taskqueue *tq __attribute__((unused)),
                          struct timeout_task *tt __attribute__((unused)),
                          int ticks __attribute__((unused)))
{ return 0; }

static inline void
taskqueue_drain(struct taskqueue *tq __attribute__((unused)),
                struct task *task __attribute__((unused))) {}

static inline void
taskqueue_drain_timeout(struct taskqueue *tq __attribute__((unused)),
                        struct timeout_task *tt __attribute__((unused))) {}
