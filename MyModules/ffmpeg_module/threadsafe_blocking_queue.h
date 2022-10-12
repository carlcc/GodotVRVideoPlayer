#pragma once

#include <condition_variable>
#include <list>
#include <mutex>

template <typename E>
class ThreadSafeBlockingQueue {
public:
    explicit ThreadSafeBlockingQueue(size_t maxSize)
        : maxSize_ { maxSize }
    {
    }
    ~ThreadSafeBlockingQueue() { close(); }

    bool push(E e)
    {
        {
            std::unique_lock<std::mutex> lck(mutex_);
            hasSpaceCv_.wait(lck, [this]() { return queue_.size() < maxSize_ || isClosed_; });
            if (isClosed_) {
                return false;
            }
            queue_.push_back(std::move(e));
        }
        hasElementCv_.notify_one();
        return true;
    }

    bool pop(E& e)
    {
        {
            std::unique_lock<std::mutex> lck(mutex_);
            hasElementCv_.wait(lck, [this]() { return !queue_.empty() || isClosed_; });
            if (isClosed_) {
                return false;
            }
            e = std::move(queue_.front());
            queue_.pop_front();
        }
        hasSpaceCv_.notify_one();
        return true;
    }

    const E& peekFront() const
    {
        assert(queue_.size() > 0);
        return queue_.front();
    }

    bool isEmpty() const { return size() == 0; }

    size_t size() const
    {
        std::unique_lock<std::mutex> lck(mutex_);
        return queue_.size();
    }

    void close()
    {
        {
            std::unique_lock<std::mutex> lck(mutex_);
            isClosed_ = true;
        }
        hasElementCv_.notify_all();
        hasSpaceCv_.notify_all();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lck(mutex_);
        queue_.clear();
    }

private:
    mutable std::mutex mutex_ {};
    std::condition_variable hasElementCv_ {};
    std::condition_variable hasSpaceCv_ {};
    std::list<E> queue_ {};
    size_t maxSize_ { 1 };
    bool isClosed_ { false };
};