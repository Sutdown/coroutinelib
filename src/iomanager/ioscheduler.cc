#include "ioscheduler.h"

#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>

static bool debug = false;

namespace colib{
  /* 构造函数和析构函数 */
  IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
  :Scheduler(threads,use_caller,name){
    // 创建epoll fd
    m_epfd = epoll_create(5000);
    assert(m_epfd > 0);

    // 创建pipe,半双工，只能从写端流向读端
    int rt = pipe(m_tickleFds);
    assert(!rt);

    // 注册pipe句柄的可读事件，tickle调度协程
    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];

    // 非阻塞，配合ET,管道中没有数据read时会返回不会阻塞
    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    assert(!rt);
    // 加入epoll，可读时会通过epoll wait返回
    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    assert(!rt);

    contextResize(32);

    start(); // 开启调度器scheduler
  }

  // 调度完所有任务之后，关闭epoll和pipe句柄，释放所有的fd context
  // 退出前，所有的IO事件都要完成调度
  IOManager::~IOManager(){
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for (size_t i = 0; i < m_fdContexts.size();++i){
      if(m_fdContexts[i]){
        delete m_fdContexts[i];
      }
    }
  }

  /* fdcontext */
  IOManager::FdContext::EventContext &IOManager::FdContext::getEventContext(Event event){
    assert(event == READ || event == WRITE);
    switch(event){
      case READ:
        return read;
      case WRITE:
        return write;
      }
      throw std::invalid_argument("Unsupported event type");
  }

  void IOManager::FdContext::resetEventContext(EventContext &ctx){
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
  }

  void IOManager::FdContext::triggerEvent(Event event){
    assert(events & event);
    // delete
    events = (Event)(events & ~event);
    // trigger
    EventContext &ctx = getEventContext(event);
    if (ctx.cb){
      ctx.scheduler->scheduleLock(&ctx.cb);
    }else{
      ctx.scheduler->scheduleLock(&ctx.fiber);
    }

    resetEventContext(ctx);
    return;
  }

  /* public */
  // 添加事件
  int IOManager::addEvent(int fd, Event event, std::function<void()> cb){
    // 找到fd所在的fdcontext，不存在则分配一个
    FdContext *fd_ctx = nullptr;

    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
   // std::shared_lock read_lock(m_mutex); 表达相似，可以互换

    if ((int)m_fdContexts.size() > fd){
      fd_ctx = m_fdContexts[fd];
      read_lock.unlock();
    }else{
     // read_lock.unlock();
      std::unique_lock<std::shared_mutex> write_lock(m_mutex);
      contextResize(fd * 1.5);
      fd_ctx = m_fdContexts[fd];
    }

    // 这个事件存在时
    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
    if(fd_ctx->events & event){
      return -1;
    }

    // 添加新的事件,将新的事件加入epoll,用epollevent的私有指针指向fd context
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->events | event;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if (rt){
      std::cerr << "addEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
      return -1;
    }

    ++m_pendingEventCount;

    // 更新fd context, event context
    fd_ctx->events = (Event)(fd_ctx->events | event);
    FdContext::EventContext &event_ctx = fd_ctx->getEventContext(event);
    assert(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);
    event_ctx.scheduler = Scheduler::GetThis();
    if (cb){
      event_ctx.cb.swap(cb);
    } else {
      event_ctx.fiber = Fiber::GetThis();
      assert(event_ctx.fiber->getState() == Fiber::RUNNING);
    }
    return 0;
  }

  // 删除事件
  bool IOManager::delEvent(int fd, Event event){
    // find fd context
    FdContext *fd_ctx = nullptr;

    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    if((int)m_fdContexts.size() > fd){
      fd_ctx = m_fdContexts[fd];
      read_lock.unlock();
    }else{
      read_lock.unlock();
      return false;
    }

    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
    // attention 考虑事件不存在
    if(!(fd_ctx->events)&event){
      return false;
    }

    // 删除事件
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if (rt){
      std::cerr << "delEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
      return -1;
    }

    --m_pendingEventCount;

    // 重置上下文
    fd_ctx->events = new_events;
    FdContext::EventContext& event_ctx=fd_ctx->getEventContext(event);
    fd_ctx->resetEventContext(event_ctx);
    return true;
  }

  // 取消事件
  bool IOManager::cancelEvent(int fd, Event event){
    // find fd context
    FdContext *fd_ctx = nullptr;

    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    if ((int)m_fdContexts.size() > fd)
    {
      fd_ctx = m_fdContexts[fd];
      read_lock.unlock();
    }
    else
    {
      read_lock.unlock();
      return false;
    }

    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
    // attention 考虑事件不存在
    if (!(fd_ctx->events) & event)
    {
      return false;
    }

    // 删除事件
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if (rt){
      std::cerr << "cancelEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
      return -1;
    }

    --m_pendingEventCount;

    // update fdcontext, event context and trigger
    fd_ctx->triggerEvent(event);
    return true;
  }

  bool IOManager::cancelAll(int fd){
    // attemp to find FdContext
    FdContext *fd_ctx = nullptr;

    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    if ((int)m_fdContexts.size() > fd){
      fd_ctx = m_fdContexts[fd];
      read_lock.unlock();
    }else{
      read_lock.unlock();
      return false;
    }

    std::lock_guard<std::mutex> lock(fd_ctx->mutex);

    // none of events exist
    if (!fd_ctx->events){
      return false;
    }

    // delete all events
    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if (rt){
      std::cerr << "IOManager::epoll_ctl failed: " << strerror(errno) << std::endl;
      return -1;
    }

    // update fdcontext, event context and trigger
    if (fd_ctx->events & READ)
    {
      fd_ctx->triggerEvent(READ);
      --m_pendingEventCount;
    }

    if (fd_ctx->events & WRITE)
    {
      fd_ctx->triggerEvent(WRITE);
      --m_pendingEventCount;
    }

    assert(fd_ctx->events == 0);
    return true;
  }

  IOManager* IOManager::GetThis(){
    return dynamic_cast<IOManager *>(Scheduler::GetThis());
  }

  /* protected */
  // 通知调度器有任务调度
  void IOManager::tickle(){
    if(!hasIdleThreads()){
      return;
    }
    // 唤醒，通知管道可读取
    int rt = write(m_tickleFds[1], "T", 1);
    assert(rt == 1);
  }

  bool IOManager::stopping(){
    uint64_t timeout = getNextTimer();
    // 没有超时，没有待处理的事件，调度器也停止了
    return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
  }

  // idle 处于空闲，等待被唤醒
  /*
  @details
  * 对于IO协程调度
  * 可能阻塞在等待IO的时候，推出的时机在于epoll_wait返回，
  * 也就是tickle或者注册的io就绪，处理定时事件
  * 对于调度器
  * 无调度任务时会发生阻塞，那么IO调度器需要关注
  * Scheduler::schedule()和当前注册IO事件的触发
  @note
  * idle是典型的协程IO调度器的实现,结合 epoll、定时器和协程调度。
  * 在每次 epoll_wait 返回后，
  * 首先处理定时器超时事件，然后处理 I/O 事件，最后调度协程执行。
  * 这种模式能够高效地处理大量 I/O 请求，
  * 并且通过协程调度提高程序的并发能力。
  */
  void IOManager::idle(){
    // 每次epoll wait最多检测256个就绪事件
    static const int MAX_EVENTS = 256;
    std::unique_ptr<epoll_event[]> events(new epoll_event[MAX_EVENTS]);

    while(true){
      if (debug)
        std::cout << "IOManager::idle(),run in thread: " << Thread::GetThreadID() << std::endl;

        if(stopping()){
          if (debug)
            std::cout << "name = " << getName() << " idle exits in thread: " << Thread::GetThreadID() << std::endl;
          break;
        }

        // 阻塞在epoll wait，等待事件发生
        int rt = 0;
        while(true){
          static const uint64_t MAX_TIMEOUT = 5000;
          uint64_t next_timeout = getNextTimer();
          next_timeout = std::min(next_timeout, MAX_TIMEOUT);

          rt = epoll_wait(m_epfd, events.get(), MAX_EVENTS, (int)next_timeout);
          if(rt < 0 && errno == EINTR){
            continue;
          }else{
            break;
          }
        }

        // 收集所有定时器超时事件
        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
        if(!cbs.empty()){
          for(const auto&cb:cbs){
            scheduleLock(cb); // 存入任务队列，唤醒协程
          }
          cbs.clear();
        }

        // 收集所有就绪事件
        for (int i = 0; i < rt;i++){
          epoll_event &event = events[i];

          // ticklefd[0]用于通知协程调度，此时读完管道的数据，
          // 本轮idle结束之后，协程会重新调度
          if(event.data.fd==m_tickleFds[0]){
            uint8_t dummy[256];
            while(read(m_tickleFds[0],dummy,sizeof(dummy))>0)
              ;
            continue;
          }

          // 通过epoll event的私有指针获取fd cntext
          FdContext *fd_ctx = (FdContext *)event.data.ptr;
          std::lock_guard<std::mutex> lock(fd_ctx->mutex);

          // EPOLLERR or EPOLLHUP 转换成其它读写事件
          if (event.events & (EPOLLERR | EPOLLHUP)){
            event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
          }
          // 事件发生在epoll wait时
          int real_events = NONE;
          if (event.events & EPOLLIN){
            real_events |= READ;
          }
          if (event.events & EPOLLOUT){
            real_events |= WRITE;
          }
          if ((fd_ctx->events & real_events) == NONE){
            continue;
          }

          // 删除已经发生的事件
          // 根据op的值，对epfd进行op操作，操作对象是fd，event是监听的事件类型
          int left_events = (fd_ctx->events & ~real_events);
          int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
          event.events = EPOLLET | left_events;
          int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
          if(rt2){
            std::cerr << "idle::epoll_ctl failed: " << strerror(errno) << std::endl;
            continue;
          }

          // 处理已经发生的事情，让调度器调度指定的函数或者协程
          if (real_events & READ)
          {
            fd_ctx->triggerEvent(READ);
            --m_pendingEventCount;
          }
          if (real_events & WRITE)
          {
            fd_ctx->triggerEvent(WRITE);
            --m_pendingEventCount;
          }
        }
        // 处理完所有事情后，idle协程退出
        // 调度协程会重新检查是否有新任务调度
        // trigglerEvent只会把相应的fiber重新加入调度
        // 执行的话需要等idle退出
        Fiber::GetThis()->yield();
    }
  }

  void IOManager::onTimerInsertedAtFront(){
    tickle();
  }

  // nolock
  void IOManager::contextResize(size_t size){
    m_fdContexts.resize(size);

    for (size_t i = 0; i < m_fdContexts.size(); ++i){
      if (m_fdContexts[i] == nullptr){
        m_fdContexts[i] = new FdContext();
        m_fdContexts[i]->fd = i;
      }
    }
  }
}
