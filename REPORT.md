# ECS 150 Project 3 - User-Level Thread Library (Part 2)
**Daniel Ritchie and Peregrine Rolland**

## Phase 1 - Semaphores
Each semaphore is uses a count to measure its number of available resources
and a queue pointer so that each semaphore can have it's own dynamically
allocated queue to serve as a blocked queue when the count is 0. 

Sem_create() and sem_destroy() are used to allocate and deallocate memory
respectively. These functions make use of the queue_create() and
queue_destroy() functions from the queue API. 

Sem_down() is used by a thread to symbolically grab a resouce. When this
function is called while count of the semaphore is 0 the function will put
the current thread on the blocked thread queue via the enqueue() function
defined in the queue API. 

Sem_up() is called to signify that more resouces are available and will
increment the count accordingly. If the semaphore currently has a blocked
thread, the function will return the thread to a ready state by using the
dequeue() function. 

#### Semaphore Edge Case
We solve the edge case of Thread C snatching a resource before thread A can
run but after it has been blocked by handling both possible outcomes
seperately. We began by checking if the thread can run in a while loop.
This way even if theread A has been unblocked, if by they time thread A
gets to run it's resource has been taken it will once again block itself. 


## Phase 2 - Thread Protected Storage
Each TPS uses a pthread_t tid to keep track of which Thread ID the TPS
belongs to and memarea strcture to contain the actual data.

The memarea structure is made up of a reference counter, used to count how
many TPS's are currently pointing to this specific memarea. This is used
when other threads clone a certain thread's TPS. Each memarea also has a
void pointer called mempage to point to the space in memory used by the
thread.

tps_init() is used to assign a signal handler, which protects each TPS and
assures the privacy of each TPS by throwing error signals to a defined
signal handler. This handler will print an error message and quit the
program. We also use tps_init to create a queue used to hold all the tps so
that we can find each one later with queue_iterate.

tps_create() creates a TPS for the current thread, assigning the TID and
allocates space for the memarea. mmap() is then called to reserve a page of
memory pointed to  the mempage. We then use memset() to set the data block
to all 0s. Finally we set the permissions to None to assure privacy for the
thread. The new tps is then enqueue'd to the tps queue.

tps_destroy() uses queue iterate to find the desired tps in the tps queue,
unmap the resereved memory with the munmap() fuction, remove the tps from
the queue and finally free all space associated with the tps. This function
also checks that there are no other threads cloning this tps before it is
deleted. If there are other threads, it simply decrements the reference 
counter.


tps_read() uses the mprotect() function to change the protection flags of
the current thread's mempage to allow readability. memcpy() is used to read
the information from the TPS and copy the information to a buffer passed in
as an argument. Finally mprotect() is once again used to change the
protection flags back.

tps_write() uses the mprotect() function to change the protection flags of
the current thread's mempage to allow writing. memcpy() is used to read
the information from buffer, passed as an argument, and copy the information
to our tps's memepage. Finally mprotect() is once again used to change the
protection flags back. Tps is also in charge of creating a new mempage in
the case of cloning, this is explained bellow. 

#### Cloning the TPS
Cloning the TPS uses copy-on-write funtionality. Cloning a TPS creates a
new TPS with the TID matching the current thread's TIP but the memarea
shares the address of the TPS the current thread is trying to clone. This
will then increment the reference counter by 1.

When writing, if the reference counter is greater than 1, we create an new
memarea with a refernce counter of 1 (as it is about to become unique), and
a new mempage. We then copy all the information from the previous
memearea using mem_read() to our new mempage and then make the changes as
expected. Finally, the original memarea will have it's number of references
decremented. This allows multiple threads to copy the same tps but only one
thread at a time will create it's own unique copy.

#### Critical Sections
Critical sections are used all throughout sem.c and tps.c. We use the
enter_critical_section() function before allocation or freeing memory,
modifying any global variables, etc. We then make sure to exit the critical
section once all critical behavior is finished and/ or right before exiting the
function in the case of any errors. 


## Testing
Testing through the file tps_tester.c was achieved using the assert()
function. We created many fault tests, making sure all functions returned
the desired inputs when the correct conditions weren't met or when a faulty
input was passed in. 

Next we did basic read and write tests, making sure the memory was suitable
for multiple data types including char*s, ints, and floats. The threads
were also able to read and write with an offset.

We tested cloning, using semaphores to manage each thread's behavior. We
began testing the cloning behavior, asserting that one or more clones would
read the same data. These clones then showed copy-on-write functionality,
creating 2 unique memareas when a write was completed. 
Finally we added a segfault error to test that accessing a private memory
space wouldd not be allowed and would properly throw a seg fault.