#pragma once

// SPSC, bounded, lock-free ring queue with small buffer optimization.
// capacity <= InlineCapacity -> inline storage, else one heap alloc up front

#include <atomic>
#include <cstddef>
#include <cstring>
#include <utility>

namespace spsc {
template <typename T, std::size_t InlineCapacity = 64>
class Queue {
	
	public:
		Queue(std::size_t capacity = InlineCapacity);
		~Queue();
		Queue(Queue&&);
		Queue& operator=(Queue&&);

		// delete copy and copy assignment constructors
		Queue(const Queue&) = delete;
		Queue& operator=(const Queue&) = delete;

		bool try_push(T value);
		bool try_pop(T& out);
		inline void reset() {
			std::size_t head = head_.load();
			std::size_t tail = tail_.load();
			std::byte* s = heap_ ? heap_ : inline_;
			for (std::size_t i = 0; i < (tail - head); i++) {
				std::size_t idx = ((head + i) % capacity_) * sizeof(T);
				reinterpret_cast<T*>(&s[idx])->~T();
			}
			head_.store(0);
			tail_.store(0);
		}

		std::size_t capacity() const { return capacity_; }
		bool is_inline() const { return heap_ == nullptr; }

	private:
		alignas(T) std::byte inline_[InlineCapacity * sizeof(T)];
		std::byte* heap_ = nullptr;
		std::size_t capacity_ = 0;

		alignas(64) std::atomic<std::size_t> head_{0};  // consumer
		alignas(64) std::atomic<std::size_t> tail_{0};  // producer
};

template <typename T, std::size_t InlineCapacity>
Queue<T, InlineCapacity>::Queue(std::size_t capacity) {
	// if capacity is greater than our threshold, use indirection
	// otherwise we benefit from locality
	if (capacity > InlineCapacity) {
		heap_ = new std::byte[capacity * sizeof(T)];
	}
	capacity_ = capacity;
}

template <typename T, std::size_t InlineCapacity>
Queue<T, InlineCapacity>::~Queue() {
	// load head and tail -- they let us check occupancy and 
	// index into our ring buffer
	std::size_t head = head_.load();
	std::size_t tail = tail_.load();
	std::byte *s = heap_ ? heap_ : inline_;

	for (std::size_t i = 0; i < (tail - head); i++) {
		std::size_t idx = ((head + i) % capacity_) * sizeof(T);
		reinterpret_cast<T*>(&s[idx])->~T();
	}
	delete[] heap_;
}

template <typename T, std::size_t InlineCapacity>
Queue<T, InlineCapacity>::Queue(Queue<T, InlineCapacity>&& value) {
	// move ctor assumes the queue has been reset().
	// move resources, memcpy inline raw bytes
	capacity_ = value.capacity_;
	heap_ = value.heap_;
	head_.store(value.head_.load());
	tail_.store(value.tail_.load());
	std::memcpy(inline_, value.inline_, sizeof(inline_));
	value.heap_ = nullptr;
}

template <typename T, std::size_t InlineCapacity>
Queue<T, InlineCapacity>& Queue<T, InlineCapacity>::operator=(Queue<T, InlineCapacity>&& value) {
	// move assignment assumes the queue is empty.
	// move resources, memcpy inline raw bytes to new value

	if (this == &value) return *this; // prevent self assignment

	capacity_ = value.capacity_;
	heap_ = value.heap_;
	head_.store(value.head_.load());
	tail_.store(value.tail_.load());
	std::memcpy(inline_, value.inline_, sizeof(inline_));
	value.heap_ = nullptr;
	return *this;
}

template <typename T, std::size_t InlineCapacity>
bool Queue<T, InlineCapacity>::try_push(T value) {
	// grab head and tail to determine occupancy
	std::size_t head = head_.load(std::memory_order_acquire);
	std::size_t tail = tail_.load(std::memory_order_relaxed);
	if ((tail - head) >= capacity_) {
		return false;
	}

	// grab offset to determine where to emplace new value
	std::byte *s = heap_ ? heap_ : inline_;
	std::size_t idx = (tail % capacity_) * sizeof(T);

	// construct T in place and increment producer
	new (&s[idx]) T(std::move(value));
	tail_.fetch_add(1, std::memory_order_release);

	return true;
}

template <typename T, std::size_t InlineCapacity>
bool Queue<T, InlineCapacity>::try_pop(T& out) {
	// grab head and tail to determine occupancy
	std::size_t head = head_.load(std::memory_order_relaxed);
	std::size_t tail = tail_.load(std::memory_order_acquire);
	if ((tail - head) == 0) {
		return false;
	}

	// grab consumer offset to determine which item to pop
	std::byte *s = heap_ ? heap_ : inline_;
	std::size_t idx = (head % capacity_) * sizeof(T);

	// cast T into rvalue for assignment, invoke dtor, increment consumer
	out = std::move(*reinterpret_cast<T*>(&s[idx]));
	reinterpret_cast<T*>(&s[idx])->~T();
	head_.fetch_add(1, std::memory_order_release);
	return true;
}

}