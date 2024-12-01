#include "hook.h"
#include "fd_manager.h"
#include "../iomanager/ioscheduler.h"

#include <dlfcn.h>
#include <iostream>
#include <cstdarg>
#include <string.h>

// apply XX to all functions
#define HOOK_FUN(XX) \
        XX(sleep) XX(usleep) XX(nanosleep) \
        XX(socket) XX(connect) XX(accept) \
        XX(read) XX(readv) \
        XX(recv) XX(recvfrom) XX(recvmsg) \
        XX(write) XX(writev) \
        XX(send) XX(sendto) XX(sendmsg) \
        XX(close) \
        XX(fcntl) XX(ioctl) \
        XX(getsockopt) XX(setsockopt)

/* namespace */
namespace colib{
  // 每个线程是否使用hook
  static thread_local bool t_hook_enable = false;

  bool is_hook_enable() { return t_hook_enable; }

  void set_hook_enable(bool flag) { t_hook_enable = flag; }

  void hook_init(){
    static bool is_inited = false;
    if(is_inited){
      return;
    }

    is_inited = true;

// 扩展到 sleep_f = (sleep_fun)dlsym(((void *) -1l), "sleep");等
// 动态加载sleep函数的地址，将其存储到sleep_f函数指针中
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX)
#undef XX
  }

// 单例模式，程序启动执行初始化，确保其只初始化一次
  struct HookIniter{
    HookIniter() { hook_init(); }
  };

  static HookIniter s_hook_initer;
}

/* global */
struct timer_info{
  int cancelled = 0;
};

template<typename OriginFun,typename... Args>
static ssize_t do_io(int fd,OriginFun fun,const char* hook_fun_name,uint32_t event,int timeout_so,Args&&... args){

}

/* extern C */
extern "C"
{

// declaration -> sleep_fun sleep_f = nullptr;
#define XX(name) name##_fun name##_f = nullptr;
  HOOK_FUN(XX)
#undef XX

  /* sleep */
  // 暂停进程的秒，微秒，毫秒
  // 期间进程不执行其它操作，直到时间到期或者信号中断（定时器修改）
  unsigned int sleep(unsigned int seconds){
    if(!colib::t_hook_enable){
      return sleep_f(seconds);
    }

    std::shared_ptr<colib::Fiber> fiber = colib::Fiber::GetThis();
    colib::IOManager* iom=colib::IOManager::GetThis();
    iom->addTimer(seconds * 1000, [fiber, iom]()
                  { iom->scheduleLock(fiber, -1); });
    fiber->yield();
    return 0;
  }
  int usleep(useconds_t usec){
    if (!colib::t_hook_enable)
    {
      return usleep_f(usec);
    }

    std::shared_ptr<colib::Fiber> fiber = colib::Fiber::GetThis();
    colib::IOManager *iom = colib::IOManager::GetThis();
    // add a timer to reschedule this fiber
    iom->addTimer(usec / 1000, [fiber, iom]()
                  { iom->scheduleLock(fiber); });
    // wait for the next resume
    fiber->yield();
    return 0;
  }
  int nanosleep(const struct timespec *req, struct timespec *rem){
    if (!colib::t_hook_enable)
    {
      return nanosleep_f(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;

    std::shared_ptr<colib::Fiber> fiber = colib::Fiber::GetThis();
    colib::IOManager *iom = colib::IOManager::GetThis();
    // add a timer to reschedule this fiber
    iom->addTimer(timeout_ms, [fiber, iom]()
                  { iom->scheduleLock(fiber, -1); });
    // wait for the next resume
    fiber->yield();
    return 0;
  }

  /* socket function */
  int socket(int domain, int type, int protocol){

  }
  int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){

  }
  int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){

  }

  /* read */
  ssize_t read(int fd, void *buf, size_t count){

  }
  ssize_t readv(int fd, const struct iovec *iov, int iovcnt){

  }

  ssize_t recv(int sockfd, void *buf, size_t len, int flags){

  } // 从sockfd接收数据
  ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen){
    
  } // 从sockfd接收数据，获取发送者地址，常用于UDP
  ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags){

  } // 接收多个消息或者带有复杂头部的消息，比如ICMP

  /* write */
  ssize_t write(int fd, const void *buf, size_t count){

  }
  ssize_t writev(int fd, const struct iovec *iov, int iovcnt){

  }

  ssize_t send(int sockfd, const void *buf, size_t len, int flags){

  }
  ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen){

  } // 常用于UDP
  ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags){

  } // 可用于ICMP

  /* fd */
  int close(int fd){

  }

  // socket control
  int fcntl(int fd, int cmd, ... /*arg*/){

  } // 操作文件描述符的各种属性
  int ioctl(int fd, unsigned long request, ...){

  } // 控制设备或者套接字

  int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen){

  } // 获取套接字的选项值
  int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen){

  }
}