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

static char msg1[TPS_SIZE] = "Hello World!\n";
static char msg2[TPS_SIZE] = "hello World!\n";

static sem_t sem1, sem2;

// Remove At End
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
/*
void *noTPS(__attribute__((unused)) void *arg)
{
        return NULL;
}

void *emptyTPS(__attribute__((unused)) void *arg)
{
        char *buffer = malloc(TPS_SIZE);
        tps_create();
        tps_write(0, TPS_SIZE, msg1);
*/
	/* Read the TPS and make sure it contains the original */
/*	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));       
        
        return NULL;
}*/

void test_init(void)
{
        assert(tps_init(1) == 0);
}

void test_create(void)
{
        assert(tps_create() == 0);
        tps_destroy();
}

void test_destroy(void)
{
        tps_create();
        assert(tps_destroy() == 0);
}

void *basic_thread(__attribute__((unused)) void *arg)
{
        assert(tps_create()==0);
        sem_up(sem2);
        sem_down(sem1);

        tps_destroy();
        return NULL;
}

void test_clone(void)
{
        sem1 = sem_create(0);
        sem2 = sem_create(0);

        pthread_t tid;
        pthread_create(&tid, NULL, basic_thread, NULL);

        sem_down(sem2);

        assert(tps_clone(tid) == 0);
        sem_up(sem1);
        pthread_join(tid, NULL);
        
        tps_destroy();
        sem_destroy(sem1);
        sem_destroy(sem2);
}

void test_double_init(void)
{
        assert(tps_init(1)==-1);
}

void test_double_destroy(void)
{
        tps_create();
        tps_destroy();
        assert(tps_destroy() == -1);
}

void test_double_create(void)
{
        tps_create();
        assert(tps_create() == -1);
        tps_destroy();
}

void test_no_init_cd(void)
{
        assert(tps_create() == -1);
        assert(tps_destroy() == -1);
}

void test_no_init_wr(void)
{
        char *buffer = malloc(10);
        assert(tps_write(0, 10, buffer) == -1);
        assert(tps_read(0, 10, buffer) == -1);
        free(buffer);
}

void test_clone_notid(void)
{
        assert(tps_clone(0) == -1);
}

void test_double_clone(void)
{
        tps_create();
        assert(tps_clone(pthread_self()) == -1);
        tps_destroy();
}

void test_invalid_offset(void)
{
        char *buffer = malloc(TPS_SIZE);
        tps_create();
        assert(tps_read(TPS_SIZE + 1, 0, buffer) == -1);
        assert(tps_write(TPS_SIZE + 1, 0, buffer) == -1);
        tps_destroy();
        free(buffer);
}

void test_invalid_length(void)
{
        char *buffer = malloc(TPS_SIZE);
        tps_create();
        assert(tps_read(0, TPS_SIZE + 1, buffer) == -1);
        assert(tps_write(0, TPS_SIZE + 1, buffer) == -1);
        tps_destroy();
        free(buffer);
}

void test_rw_basic(void)
{
	char *buffer = malloc(TPS_SIZE);
        tps_create();
        tps_write(0, TPS_SIZE, msg1);

	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));

	/* Modify TPS to write again */
	buffer[0] = 'h';
	tps_write(0, 1, buffer);

	/* Make sure our modification is still there */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!strcmp(msg2, buffer));

	tps_destroy();
	free(buffer);
}

void test_offset(void)
{
        char *buffer = malloc(TPS_SIZE);
        char str1[] = "Hello world!";
        char str2[] = "Hello, TA's!";

        tps_create();
        tps_write(0, strlen(str1), str1);
        tps_write(100, strlen(str2), str2);
        
	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, strlen(str1), buffer);
	assert(!memcmp(str1, buffer, strlen(str1)));
        
        tps_read(100, strlen(str2), buffer);
        assert(!memcmp(str2, buffer, strlen(str2)));

	tps_destroy();
	free(buffer);
}

void test_rw_int(void)
{
        int num1 = 4;
        int num2 = 8;
	int *buffer = malloc(TPS_SIZE);
        tps_create();
        tps_write(0, sizeof(int), &num1);

	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, sizeof(int), buffer);
	assert(num1 == buffer[0]);

	/* Modify TPS to write again */
	buffer[1] = 8;
	tps_write(0, 2*sizeof(int), buffer);

	/* Make sure our modification is still there */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(num2 == buffer[1]);

	tps_destroy();
	free(buffer);
}

void test_rw_float(void)
{
        float num1 = 4.5;
        float num2 = 8.5;
	float *buffer = malloc(TPS_SIZE);
        tps_create();
        tps_write(0, sizeof(float), &num1);

	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, sizeof(float), buffer);
	assert(num1 == buffer[0]);

	/* Modify TPS to write again */
	buffer[1] = 8.5;
	tps_write(0, 2*sizeof(float), buffer);

	/* Make sure our modification is still there */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(num2 == buffer[1]);

	tps_destroy();
	free(buffer);
}

void *clone_mem_help(__attribute__((unused)) void *arg)
{
        tps_create();
        tps_write(0, TPS_SIZE, msg1);

        /* Move to main thread */
        sem_up(sem2);
        sem_down(sem1);
        tps_destroy();

        return NULL;
}

void test_clone_mem(void)
{
        char *buffer = malloc(TPS_SIZE);

        sem1 = sem_create(0);
        sem2 = sem_create(0);

        pthread_t tid;
        pthread_create(&tid, NULL, clone_mem_help, NULL);
        
        /* Move to helper thread */
        sem_down(sem2);

        tps_clone(tid);
        tps_read(0, TPS_SIZE, buffer);
        assert(!memcmp(buffer, msg1, TPS_SIZE));
        
        /* Collect helper thread */
        sem_up(sem1);
        pthread_join(tid, NULL);
        
        tps_destroy();
        sem_destroy(sem1);
        sem_destroy(sem2);

}

void *clone_priv_help(__attribute__((unused)) void *arg)
{
        tps_create();
        tps_write(0, TPS_SIZE, msg1);
        /* Move to main thread */
        sem_up(sem2);
        sem_down(sem1);
        
        char *buffer = malloc(TPS_SIZE);
        tps_read(0, TPS_SIZE, buffer);
        assert(!memcmp(buffer, msg1, TPS_SIZE));

        tps_destroy();
        free(buffer);

        return NULL;
}

void test_clone_privacy(void)
{
        sem1 = sem_create(0);
        sem2 = sem_create(0);

        pthread_t tid;
        pthread_create(&tid, NULL, clone_priv_help, NULL);
        
        /* Move to helper thread */
        sem_down(sem2);

        tps_clone(tid);
        tps_write(0, TPS_SIZE, msg2);
        
        /* Collect helper thread */
        sem_up(sem1);
        pthread_join(tid, NULL);
        
        tps_destroy();
        sem_destroy(sem1);
        sem_destroy(sem2);
}

int main(void)
{
        /* fault testing: pre-init */
        test_no_init_cd();
        test_no_init_wr();

        /* basic start tests */
        test_init();
        test_create();
        test_destroy();
        test_clone();

        /* begin fault testing the TPS */
        test_double_init();
        test_double_create();
        test_double_destroy();
        test_clone_notid();
        test_double_clone();
        test_invalid_offset();
        test_invalid_length();

        /* basic read and write tests */
        test_rw_basic();
        test_offset();
        test_rw_int();
        test_rw_float();

        /* clone functionality and privacy tests */
        test_clone_mem();
        test_clone_privacy();

        /* Testing segfaults */
        /* Create thread 1 and wait */
/*	pthread_create(&tid, NULL, thread1, NULL);
	pthread_join(tid, NULL);*/

        
        /*  segfault test  */
 /*       char *tps_addr = latest_mmap_addr;

        tps_addr[0] = ' ';*/

        return 0;
}
