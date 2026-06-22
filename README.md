# spsc-q

## MAKEFILE
Use `make build` and `make bench` to run this application.

## include/spsc/queue.hpp
Single-producer / single-consumer bounded lock-free queue with a small buffer
optimization. Built for trading-style hot paths: preallocate everything up
front, then push/pop in a tight loop with no locks and no per-op allocation.

## bench/bench_queue.cpp
Throughput comparison of three single-producer / single-consumer queues, each
moving `N` `uint64_t` values from a producer thread to a consumer (the main
thread) and timing the round trip:

- `bench_spsc` — the lock-free `spsc::Queue`; producer spins on `try_push`,
  consumer spins on `try_pop`, no locks and no per-op allocation.
- `bench_mutex` — a `std::queue` guarded by a `std::mutex`, taking the lock on
  every push and pop.
- `bench_condvar` — a `std::queue` with a `std::mutex` plus a
  `std::condition_variable` the consumer waits on.

`main()` runs all three and prints elapsed seconds and millions of ops/sec for
each, so the lock-free queue can be compared against the lock-based baselines.

## src/main.cpp
POC style application that creates a few additional custom objects, `QueuePool`
and `Task`. `QueuePool` allocates a worst case `Q_MAX` amount of `spsc::Queue<T, CAPACITY>`s
which can be `acquire()`d and `release()`d back to the pool. The pool maintains a 
simple free list to facilitate this. `Task` is a simple struct that holds a `uint64_t`
and a `std::function` object to represent work.

In our `main()` definition, similar to `bench_queue.cpp`, we create a queue, fill
it with N `Task` definitions that do some simulated CPU and I/O bound work in a 
separate thread. We then immediately attempt to call `try_pop` on all pushed items,
to create some contention between producer, consumer, and ring buffer capacity. 
This allows us to see the benefits of lock free paradigms. In a real application, 
all of this work could be dispatched and the main processing thread could dispatch 
separate queues of work items.
