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
		thread_block();
		enter_critical_section();
	}
	sem->count--;
	exit_critical_section();
	return 0;
}

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
		free(tid_unblock);
	}
	exit_critical_section();
	return 0;
}

int sem_getvalue(sem_t sem, int *sval)
{
	if (sem == NULL || sval == NULL) {
		return -1;
	}

	*sval = sem->count;
	return 0;
}
