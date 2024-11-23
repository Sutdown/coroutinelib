#ifndef THREAD_H
#define THREAD_H

#include <mutex>
#include <condition_variable>
#include <functional>
#include <string>

namespace colib{
  // 信号量，用于线程同步
  class Semaphore{
  private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;

  public:
    explicit Semaphore(int count_=0):count(count_){}
    
    // P操作
    void wait(){
      std::unique_lock<std::mutex> lock(mtx);
      while(count==0){
        cv.wait(lock); // wait for signals
      }
      count--;
    }

    // V操作
    void signal(){
      std::unique_lock<std::mutex> lock(mtx);
      count++;
      cv.notify_one(); // signal
    }
  };
  
  // 1.系统自动创建主线程t_thread 2.由thread类创建的线程
  class Thread{
    public:
      Thread(std::function<void()> cb, const std::string &name);
      ~Thread();

      pid_t getID() const { return m_id; }
      const std::string &getName() const { return m_name; }

      void join();

    public:
      static pid_t GetThreadID(); // 获取系统分配的线程id
      static Thread *GetThis(); // 获取当前所在线程

      static const std::string &GetName(); // 获取当前线程名字
      static void SetName(const std::string &name); // 设置当前线程的名字

    private:
      static void *run(void *arg); // 线程函数

    private:
      pid_t m_id = -1; // 进程ID
      pthread_t m_thread = 0; // 线程ID

      std::function<void()> m_cb; // 线程需要运行的函数
      std::string m_name;

      Semaphore m_semaphore;
  };
}

#endif