#include"scheduler.h"

static bool debug = false;

namespace colib{
  // 当前线程的调度器，同一个调度器下的所有线程指向同一个调度器实例
  static thread_local Scheduler *t_scheduler = nullptr; 

  /* 调度器的创建 */
  // 线程数，是否将当前线程作为调度线程
  // caller线程，调用线程，也就是主线程
  Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name){
    assert(threads > 0 && Scheduler::GetThis() == nullptr);

    SetThis();
    Thread::SetName(m_name);

    // 当前协程是否直接参与协程调度
    // 参与调度，也执行工作任务
    if(use_caller){
      threads--;
      Fiber::GetThis(); // 创建主协程

      // 创建调度协程
      m_schedulerFiber.reset(new Fiber(std::bind(&Scheduler::run, this)), 0, false);
      Fiber::SetSchedulerFiber(m_schedulerFiber.get());

      m_threadCount = Thread::GetThreadID();
      m_threadIDs.push_back(m_rootThread);
    }

    m_threadCount = threads;
    if (debug)
      std::cout << "Scheduler::Scheduler() success\n";
  }

  Scheduler::~Scheduler()
  {
    assert(stopping() == true);
    if (GetThis() == this) {
      t_scheduler = nullptr;
    }
    if (debug)
      std::cout << "Scheduler::~Scheduler() success\n";
  }

  Scheduler *Scheduler::GetThis(){
    return t_scheduler;
  }
  // 设置当前线程为公用的调度器实例
  void Scheduler::SetThis(){
    t_scheduler = this;
  }

  /* 调度器的启动 */
  // 初始化调度线程池
  void Scheduler::start() {

  }

  /* 调度协程的实现 */
  // 不停的从任务队列读取任务并执行
  //（Fiber类有三个协程上下文的保存，每个协程执行结束后都会回到调度协程）
  // 任务队列为空时，进行idle协程，也就是忙等其实，检测到停止时结束
  // 这个过程中，协程只要从resume返回应该会变成idle，因此不会被再次调度（猜的）
  void Scheduler::run() {

  }

  /* 调度器的停止*/
  // 调度器的stop，有caller）（待归纳）
  void Scheduler::stop() {

  }

}