#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

/* mempage holds a void pointer to the private memory used by the thread.
This allows multiple threads to point the the same dynamically allocated 
space in the case of cloning. When this occurs, num_refs keeps track of the
number of cloning threads/ */
struct mempage {
	void *memptr;
	int num_refs;
};

/* Holds the TID of the the thread this TPS belogs too, allowing us to find
the tps. Holds a memarea which contains the a reference to the allocated
memory. */
struct tps {
	pthread_t tid;
	struct mempage *memarea;
};

queue_t tps_q = NULL;

/* This function helps us find a specific tps in a tps queue by searching
for it's TID */
int find_tps(void *data, void *arg)
{
	if (pthread_equal(*((pthread_t *) arg), ((struct tps *) data)->tid)) {
		return 1;
	} else {
		return 0;
	}
}

/* This function helps us find a specific tps in a tps queue by searching
for the allocated space */
int find_by_memarea(void *data, void *arg)
{
	if (((void *) arg) == ((struct tps *) data)->memarea->memptr) {
		return 1;
	} else {
		return 0;
	}
}

/* This signal handler will throw an error when private memory is accessed
and will exit the program */
static void segv_handler(int sig, siginfo_t *si, __attribute__((unused)) void
	*context)
{
	void *p_fault = (void *)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));

	enter_critical_section();
	void *temp = NULL;
	queue_iterate(tps_q, &find_by_memarea, p_fault, &temp);
	exit_critical_section();
	if (temp != NULL) {
		fprintf(stderr, "TPS protection error!\n");
	}

	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);

	raise(sig);
}

/* Initializes the TPS functionality by creating a queue where the TPS's
can be stored and initiallized the signal handler to maintain privacy */
int tps_init(int segv)
{
	enter_critical_section();
	if (tps_q != NULL) {
		exit_critical_section();
		return -1;
	}

	tps_q = queue_create();

	if (tps_q == NULL) {
		exit_critical_section();
		return -1;
	}

	if (segv) {
		struct sigaction sa;

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = segv_handler;
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGSEGV, &sa, NULL);
	}

	exit_critical_section();
	return 0;
}

/* Allocates space for the TPS amd assign it's TID to the current thread */
int tps_create(void)
{
	enter_critical_section();
	if (tps_q == NULL) {
		return -1;
	}

	struct tps *new_tps = malloc(sizeof(struct tps));
	new_tps->tid = pthread_self();

	void *temp = NULL;
	/* Makes sure there does not already exist a TPS for this thread */
	queue_iterate(tps_q, &find_tps, &(new_tps->tid), &temp);
	if (temp != NULL) {
		free(new_tps);
		exit_critical_section();
		return -1;
	}

	new_tps->memarea = malloc(sizeof(struct mempage));
	if (new_tps->memarea == NULL) {
		exit_critical_section();
		return -1;
	}

	/* Creates a space in memory that the thread can later use to read and
	write. This space is initially filled with all 0s */
	new_tps->memarea->num_refs = 1;
	new_tps->memarea->memptr = mmap(NULL, TPS_SIZE, PROT_WRITE,
		MAP_ANON|MAP_PRIVATE, -1, 0);
	memset(new_tps->memarea->memptr, 0, TPS_SIZE);

	/* Protection is set to not allow reading or writing by default */
	if (mprotect(new_tps->memarea->memptr, TPS_SIZE, PROT_NONE) < 0) {
		exit_critical_section();
		return -1;
	}

	/* Every TPS is enqueue'd to the tps queue to be found later */
	queue_enqueue(tps_q, (void *)new_tps);
	exit_critical_section();
	return 0;
}

/* Used to free the memory allowcated for the TPS. Checks to make sure
there are no other threads referencing it as their own */
int unmap_tps(void *data, void *arg)
{
	struct tps *curr_tps = data;
	if (pthread_equal(*((pthread_t *) arg), curr_tps->tid) && 
		curr_tps->memarea->num_refs <= 1) {
		munmap(curr_tps->memarea->memptr, TPS_SIZE);
		return 1;
	} else {
		return 0;
	}
}

/* Frees all memory associated with the TPS, calls unmap_tps to assist */
int tps_destroy(void)
{
	pthread_t curr_tid = pthread_self();

	enter_critical_section();
	void *temp = NULL;
	queue_iterate(tps_q, &unmap_tps, &curr_tid, &temp);
	if (temp == NULL) {
		exit_critical_section();
		return -1;
	} else {
		struct tps *curr_tps = temp;
		if (curr_tps->memarea->num_refs > 1) {
			curr_tps->memarea->num_refs--;
		} else {
			free(((struct tps *) temp)->memarea);
		}

		queue_delete(tps_q, temp);
		free(temp);
		exit_critical_section();
		return 0;
	}
}

int tps_read(size_t offset, size_t length, void *buffer)
{
	pthread_t curr_tid = pthread_self();

	enter_critical_section();
	struct tps *curr_tps = NULL;
	/* Iterate to find the right tps to read from */
	queue_iterate(tps_q, &find_tps, &curr_tid, (void **) &curr_tps);

	if (curr_tps == NULL || buffer == NULL) {
		exit_critical_section();
		return -1;
	}
	/* testing to make sure there isn't an overflow */
	if (offset + length > TPS_SIZE) {
		exit_critical_section();
		return -1;
	}

	/* Changes the permission to allow a read */
	if (mprotect(curr_tps->memarea->memptr, TPS_SIZE, PROT_READ) < 0) {
		exit_critical_section();
		return -1;
	}

	memcpy(buffer, curr_tps->memarea->memptr + offset, length);

	/* Returns permission back to none before exiting */
	if (mprotect(curr_tps->memarea->memptr, TPS_SIZE, PROT_NONE) < 0) {
		exit_critical_section();
		return -1;
	}

	exit_critical_section();
	return 0;
}

int tps_write(size_t offset, size_t length, void *buffer)
{
	pthread_t curr_tid = pthread_self();

	enter_critical_section();

	struct tps *curr_tps = NULL;
	/* Iterate to find the right tps to write too */
	queue_iterate(tps_q, &find_tps, &curr_tid, (void **) &curr_tps);

	if (curr_tps == NULL || buffer == NULL) {
		exit_critical_section();
		return -1;
	}
	/* checking for overflow */
	if (offset + length > TPS_SIZE) {
		exit_critical_section();
		return -1;
	}

	/* Checks for copies, choosing to write to a new unique mempage if there
	are more than 1 references to a mempage */ 
	if (curr_tps->memarea->num_refs > 1) {
		struct mempage *newpage = malloc(sizeof(struct mempage));
		newpage->num_refs = 1;
		newpage->memptr = mmap(NULL, TPS_SIZE, PROT_WRITE,
			MAP_ANON|MAP_PRIVATE, -1, 0);

		if (newpage == NULL) {
			free(newpage);
			exit_critical_section();
			return -1;
		}
		tps_read(0, TPS_SIZE, newpage->memptr);
		curr_tps->memarea->num_refs--;
		curr_tps->memarea = newpage;
	}
	else {
		/* Sets write permissions on TPS */
		if (mprotect(curr_tps->memarea->memptr, TPS_SIZE, PROT_WRITE) < 0) {
			perror(NULL);
			exit_critical_section();
			return -1;
		}
	}

	memcpy(curr_tps->memarea->memptr + offset, buffer, length);

	/* returns the page's privacy settings to none for either an old or a new
	page */
	if (mprotect(curr_tps->memarea->memptr, TPS_SIZE, PROT_NONE) < 0) {
		exit_critical_section();
		return -1;
	}

	exit_critical_section();
	return 0;
}

/* creates a new TPS  with a unique TID but sets the memarea to point to
the existing memarea of another thread's TPS */
int tps_clone(pthread_t tid)
{
	enter_critical_section();
	struct tps *new_tps = malloc(sizeof(struct tps));
	new_tps->tid = pthread_self();

	void *temp = NULL;
	queue_iterate(tps_q, &find_tps, &(new_tps->tid), &temp);
	if (temp != NULL) {
		free(new_tps);
		exit_critical_section();
		return -1;
	}

	struct tps *cpy_tps = NULL;
	queue_iterate(tps_q, &find_tps, &tid, (void **) &cpy_tps);

	if (cpy_tps == NULL) {
		exit_critical_section();
		return -1;
	}

	/* Increments the number of references so that the tps_write() function
	can correctly differentiate between copied pages and unique ones */
	new_tps->memarea = cpy_tps->memarea;
	new_tps->memarea->num_refs++;

	queue_enqueue(tps_q, (void *)new_tps);
	exit_critical_section();
	return 0;
}
