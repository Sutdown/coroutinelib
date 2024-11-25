#ifndef FIBER_H
#define FIBER_H

#include <iostream>
#include <memory>
#include <atomic>
#include <functional>
#include <cassert>
#include <ucontext.h>
#include <unistd.h>
#include <mutex>

namespace colib
{
  class Fiber : public std::enable_shared_from_this<Fiber>
  {
  public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State
    {
      READY, // 就绪
      RUNNING, // 运行
      TERM // 结束
    };

  private:
    // 无参构造只能用于创建主函数对应的协程，因此只能是私有
    Fiber();

  public:
  // 创建用户协程
    Fiber(std::function<void()> cb,
          size_t stacksize = 0, bool run_in_scheduler = true);
    ~Fiber();
    
    // 重置协程状态和入口函数，复用栈空间，不重新创建栈
    void reset(std::function<void()> cb);

    void resume(); // 恢复协程运行
    void yield();  // 让出执行

    // 获取协程ID，协程状态
    uint64_t getId() const { return m_id; }
    State getState() const { return m_state; }
  
  public:
    // 设置正在运行的协程
    static void SetThis(Fiber *f);
    // 主协程
    static std::shared_ptr<Fiber> GetThis();

    // 协程调度器
    static void SetSchedulerFiber(Fiber *f);
    // 协程ID
    static uint64_t GetFiberId();
    // 协程入口函数
    static void MainFunc();

  private:
    uint64_t m_id = 0;
    State m_state = READY;

    ucontext_t m_ctx;

    uint32_t m_stacksize = 0;    // 栈大小
    void *m_stack = nullptr;    // 栈空间

    std::function<void()> m_cb; // 运行函数
    bool m_runInScheduler; // 是否参与协程调度器
  };
}
#endif