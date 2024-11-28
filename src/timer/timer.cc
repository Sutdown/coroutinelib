#include "timer.h"

namespace colib
{
  /* Timer*/

  bool Timer::cancel() {
    std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);

    // 将回调函数设置为空
    if(m_cb==nullptr){
      return false;
    }else{
      m_cb = nullptr;
    }

    // 将定时器管理器中的该定时器删除
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it!=m_manager->m_timers.end()){
      m_manager->m_timers.erase(it);
    }
    return true;
  }

  // refresh 只会向后调整
  bool Timer::refresh() {
    std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);

    if(m_cb==nullptr){
      return false;
    }

    // 删除管理中的原定时器，重新添加，这里采用了共享锁的机制，也在访问共享资源时采用了独占锁
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it==m_manager->m_timers.end()){
      return false;
    }
    m_manager->m_timers.erase(it);
    m_next = std::chrono::system_clock::now() + std::chrono::milliseconds(m_ms);
    m_manager->m_timers.insert(shared_from_this());
    return true;
  }

  // 重置定时器周期
  bool Timer::reset(uint64_t ms, bool from_now) {
    if(ms==m_ms&&!from_now){
      return true;
    }

    {
      std::unique_lock<std::shared_mutex> wirte_lock(m_manager->m_mutex);
      if(!m_cb){
        return false;
      }

      auto it = m_manager->m_timers.find(shared_from_this());
      if(it==m_manager->m_timers.end()){
        return false;
      }
      m_manager->m_timers.erase(it);
    }

    // 重置周期
    auto start = from_now ? std::chrono::system_clock::now() : m_next - std::chrono::milliseconds(m_ms);
    m_ms = ms;
    m_next = start + std::chrono::milliseconds(m_ms);
    m_manager->addTimer(shared_from_this());
    return true;
  }

  Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager)
                  : m_recurring(recurring), m_ms(ms), m_cb(cb), m_manager(manager) {
    auto now = std::chrono::system_clock::now();
    m_next = now + std::chrono::milliseconds(m_ms);
  }

  bool Timer::Comparator::operator()(const std::shared_ptr<Timer> &lhs, const std::shared_ptr<Timer> &rhs) const{
    assert(lhs != nullptr && rhs != nullptr);
    return lhs->m_next < rhs->m_next;
  }

  /* Time manager */

  TimerManager::TimerManager() {
    m_previouseTime = std::chrono::system_clock::now();
  }

  TimerManager::~TimerManager() {}

  std::shared_ptr<Timer> TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring)
  {
    std::shared_ptr<Timer> timer(new Timer(ms, cb, recurring, this));
    addTimer(timer);
    return timer;
  }

  // 如果条件存在 -> 执行cb()
  static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
  
  }

  std::shared_ptr<Timer> TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring){
    
  }

  uint64_t TimerManager::getNextTimer() {
    
  }

  void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
   
  }

  bool TimerManager::hasTimer() {
    
  }

  // lock + tickle()
  void TimerManager::addTimer(std::shared_ptr<Timer> timer) {
    bool at_front = false;
    {
      std::unique_lock<std::shared_mutex> write_lock(m_mutex);
      auto it = m_timers.insert(timer).first;
      at_front = (it == m_timers.begin()) && !m_tickled;

      // 只有一个线程在插入第一个定时器时执行“tickle”操作
      if(at_front){
        m_tickled = true;
      }
    }

    if(at_front){
      onTimerInsertedAtFront();
    }
  }

  // 检测系统时间是否发生了回滚
  bool TimerManager::detectClockRollover() {
    bool rollover = false;
    auto now = std::chrono::system_clock::now();
    if (now < (m_previouseTime - std::chrono::milliseconds(60 * 60 * 1000))){
      rollover = true;
    }
    m_previouseTime = now;
    return rollover;
  }
}
