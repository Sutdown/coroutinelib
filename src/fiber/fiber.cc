#include "fiber.h"

static bool debug = false;

namespace colib
{
  /* 一个协程最多知到两个协程的上下文*/
  // 线程局部变量，当前线程正在运行的协程
  static thread_local Fiber *t_fiber = nullptr;
  // 线程局部变量，当前线程的主协程，切换到这个协程，相当于切换到主线程
  static thread_local std::shared_ptr<Fiber> t_thread_fiber = nullptr;

  static std::atomic<uint64_t> s_fiber_id{0};
  static std::atomic<uint64_t> s_fiber_count{0};

  void Fiber::SetThis(Fiber *f)
  {
    t_fiber = f;
  }

  /* 
  * 线程创建协程时，初始化主函数协程
  * 这个函数用于返回当前线程正在执行的协程 
  * details
  * 如果当前存在正在运行的协程，
  * 那么直接为其加上shared_ptr的特点化身为主协程
  * 没有则创建新的协程
  * 注意调度器和主协程存在密切关系，需要一同赋值
  */
  std::shared_ptr<Fiber> Fiber::GetThis()
  {
    if(t_fiber){
      return t_fiber->shared_from_this();
    }

    std::shared_ptr<Fiber> main_fiber(new Fiber());
    t_thread_fiber = main_fiber;

    assert(t_fiber == main_fiber.get());
    return t_fiber->shared_from_this();
  }

  uint64_t Fiber::GetFiberId()
  {
    if(t_fiber){
      return t_fiber->getId();
    }
    return (uint64_t)-1;
  }

  /* 
  * 协程的构建，需要分配栈内存空间
  * 子协程需要初始化上下文和栈空间，要求传入协程的入口函数，可选协程栈大小
  */
  // 主协程
  Fiber::Fiber()
  {
    SetThis(this);
    m_state = RUNNING;

    if(getcontext(&m_ctx)){
      std::cerr << "Fiber() failed\n";
      pthread_exit(NULL);
    }

    m_id = s_fiber_id++;
    s_fiber_count++;

    if (debug) std::cout << "Fiber(): main id = " << m_id << std::endl;
  }

  /*
   * 用户协程的创建
   * 分配栈空间，默认128kb
   * 初始化协程上下文，设置栈和入口函数
  */
  Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler)
      : m_cb(cb), m_runInScheduler(run_in_scheduler)
  {
    m_state = READY;

    // 栈大小默认128k
    m_stacksize = stacksize ? stacksize : 128000;
    m_stack = malloc(m_stacksize);

    // 初始化下文：
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);

    m_id = s_fiber_id;
    s_fiber_count++;
    if (debug)
      std::cout << "Fiber(): child id = " << m_id << std::endl;
  }

  Fiber::~Fiber(){
    s_fiber_count--;
    if (m_stack)
    {
      free(m_stack);
    }
    if (debug)
      std::cout << "~Fiber(): id = " << m_id << std::endl;
  }

  // 结束的协程可以重置
  void Fiber::reset(std::function<void()> cb)
  {
    assert(m_stack != nullptr && m_state == TERM);

    m_state = READY;
    m_cb = cb;

    if(getcontext(&m_ctx)){
      std::cerr << "reset() failed\n";
      pthread_exit(NULL);
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;
    makecontext(&m_ctx, &Fiber::MainFunc, 0);
  }

  /*
  * 协程的恢复
  * 将状态从READY转换到RUNNING
  * 根据是否参与调度器，选择上下文切换到调度器或者主线程
  */ 
  void Fiber::resume()
  {
    assert(m_state == READY);

    m_state = RUNNING;

      // 不参与协程调度器
    SetThis(this);
    if (swapcontext(&(t_thread_fiber->m_ctx), &m_ctx))
    {
      std::cerr << "resume() to t_thread_fiber failed\n";
      pthread_exit(NULL);
    } 
  }

  /*
   * 协程的让出
   * yield 将协程从 RUNNING 切换回 READY（或 TERM），
   * 然后切换回调用方上下文（主线程或调度器）。
   */
  void Fiber::yield()
  {
    assert(m_state == RUNNING || m_state == TERM);
    SetThis(t_thread_fiber.get());
    if(m_state!=TERM){
      m_state = READY;
    }

    if (swapcontext(&m_ctx, &(t_thread_fiber->m_ctx)))
    {
        std::cerr << "yield() to t_thread_fiber failed\n";
        pthread_exit(NULL);
    }
  }

  /*
  * 协程的入口函数
  * 执行用户定义的回调函数。
  * 协程任务完成后，状态置为 TERM，
  * 并主动调用 yield 切换回调用方上下文。
  */
  void Fiber::MainFunc()
  {
    std::shared_ptr<Fiber> curr = GetThis();
    assert(curr != nullptr);

    curr->m_cb();
    curr->m_cb = nullptr;
    curr->m_state = TERM;

    // 运行完毕，让出执行权
    auto raw_ptr = curr.get();
    curr.reset();
    raw_ptr->yield(); // 协程结束自动退出
  }
}