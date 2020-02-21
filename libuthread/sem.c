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

/* Sem_create allocates space for the semaphore and sets the count equal to
the input. It then creates a queue to be used as a blocked queue for the
semaphore.  */
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

/* Sem destroy calles queue destroy, and assures success. Then frees the
space associated with the semaphore */
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

/* Sem down blocks a thread if there are no available resources and
decrements the semaphore's count once the thread is able to run */
int sem_down(sem_t sem)
{
	enter_critical_section();

	if (sem == NULL) {
		return -1;
	}
        /* While loop handles corner cases where resources are snatched by
        another thread before and unblocked thread can claim them */
	while (sem->count < 1) {
                /* Creates a pthread_t pointer to be enqueue'd to the
                blocked queue. This pointer is later free'd in sem_up() */

		pthread_t *curr_tid = malloc(sizeof(pthread_t));
		*curr_tid = pthread_self();
		queue_enqueue(sem->q, (void *)curr_tid);
		thread_block();
		enter_critical_section();
	}
	sem->count--;
	exit_critical_section();
	return 0;
}

/* increments the count and unblocks any thread waiting for resources */
int sem_up(sem_t sem)
{
	enter_critical_section();
	if (sem == NULL) {
		return -1;
	}

	sem->count++;
	if (queue_length(sem->q) > 0) {
		pthread_t *tid_unblock;
		queue_dequeue(sem->q, (void **)&tid_unblock);
		thread_unblock(*tid_unblock);

                /* Free pthread_t allocated by sem_down */
		free(tid_unblock);
	}
	exit_critical_section();
	return 0;
}

/* Gets the semaphore'c count */
int sem_getvalue(sem_t sem, int *sval)
{
	if (sem == NULL || sval == NULL) {
		return -1;
	}

	*sval = sem->count;
	return 0;
}
