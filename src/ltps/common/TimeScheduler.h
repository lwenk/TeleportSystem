#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace ltps {

/**
 * @brief 通用时间调度器（TimeScheduler）
 * 任务需包含 getExpireTime() 方法，返回 std::chrono::time_point。
 * Compare 用于定义堆中优先级（默认为最小时间在顶）。
 *
 * 用法示例：
 * struct Task {
 *     std::chrono::steady_clock::time_point expire;
 *     std::chrono::steady_clock::time_point getExpireTime() const { return expire; }
 * };
 *
 * ltps::TimeScheduler<std::shared_ptr<Task>> scheduler;
 * scheduler.start();
 * scheduler.add(task);
 */
template <typename Ty, class Compare = std::greater<Ty>>
class TimeScheduler {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Callback  = std::function<void(std::shared_ptr<Ty> const&)>;

    static_assert(
        std::is_invocable_r_v<TimePoint, decltype(&Ty::getExpireTime), Ty const*>,
        "Type Ty must provide getExpireTime() -> std::chrono::time_point"
    );

private:
    std::priority_queue<std::shared_ptr<Ty>, std::vector<std::shared_ptr<Ty>>, Compare> mQueue;

    std::mutex              mMutex;
    std::condition_variable mCv;
    std::atomic<bool>       mAbort{false};
    std::thread             mWorker;
    Callback                mOnExpire;

public:
    TimeScheduler() = default;
    ~TimeScheduler() { stop(); }

    void setExpireCallback(Callback cb) { mOnExpire = std::move(cb); }

    void start() {
        mAbort.store(false);
        mWorker = std::thread([this] { workerLoop(); });
    }

    void stop() {
        mAbort.store(true);
        mCv.notify_all();
        if (mWorker.joinable()) mWorker.join();
    }

    void add(std::shared_ptr<Ty> const& item) {
        {
            std::lock_guard lock(mMutex);
            mQueue.push(item);
        }
        mCv.notify_all();
    }

private:
    void workerLoop() {
        using namespace std::chrono;
        std::unique_lock lock(mMutex);

        while (!mAbort.load()) {
            if (mQueue.empty()) {
                mCv.wait_for(lock, 1s);
                continue;
            }

            auto now = std::chrono::steady_clock::now();

            while (!mQueue.empty() && mQueue.top()->getExpireTime() <= now) {
                auto item = mQueue.top();
                mQueue.pop();
                lock.unlock();
                if (mOnExpire) mOnExpire(item);
                lock.lock();
            }

            if (!mQueue.empty()) {
                mCv.wait_until(lock, mQueue.top()->getExpireTime());
            }
        }
    }
};

} // namespace ltps
