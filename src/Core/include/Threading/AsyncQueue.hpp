#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>

namespace ettycc
{
    // ── AsyncQueue ───────────────────────────────────────────────────────────────
    // Lock-free Single-Producer Single-Consumer bounded ring buffer.
    //
    // Use cases:
    //   • Network thread  → main thread  (inbound transform packets)
    //   • Main thread     → network thread (outbound broadcasts)
    //   • Physics worker  → main thread  (future results)
    //
    // Capacity MUST be a power of two (compile-time enforced).
    // Head/tail indices are cache-line aligned to avoid false sharing.
    template <typename T, size_t Capacity>
    class AsyncQueue
    {
        static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                      "AsyncQueue capacity must be a power of two");

    public:
        // Try to enqueue an item. Returns false if the queue is full (back-pressure).
        bool TryPush(const T& item)
        {
            const size_t head = head_.load(std::memory_order_relaxed);
            const size_t next = (head + 1) & kMask;
            if (next == tail_.load(std::memory_order_acquire))
                return false; // full
            buffer_[head] = item;
            head_.store(next, std::memory_order_release);
            return true;
        }

        bool TryPush(T&& item)
        {
            const size_t head = head_.load(std::memory_order_relaxed);
            const size_t next = (head + 1) & kMask;
            if (next == tail_.load(std::memory_order_acquire))
                return false;
            buffer_[head] = std::move(item);
            head_.store(next, std::memory_order_release);
            return true;
        }

        // Try to dequeue an item. Returns std::nullopt if empty.
        std::optional<T> TryPop()
        {
            const size_t tail = tail_.load(std::memory_order_relaxed);
            if (tail == head_.load(std::memory_order_acquire))
                return std::nullopt; // empty
            T item = std::move(buffer_[tail]);
            tail_.store((tail + 1) & kMask, std::memory_order_release);
            return item;
        }

        // Drain all available items into the provided callback. Returns count drained.
        // Usage:  queue.DrainInto([](TransformPacket&& pkt) { apply(pkt); });
        template <typename Fn>
        size_t DrainInto(Fn&& fn)
        {
            size_t count = 0;
            while (auto item = TryPop())
            {
                fn(std::move(*item));
                ++count;
            }
            return count;
        }

        bool Empty() const
        {
            return head_.load(std::memory_order_acquire)
                == tail_.load(std::memory_order_acquire);
        }

        size_t ApproxSize() const
        {
            const size_t h = head_.load(std::memory_order_relaxed);
            const size_t t = tail_.load(std::memory_order_relaxed);
            return (h - t) & kMask;
        }

    private:
        static constexpr size_t kMask = Capacity - 1;

        std::array<T, Capacity>         buffer_{};
        alignas(64) std::atomic<size_t> head_{0};
        alignas(64) std::atomic<size_t> tail_{0};
    };

} // namespace ettycc
