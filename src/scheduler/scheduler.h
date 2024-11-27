#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <vector>
#include <mutex>
#include <list>
#include "../fiber/fiber.h"
#include "../thread/thread.h"

  // 简单调度类，支持添加调度任务以及运行调度任务
  /*
  * FIFO
  * 多线程：
  ** 一个线程同时只能运行一个协程，所以
  ** 多线程意味着多个协程可以同时执行
  * 调度器：
  ** 调度协程池
  ** 添加调度任务
  ** 调度器停止
  */ 

 // 主协程（main），调度协程，任务协程
namespace colib{
  class Scheduler{
    public:
      Scheduler(size_t threads = 1, bool use_caller = true,
                const std::string &name = "Scheduler");
      virtual ~Scheduler();

      const std::string &getName() const { return m_name; }

    public:
      static Scheduler *GetThis();  // 获取正在运行的调度器
      static Fiber *GetMainFiber(); // 获取当前线程的主协程
    
    public:
      template<class FiberOrCb> // 协程对象or指针，线程号
      void scheduleLock(FiberOrCb fc,int thread=-1){
        bool need_tickle = false;
        {
          std::lock_guard<std::mutex> lock(m_mutex);
          need_tickle = m_tasks.empty();

          ScheduleTask task(fc, thread);
          if (task.fiber || task.cb)
          {
            m_tasks.push_back(task);
          }
        }

        if (need_tickle)
        {
          tickle(); // 唤醒idle协程
        }
      }

      void start();
      void stop();

    protected:
      void SetThis(); // 设置正在运行的调度器

      virtual void tickle() {}; // 通知协程调度器有任务了
      void run();               // 协程调度函数
      virtual void idle();      // 无任务执行idle协程

      virtual bool stopping();                                // 是否可以停止
      bool hasIdleThreads() { return m_idleThreadCount > 0; } // 是否有空闲协程

    private:
      // 调度任务，协程or函数
      struct ScheduleTask{
        std::shared_ptr<Fiber> fiber;
        std::function<void()> cb; // callback
        int thread;

        ScheduleTask(std::shared_ptr<Fiber> f,int thr) {
          fiber = f;
          thread = thr;
        }

        ScheduleTask(std::shared_ptr<Fiber> *f, int thr) {
          fiber.swap(*f);
          thread = thr;
        }

        ScheduleTask(std::function<void()> f, int thr) {
          cb = f;
          thread = thr;
        }

        ScheduleTask() {
          thread = -1;
        }

        void reset() {
          fiber = nullptr;
          cb = nullptr;
          thread = -1;
        }
      };

      private:
        std::string m_name; // 协程调度器名称

        std::mutex m_mutex;                             // 互斥锁
        std::vector<std::shared_ptr<Thread>> m_threads; // 线程池
        std::list<ScheduleTask> m_tasks;                // 任务队列
        std::vector<int> m_threadIDs;                   // 线程池的ID数组

        size_t m_threadCount = 0; // 工作线程数量
        std::atomic<size_t> m_activeThreadCount = {0}; // 活跃线程数
        std::atomic<size_t> m_idleThreadCount = {0};   // 空闲线程数

        bool m_useCaller;                        // 主线程是否有工作线程
        std::shared_ptr<Fiber> m_schedulerFiber; // 额外创建调度协程
        int m_rootThread = -1;                   // 主线程的线程id

        bool m_stopping = false; // 是否正在关闭
  };
}

#endif