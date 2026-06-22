#include "../include/spsc/queue.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

/// poc of a program that allocates a worst case (n) spsc q's
/// and can acquire to fill with tasks, execute those tasks in
/// and eventually release the queue back to the pool. the q's
/// are lock-free and benches well against q's w/ mutex's and cv's.


constexpr int         N     = 10'000'000;
constexpr std::size_t Q_CAP = 4096;
constexpr int         Q_MAX = 64;

struct Task {
	std::uint64_t id;
	std::function<void()> fn;
};

template<typename T, std::size_t q_cap>
struct QueuePool {
    alignas(spsc::Queue<T, q_cap>) 
		std::byte buf_[Q_MAX * sizeof(spsc::Queue<T, q_cap>)];

	spsc::Queue<T, q_cap>* free_[Q_MAX];
	std::size_t free_top_ = 0;

	QueuePool() {
		for (int i = 0; i < Q_MAX; i++) {
			spsc::Queue<T, q_cap>* q = 
				new (&buf_[i * sizeof(spsc::Queue<T, q_cap>)]) spsc::Queue<T, q_cap>;
			free_[free_top_++] = q;
		}
	}

	// in a real application we need a destructor to release queue resources

    spsc::Queue<T, q_cap>* acquire() {
		if (free_top_ == 0) {
			return nullptr;
		}
		return free_[--free_top_];
    }

    void release(spsc::Queue<T, q_cap>* value) {
		free_[free_top_++] = value;
    }
};

using Clock = std::chrono::steady_clock;

int main() {
	// pool holds Task by value, so each queue owns its storage; the
	// per queue inline buffer is large, so keep the pool off the stack.
	auto q_pool = std::make_unique<QueuePool<Task, Q_CAP>>();

	auto q = q_pool->acquire();
    std::chrono::time_point t0 = Clock::now();
    std::thread prod([&] {
		for (int i = 0; i < N;) {
			Task t;
			t.id = i;
			t.fn = [i]() {
				float r = static_cast<float>(rand()) / RAND_MAX;
				if (r >= 0.75) {
					// 25% chance of sleeping to simulate some i/o bound work
					std::this_thread::sleep_for(std::chrono::microseconds(1));
				}
				volatile uint64_t sink = 0;
				for (int j = 0; j < 100; j++) {
					sink += j * i;
				}
			};
			// push by move: try_push only takes ownership when there is
			// room, so a failed (full) push never touches shared storage.
			if (q->try_push(std::move(t))) ++i;
		}
    });

    std::uint64_t sum = 0;
    for (int n = 0; n < N;) {
		Task t;
		if (q->try_pop(t)) {
			sum += t.id;
			t.fn();
			++n;
		}
    }

    prod.join();
    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();

    std::printf("processed %d tasks in %.3f s  (%.1f M/s)  checksum=%llu\n",
                N, elapsed, N / elapsed / 1e6,
                (unsigned long long)sum);
}
