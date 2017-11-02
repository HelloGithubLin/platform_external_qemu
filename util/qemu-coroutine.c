/*
 * QEMU coroutines
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Stefan Hajnoczi    <stefanha@linux.vnet.ibm.com>
 *  Kevin Wolf         <kwolf@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "util/trace.h"
#include "qemu-common.h"
#include "qemu/thread.h"
#include "qemu/thread_local.h"
#include "qemu/atomic.h"
#include "qemu/coroutine.h"
#include "qemu/coroutine_int.h"
#include "block/aio.h"

enum {
#if defined(_WIN32) && !defined(_WIN64)
    POOL_BATCH_SIZE = 8,
#else
    POOL_BATCH_SIZE = 64,
#endif
};

/** Free list to speed up creation */
static QSLIST_HEAD(, Coroutine) release_pool = QSLIST_HEAD_INITIALIZER(pool);
static unsigned int release_pool_size;

typedef struct {
    QSLIST_HEAD(, Coroutine) pool;
    unsigned int size;
    Notifier cleanup_notifier;
} CoroutinePool;

QEMU_THREAD_LOCAL_DECLARE(CoroutinePool, co_alloc_pool);

static void coroutine_pool_cleanup(Notifier *n, void *value)
{
    Coroutine *co;
    Coroutine *tmp;

    CoroutinePool* pool = QEMU_THREAD_LOCAL_GET_PTR(co_alloc_pool);
    QSLIST_FOREACH_SAFE(co, &pool->pool, pool_next, tmp) {
        QSLIST_REMOVE_HEAD(&pool->pool, pool_next);
        qemu_coroutine_delete(co);
    }
}

Coroutine *qemu_coroutine_create(CoroutineEntry *entry, void *opaque)
{
    Coroutine *co = NULL;

    if (CONFIG_COROUTINE_POOL) {
        CoroutinePool* pool = QEMU_THREAD_LOCAL_GET_PTR(co_alloc_pool);
        co = QSLIST_FIRST(&pool->pool);
        if (!co) {
            if (release_pool_size > POOL_BATCH_SIZE) {
                /* Slow path; a good place to register the destructor, too.  */
                if (!pool->cleanup_notifier.notify) {
                    pool->cleanup_notifier.notify = coroutine_pool_cleanup;
                    qemu_thread_atexit_add(&pool->cleanup_notifier);
                }

                /* This is not exact; there could be a little skew between
                 * release_pool_size and the actual size of release_pool.  But
                 * it is just a heuristic, it does not need to be perfect.
                 */
                pool->size = atomic_xchg(&release_pool_size, 0);
                QSLIST_MOVE_ATOMIC(&pool->pool, &release_pool);
                co = QSLIST_FIRST(&pool->pool);
            }
        }
        if (co) {
            QSLIST_REMOVE_HEAD(&pool->pool, pool_next);
            pool->size--;
        }
    }

    if (!co) {
        co = qemu_coroutine_new();
    }

    co->entry = entry;
    co->entry_arg = opaque;
    QSIMPLEQ_INIT(&co->co_queue_wakeup);
    return co;
}

static void coroutine_delete(Coroutine *co)
{
    co->caller = NULL;

    if (CONFIG_COROUTINE_POOL) {
        if (release_pool_size < POOL_BATCH_SIZE * 2) {
            QSLIST_INSERT_HEAD_ATOMIC(&release_pool, co, pool_next);
            atomic_inc(&release_pool_size);
            return;
        }
        CoroutinePool* pool = QEMU_THREAD_LOCAL_GET_PTR(co_alloc_pool);
        if (pool->size < POOL_BATCH_SIZE) {
            QSLIST_INSERT_HEAD(&pool->pool, co, pool_next);
            pool->size++;
            return;
        }
    }

    qemu_coroutine_delete(co);
}

void qemu_aio_coroutine_enter(AioContext *ctx, Coroutine *co)
{
    Coroutine *self = qemu_coroutine_self();
    CoroutineAction ret;

    trace_qemu_aio_coroutine_enter(ctx, self, co, co->entry_arg);

    if (co->caller) {
        fprintf(stderr, "Co-routine re-entered recursively\n");
        abort();
    }

    co->caller = self;
    co->ctx = ctx;

    /* Store co->ctx before anything that stores co.  Matches
     * barrier in aio_co_wake and qemu_co_mutex_wake.
     */
    smp_wmb();

    ret = qemu_coroutine_switch(self, co, COROUTINE_ENTER);

    qemu_co_queue_run_restart(co);

    switch (ret) {
    case COROUTINE_YIELD:
        return;
    case COROUTINE_TERMINATE:
        assert(!co->locks_held);
        trace_qemu_coroutine_terminate(co);
        coroutine_delete(co);
        return;
    default:
        abort();
    }
}

void qemu_coroutine_enter(Coroutine *co)
{
    qemu_aio_coroutine_enter(qemu_get_current_aio_context(), co);
}

void qemu_coroutine_enter_if_inactive(Coroutine *co)
{
    if (!qemu_coroutine_entered(co)) {
        qemu_coroutine_enter(co);
    }
}

void coroutine_fn qemu_coroutine_yield(void)
{
    Coroutine *self = qemu_coroutine_self();
    Coroutine *to = self->caller;

    trace_qemu_coroutine_yield(self, to);

    if (!to) {
        fprintf(stderr, "Co-routine is yielding to no one\n");
        abort();
    }

    self->caller = NULL;
    qemu_coroutine_switch(self, to, COROUTINE_YIELD);
}

bool qemu_coroutine_entered(Coroutine *co)
{
    return co->caller;
}
