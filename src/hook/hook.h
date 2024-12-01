#ifndef HOOK_H_
#define HOOK_H_

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

/*hook是对系统调用API进行一次封装，将其封装成一个与原始的系统调用API同名的接口
  在应用这个接口时，会先执行封装中的操作，再执行原始的系统调用API。
  本项目中，出于保障协程能够在发生阻塞时随时切换，因此会对IO协程调度中相关的系统调用进行hook
  让调度协程尽可能把时间片花在有意义的操作上。
  hook的重点在于替换API的底层实现同时完全模拟原本的行为*/

/*本项目中，关于hook模块和IO协程调度的整合，一共有三类接口需要hook：
  1.
  sleep延时系列接口。对于这些接口，给IO协程调度器注册一个定时事件，定时事件触发之后再执行当前协程即可。
  注册完之后yield让出执行权。
  2.
  socket IO系系列接口。包括read/write/recv/send/connect/accept...这类接口的hook需要先判断fd是否为socket fd，
  以及用户是否显式的对该fd设置过非阻塞模式，如果都不是，就不需要hook了。
  如果需要hook，现在IO协程调度器上注册对应读写事件，事件发生后再继续当前协程。
  当前协程注册完IO之后即可yield让出执行权。
  3.
  socket/fcntl/ioctl/close...这类接口主要用于处理边缘情况，
  比如fd上下文，处理超时，显示非阻塞等。
*/

namespace colib{
  // hook模块是线程粒度的，各个线程可以单独启用或者关闭hook
  bool is_hook_enable();
  void set_hook_enable(bool flag);
}

extern "C"{
  /* track the original version */
  typedef unsigned int (*sleep_fun)(unsigned int seconds);
  extern sleep_fun sleep_f;

  typedef int (*usleep_fun)(useconds_t usec);
  extern usleep_fun usleep_f;

  typedef int (*nanosleep_fun)(const struct timespec *req, struct timespec *rem);
  extern nanosleep_fun nanosleep_f;

  typedef int (*socket_fun)(int domain, int type, int protocol);
  extern socket_fun socket_f;

  typedef int (*connect_fun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
  extern connect_fun connect_f;

  typedef int (*accept_fun)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
  extern accept_fun accept_f;

  typedef ssize_t (*read_fun)(int fd, void *buf, size_t count);
  extern read_fun read_f;

  typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
  extern readv_fun readv_f;

  typedef ssize_t (*recv_fun)(int sockfd, void *buf, size_t len, int flags);
  extern recv_fun recv_f;

  typedef ssize_t (*recvfrom_fun)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
  extern recvfrom_fun recvfrom_f;

  typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr *msg, int flags);
  extern recvmsg_fun recvmsg_f;

  typedef ssize_t (*write_fun)(int fd, const void *buf, size_t count);
  extern write_fun write_f;

  typedef ssize_t (*writev_fun)(int fd, const struct iovec *iov, int iovcnt);
  extern writev_fun writev_f;

  typedef ssize_t (*send_fun)(int sockfd, const void *buf, size_t len, int flags);
  extern send_fun send_f;

  typedef ssize_t (*sendto_fun)(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
  extern sendto_fun sendto_f;

  typedef ssize_t (*sendmsg_fun)(int sockfd, const struct msghdr *msg, int flags);
  extern sendmsg_fun sendmsg_f;

  typedef int (*close_fun)(int fd);
  extern close_fun close_f;

  typedef int (*fcntl_fun)(int fd, int cmd, ... /* arg */);
  extern fcntl_fun fcntl_f;

  typedef int (*ioctl_fun)(int fd, unsigned long request, ...);
  extern ioctl_fun ioctl_f;

  typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
  extern getsockopt_fun getsockopt_f;

  typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
  extern setsockopt_fun setsockopt_f;

  /* function prototype */
  // sleep
  // 暂停进程的秒，微秒，毫秒
  // 期间进程不执行其它操作，直到时间到期或者信号中断（定时器修改）
  unsigned int sleep(unsigned int seconds); 
  int usleep(useconds_t usec);
  int nanosleep(const struct timespec *req, struct timespec *rem);

  // socket function
  int socket(int domain, int type, int protocol);
  int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
  int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

  // read
  ssize_t read(int fd, void *buf, size_t count);
  ssize_t readv(int fd,const struct iovec *iov,int iovcnt);

  ssize_t recv(int sockfd, void *buf, size_t len, int flags);      // 从sockfd接收数据
  ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen); // 从sockfd接收数据，获取发送者地址，常用于UDP
  ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);      // 接收多个消息或者带有复杂头部的消息，比如ICMP

  // write
  ssize_t write(int fd, const void *buf, size_t count);
  ssize_t writev(int fd, const struct iovec *iov, int iovcnt);

  ssize_t send(int sockfd, const void *buf, size_t len, int flags);
  ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen); // 常用于UDP
  ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);    // 可用于ICMP

  // fd
  int close(int fd);

  // socket control
  int fcntl(int fd, int cmd, ... /*arg*/);       // 操作文件描述符的各种属性
  int ioctl(int fd, unsigned long request, ...); // 控制设备或者套接字

  int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen); // 获取套接字的选项值
  int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
}

#endif