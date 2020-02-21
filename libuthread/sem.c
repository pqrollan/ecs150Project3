#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"

struct semaphore {
	size_t count;
        queue_t q;
};
/*
 * sem_create - Create semaphore
 * @count: Semaphore count
 *
 * Allocate and initialize a semaphore of internal count @count.
 *
 * Return: Pointer to initialized semaphore. NULL in case of failure when
 * allocating the new semaphore.
 */
sem_t sem_create(size_t count)
{
        enter_critical_section();
	sem_t new_sem = malloc(sizeof(struct semaphore));

        if (new_sem == NULL) {
                return NULL;
        }

        new_sem->count = count;
        new_sem->q = queue_create();
        exit_critical_section();
        return new_sem;
}
/*
 * sem_destroy - Deallocate a semaphore
 * @sem: Semaphore to deallocate
 *
 * Deallocate semaphore @sem.
 *
 * Return: -1 if @sem is NULL or if other threads are still being blocked on
 * @sem. 0 is @sem was successfully destroyed.
 */
int sem_destroy(sem_t sem)
{
        enter_critical_section();
	if (sem == NULL || queue_destroy(sem->q) == -1) {
                return -1;
        }

        free(sem);
        exit_critical_section();
        return 0;
}
/*
 * sem_down - Take a semaphore
 * @sem: Semaphore to take
 *
 * Take a resource from semaphore @sem.
 *
 * Taking an unavailable semaphore will cause the caller thread to be blocked
 * until the semaphore becomes available.
 *
 * Return: -1 if @sem is NULL. 0 if semaphore was successfully taken.
 */
int sem_down(sem_t sem)
{
        enter_critical_section();

        if (sem == NULL) {
                return -1;
        }
        while (sem->count < 1) {
                pthread_t *curr_tid = malloc(sizeof(pthread_t));
                *curr_tid = pthread_self();
                queue_enqueue(sem->q, (void *)curr_tid);
                //exit_critical_section();
                thread_block();
                enter_critical_section();
        }
        //enter_critical_section();

        sem->count--;
        exit_critical_section();
        return 0;
}
/*
 * sem_up - Release a semaphore
 * @sem: Semaphore to release
 *
 * Release a resource to semaphore @sem.
 *
 * If the waiting list associated to @sem is not empty, releasing a resource
 * also causes the first thread (i.e. the oldest) in the waiting list to be
 * unblocked.
 *
 * Return: -1 if @sem is NULL. 0 if semaphore was successfully released.
 */
int sem_up(sem_t sem)
{
        enter_critical_section();
        if (sem == NULL) {
                return -1;
        }

	sem->count++;
        //exit_critical_section();

        if (queue_length(sem->q) > 0) {
                pthread_t *tid_unblock;
                queue_dequeue(sem->q, (void **)&tid_unblock);
                thread_unblock(*tid_unblock);
                free(tid_unblock);
                //exit_critical_section();
        }
        exit_critical_section();
        return 0;
}
/*
 * sem_getvalue - Inspect semaphore's internal state
 * @sem: Semaphore to inspect
 * @sval: Address of data item where value is received
 *
 * If semaphore @sem's internal count is greater than 0, assign internal count
 * to data item pointed by @sval.
 *
 * If semaphore @sems's internal count is equal to 0, assign a negative number
 * whose absolute value is the count of the number of threads currently blocked
 * in sem_down().
 *
 * Return: -1 if @sem or @sval are NULL. 0 if semaphore was successfully
 * inspected.
 */
int sem_getvalue(sem_t sem, int *sval)
{
	if (sem == NULL || sval == NULL) {
                return -1;
        }

        *sval = sem->count;
        return 0;
}
