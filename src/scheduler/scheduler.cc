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

    // 主线程参与调度
    if(use_caller){
      threads--;
      Fiber::GetThis(); // 创建主协程

      // 创建调度协程
      m_schedulerFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false));
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
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_stopping){
      std::cerr << "Scheduler is stopped" << std::endl;
      return;
    }

    assert(m_threads.empty());
    m_threads.resize(m_threadCount);
    for (size_t i = 0; i < m_threadCount;i++){
      m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
      m_threadIDs.push_back(m_threads[i]->getID());
    }
    if (debug)
      std::cout << "Scheduler::start() success\n";
  }

  /* 调度协程的实现 */
  // 不停的从任务队列读取任务并执行
  //（Fiber类有三个协程上下文的保存，每个协程执行结束后都会回到调度协程）
  // 任务队列为空时，进行idle协程，也就是忙等其实，检测到停止时结束
  // 这个过程中，协程只要从resume返回应该会变成idle，因此不会被再次调度（猜的）
  void Scheduler::run() {
    int thread_id = Thread::GetThreadID();
    if (debug)
      std::cout << "Schedule::run() starts in thread: " << thread_id << std::endl;
    SetThis();
    // 如果当前线程不为主线程，那么需要创建
    if(thread_id!=m_rootThread){
      Fiber::GetThis();
    }

    // idle coroutine 这个线程为什么类似于一直处于忙等的状态
    std::shared_ptr<Fiber> idle_fiber = std::make_shared<Fiber>(std::bind(&Scheduler::idle, this));
    ScheduleTask task;

    while (true)
    {
      task.reset();
      bool tickle_me = false;

      {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.begin();
        // 遍历任务队列
        while(it!=m_tasks.end()){
          if(it->thread!=-1&&it->thread!=thread_id){
            it++;
            tickle_me = true;
            continue;
          }

          // 取出任务
          assert(it->fiber || it->cb);
          task = *it;
          m_tasks.erase(it);
          m_activeThreadCount++;
          break;
        }
        tickle_me = tickle_me || (it != m_tasks.end());
      }
      // ...?
      if(tickle_me){
        tickle();
      }

      // 执行任务
      if(task.fiber){
        {
          std::lock_guard<std::mutex> lock(task.fiber->m_mutex);
          if(task.fiber->getState()!=Fiber::TERM){
            task.fiber->resume();
          }
        }
        m_activeThreadCount--;
        task.reset();
      }else if(task.cb){
        std::shared_ptr<Fiber> cb_fiber = std::make_shared<Fiber>(task.cb);
        {
          std::lock_guard<std::mutex> lock(cb_fiber->m_mutex);
          cb_fiber->resume();
        }
        m_activeThreadCount--;
        task.reset();
      }else{
        // 没有任务，执行空闲协程
        // 系统关闭 -> idle协程将从死循环跳出并结束 -> 
        // 此时的idle协程状态为TERM -> 再次进入将跳出循环并退出run()
        if(idle_fiber->getState()==Fiber::TERM){ // 也会退出整个调度
          if (debug)
            std::cout << "Schedule::run() ends in thread: " << thread_id << std::endl;
          break;
        }
        m_idleThreadCount++;
        idle_fiber->resume(); // yield,调度器检测到停止，idle会结束
        m_idleThreadCount--;
      }
    }
  }

  /* 调度器的停止*/
  // 调度器的stop，有caller）（待归纳）
  void Scheduler::stop() {
    if (debug)
      std::cout << "Schedule::stop() starts in thread: " << Thread::GetThreadID() << std::endl;

    if (stopping()){
      return;
    }

    m_stopping = true;

    if (m_useCaller) {
      assert(GetThis() == this);
    } else {
      assert(GetThis() != this);
    }

    for (size_t i = 0; i < m_threadCount; i++) {
      tickle();
    }

    if (m_schedulerFiber) {
      tickle();
    }

    if (m_schedulerFiber) {
      m_schedulerFiber->resume();
      if (debug)
        std::cout << "m_schedulerFiber ends in thread:" << Thread::GetThreadID() << std::endl;
    }

    // 巧妙的释放vector，好处是可以保障线程安全
    std::vector<std::shared_ptr<Thread>> thrs;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      thrs.swap(m_threads);
    }
    for (auto &i : thrs) {
      i->join(); // 阻塞当前进程，直到所有进程结束，确保主线程不会在子线程完成前退出
    }

    if (debug)
      std::cout << "Schedule::stop() ends in thread:" << Thread::GetThreadID() << std::endl;
  }

  bool Scheduler::stopping(){
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stopping && m_tasks.empty() && m_activeThreadCount == 0;
  }

  void Scheduler::idle() {
    while(!stopping()){
      if (debug)
        std::cout << "Scheduler::idle(), sleeping in thread: " << Thread::GetThreadID() << std::endl;
      sleep(1);
      Fiber::GetThis()->yield();
    }
  }
}