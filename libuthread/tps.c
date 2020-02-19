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

/*
 * Size of a TPS area in bytes
 */
//#define TPS_SIZE 4096

struct tps {
        pthread_t tid;
        void *memarea;
};

queue_t tps_q = NULL;

int find_tps(void *data, void *arg)
{
        if (*((pthread_t *) arg) == ((struct tps *) data)->tid) {
                return 1;
        } else {
                return 0;
        }
}

/*
 * tps_init - Initialize TPS
 * @segv - Activate segfault handler
 *
 * Initialize TPS API. This function should only be called once by the client
 * application. If @segv is different than 0, the TPS API should install a
 * page fault handler that is able to recognize TPS protection errors and
 * display the message "TPS protection error!\n" on stderr.
 *
 * Return: -1 if TPS API has already been initialized, or in case of failure
 * during the initialization. 0 if the TPS API was successfully initialized.
 */
int tps_init(int segv)
{
        if (segv != 0) {
                segv = 0;
        }
	if (tps_q != NULL) {
                return -1;
        }

        tps_q = queue_create();
        
        if (tps_q == NULL) {
                return -1;
        }
        
        return 0;
}

/*
 * tps_create - Create TPS
 *
 * Create a TPS area and associate it to the current thread. The TPS area is
 * initialized to all zeros.
 *
 * Return: -1 if current thread already has a TPS, or in case of failure during
 * the creation (e.g. memory allocation). 0 if the TPS area was successfully
 * created.
 */
int tps_create(void)
{
	struct tps *new_tps = malloc(sizeof(struct tps));
        new_tps->tid = pthread_self();
        
        void *temp = NULL;
        queue_iterate(tps_q, &find_tps, &(new_tps->tid), &temp);
        if (temp != NULL) {
                free(new_tps);
                return -1;
        }

        new_tps->memarea = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE,
                MAP_ANON|MAP_PRIVATE, -1, 0);

        if (new_tps->memarea == NULL) {
                free(new_tps);
                return -1;
        }

        queue_enqueue(tps_q, (void *)new_tps);
        return 0;
}

int unmap_tps(void *data, void *arg)
{
        struct tps *curr_tps = data;
        if (*((pthread_t *) arg) == curr_tps->tid) {
                munmap(curr_tps->memarea, TPS_SIZE);
                return 1;
        } else {
                return 0;
        }
}

/*
 * tps_destroy - Destroy TPS
 *
 * Destroy the TPS area associated to the current thread.
 *
 * Return: -1 if current thread doesn't have a TPS. 0 if the TPS area was
 * successfully destroyed.
 */
int tps_destroy(void)
{
        pthread_t curr_tid = pthread_self();
	void *temp = NULL;
        queue_iterate(tps_q, &unmap_tps, &curr_tid, &temp);
        if (temp == NULL) {
                return -1;
        } else {
                queue_delete(tps_q, temp);
                return 0;
        }
}

/*
 * tps_read - Read from TPS
 * @offset: Offset where to read from in the TPS
 * @length: Length of the data to read
 * @buffer: Data buffer receiving the read data
 *
 * Read @length bytes of data from the current thread's TPS at byte offset
 * @offset into data buffer @buffer.
 *
 * Return: -1 if current thread doesn't have a TPS, or if the reading operation
 * is out of bound, or if @buffer is NULL, or in case of internal failure. 0 if
 * the TPS was successfully read from.
 */
int tps_read(size_t offset, size_t length, void *buffer)
{
	pthread_t curr_tid = pthread_self();
        struct tps *curr_tps = NULL;
        queue_iterate(tps_q, &find_tps, &curr_tid, (void **) &curr_tps);

        if (curr_tps == NULL || buffer == NULL) {
                return -1;
        }
        if (offset + length > TPS_SIZE) {
                return -1;
        }
        
        memcpy(buffer, curr_tps->memarea + offset, length);
        
        return 0;
}

/*
 * tps_write - Write to TPS
 * @offset: Offset where to write to in the TPS
 * @length: Length of the data to write
 * @buffer: Data buffer holding the data to be written
 *
 * Write @length bytes located in data buffer @buffer into the current thread's
 * TPS at byte offset @offset.
 *
 * If the current thread's TPS shares a memory page with another thread's TPS,
 * this should trigger a copy-on-write operation before the actual write occurs.
 *
 * Return: -1 if current thread doesn't have a TPS, or if the writing operation
 * is out of bound, or if @buffer is NULL, or in case of failure. 0 if the TPS
 * was successfully written to.
 */
int tps_write(size_t offset, size_t length, void *buffer)
{
	pthread_t curr_tid = pthread_self();
        struct tps *curr_tps = NULL;
        queue_iterate(tps_q, &find_tps, &curr_tid, (void **) &curr_tps);

        if (curr_tps == NULL || buffer == NULL) {
                return -1;
        }
        if (offset + length > TPS_SIZE) {
                return -1;
        }
        
        memcpy(curr_tps->memarea + offset, buffer, length);
        
        return 0;
}

/*
 * tps_clone - Clone TPS
 * @tid: TID of the thread to clone
 *
 * Clone thread @tid's TPS. In the first phase, the cloned TPS's content should
 * copied directly. In the last phase, the new TPS should not copy the cloned
 * TPS's content but should refer to the same memory page.
 *
 * Return: -1 if thread @tid doesn't have a TPS, or if current thread already
 * has a TPS, or in case of failure. 0 is TPS was successfully cloned.
 */
int tps_clone(pthread_t tid)
{
	struct tps *new_tps = malloc(sizeof(struct tps));
        new_tps->tid = pthread_self();
        
        void *temp = NULL;
        queue_iterate(tps_q, &find_tps, &(new_tps->tid), &temp);
        if (temp != NULL) {
                free(new_tps);
                return -1;
        }

        new_tps->memarea = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE,
                MAP_ANON|MAP_PRIVATE, -1, 0);

        if (new_tps->memarea == NULL) {
                free(new_tps);
                return -1;
        }

        struct tps *cpy_tps = NULL;
        queue_iterate(tps_q, &find_tps, &tid, (void **) &cpy_tps);

        if (cpy_tps == NULL) {
                return -1;
        }
        memcpy(new_tps->memarea, cpy_tps->memarea, TPS_SIZE);

        queue_enqueue(tps_q, (void *)new_tps);
        return 0;
}
