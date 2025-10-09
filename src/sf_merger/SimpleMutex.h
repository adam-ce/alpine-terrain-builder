#include <mutex>
#include <utility>

template <typename T>
class SimpleMutex {
public:
    SimpleMutex() = default;

    explicit SimpleMutex(T value) : value_(std::move(value)) {}

    // Disable copy and move to avoid ownership issues
    SimpleMutex(const SimpleMutex &) = delete;
    SimpleMutex &operator=(const SimpleMutex &) = delete;

    // Access wrapper: provides scoped locking and reference
    class LockGuard {
    public:
        LockGuard(std::mutex &mutex, T &value)
            : lock_(mutex), ref_(value) {}

        T &get() {
            return ref_;
        }
        T *operator->() {
            return &ref_;
        }
        T &operator*() {
            return ref_;
        }

    private:
        std::unique_lock<std::mutex> lock_;
        T &ref_;
    };

    // Lock and return access to the inner value
    LockGuard lock() {
        return LockGuard(mutex_, value_);
    }

private:
    T value_;
    mutable std::mutex mutex_;
};
