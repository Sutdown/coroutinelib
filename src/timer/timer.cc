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
    std::shared_ptr<void> tmp = weak_cond.lock();
    if(tmp){
      cb();
    }
  }

  std::shared_ptr<Timer> TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring){
    return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
  }

  // 找到最近的超时时间
  uint64_t TimerManager::getNextTimer() {
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);

    // 重置
    m_tickled = false;

    if(m_timers.empty()){
      return ~0ull;
    }

    auto now = std::chrono::system_clock::now();
    auto time = (*m_timers.begin())->m_next;

    if(now>=time){
      // 已经超时
      return 0;
    }else{
      // 返回剩余时间
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time - now);
      return static_cast<uint64_t>(duration.count());
    }
  }

  // 取出所有超时定时器的回调函数
  void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
    auto now = std::chrono::system_clock::now();
    std::unique_lock<std::shared_mutex> write_lock(m_mutex);
    
    bool rollover = detectClockRollover(); // 检测时间是否回滚

    // 定时器不为空，rollover为true
    // 定时器不为空，到达第一个定时器的触发时间
    while (!m_timers.empty() && rollover || !m_timers.empty() && (*m_timers.begin())->m_next <= now){
      std::shared_ptr<Timer> temp = *m_timers.begin();
      m_timers.erase(m_timers.begin());

      cbs.push_back(temp->m_cb);

      if(temp->m_recurring){
        // 重新加入
        temp->m_next = now + std::chrono::milliseconds(temp->m_ms);
        m_timers.insert(temp);
      }else{
        temp->m_cb = nullptr;
      }
    }
  }

  bool TimerManager::hasTimer() {
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    return !m_timers.empty();
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
    // 如果当前时间 now 小于这个时间窗口的上限，
    // 说明当前时间比上次记录的时间少了超过一小时，
    // 这通常是由于系统时间回绕或时钟调整导致的。
    if (now < (m_previouseTime - std::chrono::milliseconds(60 * 60 * 1000))){
      rollover = true;
    }
    // 调整准确时间
    m_previouseTime = now;
    return rollover;
  }
}
