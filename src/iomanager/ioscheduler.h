#ifndef IOMANAGER_H
#define IOMANAGER_H

#include "../scheduler/scheduler.h"
#include "../timer/timer.h"

namespace colib{
  /*
  * 注册事件
  * 等待就绪
  * 调度回调函数
  * 退出事件
  * 运行回调函数
  */
  class IOManager : public Scheduler, public TimerManager
  {
  public:
    enum Event
    {
      NONE = 0x0,
      READ = 0x1, // EPOLLIN
      WRITE = 0x4 // EPOLLOUT
    };
  
  private:
    struct FdContext{
      struct EventContext{
        Scheduler *scheduler = nullptr; // 调度器
        std::shared_ptr<Fiber> fiber;   // 协程
        std::function<void()> cb;       // 回调函数
      };

      EventContext read;
      EventContext write;
      int fd = 0;
      Event events = NONE; // events registered
      std::mutex mutex;

      EventContext &getEventContext(Event event);
      void resetEventContext(EventContext &ctx);
      void triggerEvent(Event event);
    };
  
  public:
    // 改造协程调度器，使其支持epoll,重载tickle和idle
    // 实现通知调度协程和IO协程调度功能
    IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");
    ~IOManager();

    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);
    bool cancelAll(int fd);

    static IOManager *GetThis();

  protected:
    void tickle() override;
    bool stopping() override;
    void idle() override;
    void onTimerInsertedAtFront() override;

    void contextResize(size_t size);

  private:
    int m_epfd = 0;     // epoll文件描述符，监视文件描述符的状态变化
    int m_tickleFds[2]; // fd[0]=read fd[1]=write，用于触发事件的管道文件描述符数组
    std::atomic<size_t> m_pendingEventCount = {0}; // 待处理的事件数
    std::shared_mutex m_mutex;                     // 共享锁
    std::vector<FdContext *> m_fdContexts;         // 存储待处理的文件描述符和相关上下文信息
  };
}

#endif