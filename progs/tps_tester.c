#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

void *latest_mmap_addr;

void *__real_mmap(void *addr, size_t len, int prot, int flags, int fildes,
        off_t off);
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes,
        off_t off)
{
        latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
        return latest_mmap_addr;
}

static char msg1[TPS_SIZE] = "Hello world!\n";
static char msg2[TPS_SIZE] = "hello world!\n";

static sem_t sem1, sem2;


void *thread2(__attribute__((unused)) void *arg)
{
	char *buffer = malloc(TPS_SIZE);

	/* Create TPS and initialize with *msg1 */
	tps_create();
	tps_write(0, TPS_SIZE, msg1);

	/* Read from TPS and make sure it contains the message */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread2: read OK!\n");

	/* Transfer CPU to thread 1 and get blocked */
	sem_up(sem1);
	sem_down(sem2);

	/* When we're back, read TPS and make sure it sill contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread2: read OK!\n");

	/* Transfer CPU to thread 1 and get blocked */
	sem_up(sem1);
	sem_down(sem2);

	/* Destroy TPS and quit */
	tps_destroy();
	free(buffer);
	return NULL;
}

void *thread1(__attribute__((unused)) void *arg)
{
	pthread_t tid;
	char *buffer = malloc(TPS_SIZE);

	/* Create thread 2 and get blocked */
	pthread_create(&tid, NULL, thread2, NULL);
	sem_down(sem1);

	/* When we're back, clone thread 2's TPS */
	tps_clone(tid);

	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread1: read OK!\n");

	/* Modify TPS to cause a copy on write */
	buffer[0] = 'h';
	tps_write(0, 1, buffer);

	/* Transfer CPU to thread 2 and get blocked */
	sem_up(sem2);
	sem_down(sem1);

	/* When we're back, make sure our modification is still there */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!strcmp(msg2, buffer));
	printf("thread1: read OK!\n");

	/* Transfer CPU to thread 2 */
	sem_up(sem2);

	/* Wait for thread2 to die, and quit */
	pthread_join(tid, NULL);
	tps_destroy();
	free(buffer);
	return NULL;
}

void *threadWrite(__attribute__((unused)) void *arg)
{
	char *buffer = malloc(TPS_SIZE);
        tps_create();
        tps_write(0, TPS_SIZE, msg1);

	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("Write thread has correctly writen: read OK!\n");

	/* Modify TPS to write again */
	buffer[0] = 'h';
	tps_write(0, 1, buffer);

	/* Make sure our modification is still there */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!strcmp(msg2, buffer));
	printf("Write thread modification: read OK!\n");

	/* Wait for thread2 to die, and quit */
	tps_destroy();
	free(buffer);
	return NULL;
}

void *noTPS(__attribute__((unused)) void *arg)
{
        return NULL;
}

void *emptyTPS(__attribute__((unused)) void *arg)
{
        char *buffer = malloc(TPS_SIZE);
        tps_create();
        tps_write(0, TPS_SIZE, msg1);

	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));       
        return NULL;
}

int main(void)
{
        pthread_t tid;

	/* Create two semaphores for thread synchro */
	sem1 = sem_create(0);
	sem2 = sem_create(0);


	/* Init TPS API */
	tps_init(1);
        tps_create();

        /* begin fault testing the TPS */
        assert(tps_init(1)==-1);
        assert(tps_create()==-1);
        assert(tps_destroy()==0);
        assert(tps_destroy()==-1);

        /* testing tps functions on empty tps */
        char *buffer = malloc(TPS_SIZE);
        assert(tps_write(0, TPS_SIZE, msg1)==-1);
        assert(tps_read(0, TPS_SIZE, buffer)==-1);
        
        /*clone fault tests */
        pthread_create(&tid, NULL, noTPS, NULL);
        assert(tps_clone(tid)==-1);
        tps_create();
        pthread_create(&tid, NULL, emptyTPS, NULL);
        assert(tps_clone(tid)==-1);
        pthread_join(tid, NULL);

        /* Create write thread and wait */
	pthread_create(&tid, NULL, threadWrite, NULL);
	pthread_join(tid, NULL);

        /* Create thread 1 and wait */
	pthread_create(&tid, NULL, thread1, NULL);
	pthread_join(tid, NULL);

        /* Destroy resources*/
	sem_destroy(sem1);
	sem_destroy(sem2);


        /*  segfault test  */
        char *tps_addr = latest_mmap_addr;

        tps_addr[0] = ' ';

        return 0;
}
