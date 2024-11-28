#include "timer.h"

namespace colib
{
  /* Timer*/

  bool Timer::cancel() {
  }

  // refresh 只会向后调整
  bool Timer::refresh() {
  }

  bool Timer::reset(uint64_t ms, bool from_now) {
  }

  Timer::Timer(uint64_t ms, std::function<void()> cb,
               bool recurring, TimerManager *manager) : m_recurring(recurring), m_ms(ms), m_cb(cb), m_manager(manager) {
  }

  bool Timer::Comparator::operator()(const std::shared_ptr<Timer> &lhs, const std::shared_ptr<Timer> &rhs) const{

  }

  /* Time manager */
  
  TimerManager::TimerManager() {

  }

  TimerManager::~TimerManager() {

  }

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
    
  }

  bool TimerManager::detectClockRollover() {
   
  }

}
