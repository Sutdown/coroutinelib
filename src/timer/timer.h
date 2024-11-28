#ifndef TIMER_H
#define TIMER_H

#include <memory>
#include <vector>
#include <set>
#include <shared_mutex>
#include <assert.h>
#include <functional>
#include <mutex>

namespace colib
{
  class TimerManager;

  class Timer : public std::enable_shared_from_this<Timer>
  {
    friend class TimerManager;

  public:
    bool cancel();                          // 定时器取消
    bool refresh();                         // 定时器刷新
    bool reset(uint64_t ms, bool from_now); // 重置定时器时间

  private:
    // 构造函数
    /*
     * ms 定时器执行间隔时间，cb 回调函数
     * recurring 是否循环，manager 定时器管理器
     */
    Timer(uint64_t ms, std::function<void()> cb,
          bool recurring, TimerManager *manager);

  private:
    bool m_recurring = false; // 是否循环
    uint64_t m_ms = 0;        // 执行周期

    std::chrono::time_point<std::chrono::system_clock> m_next; // 绝对超时时间
    std::function<void()> m_cb;                                // 回调函数

    TimerManager *m_manager = nullptr; // 定时器管理器

  private:
    // 最小堆的比较函数
    struct Comparator
    {
      bool operator()(const std::shared_ptr<Timer> &lhs,
                      const std::shared_ptr<Timer> &rhs) const;
    };
  };

  class TimerManager
  {
    friend class Timer;

  public:
    TimerManager();
    virtual ~TimerManager();

    // 添加定时器
    std::shared_ptr<Timer> addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);

    // 添加条件定时器
    std::shared_ptr<Timer> addConditionTimer(uint64_t ms, std::function<void()> cb,
                                             std::weak_ptr<void> weak_cond, bool recurring = false);

    // 找到最近的超时时间
    uint64_t getNextTimer();

    // 取出所有超时定时器的回调函数
    void listExpiredCb(std::vector<std::function<void()>> &cbs);

    // 堆中是否有定时器
    bool hasTimer();

  protected:
    // 当一个最早的定时器加入堆中时调用
    virtual void onTimerInsertedAtFront() {};
    // 添加定时器
    void addTimer(std::shared_ptr<Timer> timer);

  private:
    // 系统时间改变时调用该函数
    bool detectClockRollover();

  private:
    std::shared_mutex m_mutex;
    // 时间堆
    std::set<std::shared_ptr<Timer>, Timer::Comparator> m_timers;
    // 在下次getNextTime()执行前
    // onTimerInsertedAtFront()是否已经被触发了
    // -> 在此过程中 onTimerInsertedAtFront()只执行一次
    bool m_tickled = false;
    // 上次检查系统时间是否回退的绝对时间
    std::chrono::time_point<std::chrono::system_clock> m_previouseTime;
  };

}

#endif