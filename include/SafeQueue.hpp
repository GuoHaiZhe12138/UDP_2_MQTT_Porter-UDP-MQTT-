#ifndef SAFE_QUEUE_HPP
#define SAFE_QUEUE_HPP

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

template <typename T>
class SafeQueue {
public:

    // 禁用拷贝构造和赋值
    SafeQueue(const SafeQueue&) = delete;
    SafeQueue& operator=(const SafeQueue&) = delete;

    // 禁止赋值构造
    explicit SafeQueue(size_t capacity) 
        : _capacity(capacity), _is_closed(false) 
    {

    }

    /**
     * 生产者调用：向队列压入任务
     * @return true 成功, false 队列已关闭
     */
    bool push(T&& value) {
        // 获取锁
        std::unique_lock<std::mutex> lock(_mtx);
        
        // 阻塞直到队列不满，或者队列关闭
        _cv_not_full.wait(lock, [this] {
            return _queue.size() < _capacity || _is_closed;
        });

        // 队列关闭时拒绝入队
        if (_is_closed) return false;

        _queue.emplace(std::forward<T>(value));
        
        // 唤醒消费者
        _cv_not_empty.notify_one();
        return true;
    }

    /**
     * 消费者调用：从队列取出任务
     * @return true 成功获取, false 队列已关闭且没有存货
     */
    bool pop(T& value) {
        // 获取锁
        std::unique_lock<std::mutex> lock(_mtx);

        // 阻塞直到队列不空，或者队列关闭
        _cv_not_empty.wait(lock, [this] {
            return !_queue.empty() || _is_closed;
        });

        // 即使关闭了，如果队列还有货，也得让消费者处理完
        if (_queue.empty() && _is_closed) {
            return false;
        }

        value = std::move(_queue.front());
        _queue.pop();

        // 关键点：出队后唤醒【可能在等待空位】的生产者
        _cv_not_full.notify_one();
        return true;
    }


    void close() {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _is_closed = true;
        }
        // 唤醒所有正在阻塞的生产者和消费者，让他们通过判断逻辑退出循环
        _cv_not_full.notify_all();
        _cv_not_empty.notify_all();
    }

private:
    std::queue<T>           _queue;         // 基础容器
    size_t                  _capacity;      // 最大容量
    std::mutex              _mtx;           // 互斥锁
    std::condition_variable _cv_not_empty;  // 消费者等待的条件
    std::condition_variable _cv_not_full;   // 生产者等待的条件
    bool                    _is_closed;     // 状态标记
};

#endif // SAFE_QUEUE_HPP
