# ECS 150 Project 3 - User-Level Thread Library (Part 2)
**Daniel Ritchie and Peregrine Rolland**

## Phase 1 - Semaphores
Each semaphore is uses a count to measure its number of available resources
and a queue pointer so that each semaphore can have it's own dynamically
allocated queue to serve as a blocked queue when the count is 0.

Sem_create() and sem_destroy() initialize, allocate, and deallocate memory
associated with the semaphore. These functions make use of the queue_create()
and queue_destroy() functions from the queue API. 

Sem_down() decrements the semaphore counter, but while count of the semaphore
is 0 the function will put the current thread on the blocked thread queue via
the enqueue() function defined in the queue API. 

Sem_up() increments the semaphore counter. If the semaphore currently has a 
blocked thread, the function will return the thread to a ready state by using
the dequeue() function. It will then free the memory associated with that
element in the queue.

#### Semaphore Edge Case
We solve the edge case of Thread C snatching a resource before thread A can
run but after it has been blocked by handling both possible outcomes
seperately. We began by checking if the thread can run in a while loop.
This way even if thread A has been unblocked, if by they time thread A
gets to run it's resource has been taken it will once again block itself. 

## Phase 2 - Thread Protected Storage
Each TPS uses a pthread_t to keep track of which Thread the TPS
belongs to. Each TPS also has a mempage struct that wraps the pointer to the
actual data.

The mempage structure is made up of a reference counter, used to count how
many TPS's are currently pointing to this specific memarea. Each memarea also
has a void pointer called mempage to point to the memory mapped to the thread.

We used a queue to store the TPS structs, because the queue is already provided
to us and is more flexible than an array.

We have two main helper functions, find_tps() and find_by_memarea(). They both
search the queue, but for different TPS values. find_tps() searches by TID, and
find_by_memarea() searches by memory mapped pages. The latter is useful for the
signal handler that will be described below.

tps_init() has two roles: first, it initializes the queue. Secondly, it sets up
the SIGSEGV and SIGBUS signal handler. The signal handlers uses the
find_by_memarea() function described above to find out if the segfault
originated from a thread using the TPS API. It will print an error message in
this case, and send the signal on no matter what.

tps_create() allocates a TPS struct for the current thread, and adds it to the
queue of TPS's if it does not already exist in the queue. mmap() is then called
to reserve a page of memory. We then use memset() to set the data block
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
Our tps_clone() function implements copy-on-write functionality by initializing
a new TPS (similar to tps_create()), but instead of using mmap to reserve a new
page in memory, it points to the memory page of the TPS it is cloning. The
mempage's count is then incremented so that the program knows that particular
mempage has multiple TPS's that can read from it.

When writing, if the reference counter is greater than 1, we create an new
memarea with a reference counter of 1 (as it is about to become unique), and
a new mempage. We then copy all the information from the previous
memearea using mem_read() to our new mempage and then make the changes as
expected. We used mem_read() to avoid code copy+pasting. Finally, the original
memarea will have it's number of references decremented. This allows multiple
threads to copy the same tps but only one thread at a time will create it's
own unique copy.

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
for multiple data types including char's, ints, and floats. The threads
were also able to read and write with an offset.

We tested cloning, using semaphores to manage each thread's behavior. We
began testing the cloning behavior, asserting that one or more clones would
read the same data. These clones then showed copy-on-write functionality,
creating 2 unique memareas when a write was completed. The cloning test ensures
that a new mempage is created *only* when a mempage is written to, not when it
is read. 

Finally we added a segfault error to test that accessing a private memory
space would not be allowed and would properly throw a seg fault.
