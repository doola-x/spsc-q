#include "../include/spsc/queue.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <queue>
#include <thread>

using Clock = std::chrono::steady_clock;
constexpr int N = 10'000'000;

// define two timed functions -- 
  // create a synced queue object
  // create N threads that push a value
  // loop N times to pop each values, increment once you have popped successfully
  // join all threads (they spin while size is bounded at capacity)
  // return time

double bench_spsc() {
	spsc::Queue<std::uint64_t, 4096> q(4096);
	std::thread prod([&] { 
		for (int i = 0; i < N;) {
			if (q.try_push(i)) ++i; 
		}
	});
	auto t0 = Clock::now();

	for (int n = 0; n < N;) { 
		std::uint64_t v; 
		if (q.try_pop(v)) ++n; 
	}

	prod.join();
	return std::chrono::duration<double>(Clock::now() - t0).count();
}

double bench_mutex() {
	std::queue<std::uint64_t> q;
	std::mutex m;
	
	std::thread prod([&] {
		for (int i = 0; i < N; ++i) { 
			std::lock_guard lk(m); 
			q.push(i); 
		}
	});
	auto t0 = Clock::now();
	
	for (int n = 0; n < N;) {
		std::lock_guard lk(m);
		if (!q.empty()) { q.pop(); ++n; }
	}

	prod.join();
	return std::chrono::duration<double>(Clock::now() - t0).count();
}

double bench_condvar() {
	std::queue<std::uint64_t> q;
	std::mutex m;
	std::condition_variable cv;
	
	std::thread prod([&] {
		for (int i = 0; i < N; ++i) {
			{
				std::lock_guard lk(m);
				q.push(i);
			}
			cv.notify_one();
		}
	});
	auto t0 = Clock::now();

	for (int n = 0; n < N; ++n) {
		std::unique_lock lk(m);
		cv.wait(lk, [&] { return !q.empty(); });
		q.pop();
	}

	prod.join();
	return std::chrono::duration<double>(Clock::now() - t0).count();
}

int main() {
	for (int i = 0; i < 2; i++) {
		// warm up
		bench_spsc();
		bench_mutex();
		bench_condvar();
	}
	double t1 = bench_spsc();
	double t2 = bench_mutex();
	double t3 = bench_condvar();
	std::printf("spsc::Queue         %.3f s  %.1f M/s\n", t1, N / t1 / 1e6);
	std::printf("std::queue+mutex    %.3f s  %.1f M/s\n", t2, N / t2 / 1e6);
	std::printf("std::queue+condvar  %.3f s  %.1f M/s\n", t3, N / t3 / 1e6);
}
