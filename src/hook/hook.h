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

#endif