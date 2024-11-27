# Coroutine

待办：

- [x] thread

  线程模块，封装了pthread里面的一些常用功能，Thread,Semaphore,Mutex,RWMutex,Spinlock等对象，可以方便开发中对线程日常使用 为什么不适用c++11里面的thread 本框架是使用C++11开发，不使用thread，是因为thread其实也是基于pthread实现的。并且C++11里面没有提供读写互斥量，RWMutex，Spinlock等，在高并发场景，这些对象是经常需要用到的。所以选择了自己封装pthread

- [x] 协程类

  协程：用户态的线程，相当于线程中的线程，更轻量级。后续配置socket hook，可以把复杂的异步调用，封装成同步操作。降低业务逻辑的编写复杂度。 目前该协程是基于ucontext_t来实现的，后续将支持采用boost.context里面的fcontext_t的方式实现

- [x] 协程调度

  协程调度器，管理协程的调度，内部实现为一个线程池，支持协程在多线程中切换，也可以指定协程在固定的线程中执行。是一个N-M的协程调度模型，N个线程，M个协程。重复利用每一个线程。

  - [ ] 容易出现core dump，未解决

- [ ] 协程IO

  继承与协程调度器，封装了epoll（Linux），并支持定时器功能（使用epoll实现定时器，精度毫秒级）,支持Socket读写时间的添加，删除，取消功能。支持一次性定时器，循环定时器，条件定时器等功能

- [ ] 定时器

- [ ] hook

  hook系统底层和socket相关的API，socket io相关的API，以及sleep系列的API。hook的开启控制是线程粒度的。可以自由选择。通过hook模块，可以使一些不具异步功能的API，展现出异步的性能。如（mysql）



## thread

主要有两个类，`Semaphore`和`Thread`

#### Semaphore

信号量，实现PV操作，主要用于线程同步

#### Thread

1. 系统自动创建主线程t_thread

2. 由thread类创建的线程。

   m_thread 通常是线程类内部的成员变量，用来存储底层的线程标识符

   t_thread 可能是外部管理线程生命周期的对象或容器，它可以是线程池、线程列表、智能指针等，帮助你在类外部管理多个线程的创建、执行、销毁等操作。

   

## 协程类

- 非对称模型
- 有栈协程，独立栈。

对于协程类，其中需要什么。协程首先需要随时切换和恢复，这里采用的是**glibc的ucontext组件**。

### ucontext_t

这个类中有成员：

```cpp
// 当前上下文结束后下一个激活的上下文对象的指针，只在当前上下文是由makecontext创建时有效
struct ucontext_t *uc_link;
// 当前上下文的信号屏蔽掩码
sigset_t uc_sigmask;
// 当前上下文使用的栈内存空间，只在当前上下文是由makecontext创建时有效
stack_t uc_stack;
// 平台相关的上下文具体内容，包含寄存器的值
mcontext_t uc_mcontext;
```

函数：

```cpp
// 获取当前上下文
int getcontext(ucontext_t *ucp);

// 恢复ucp指向的上下文
int setcontext(const ucontext_t *ucp);

// 修改当前上下文指针ucp，将其与func函数绑定
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);

// 将当前上下文保存到oucp中，将执行转到ucp中
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);
```



> 对于该协程类，有栈 or 无栈？对称 or 非对称？

- 对于对称和非对称的话，**对称协程更为灵活，非对称协程更为简单易实现**。协程中一般存在协程调度器和协程两种角色，对称协程中相当于每个协程都要充当调度器的角色，程序设计复杂，程序的控制流也会复杂难以管理。

  常见的`js中的async/await`，`go中的coroutine`都是非对称协程，是因为非对称协程的切换过程是单项的，更适合事件驱动，任务队列等调度模型；但是c语言中的`ucontext`属于对称协程的经典实现，`boost.context`为对称协程的现代实现，更适合需要多个协程频繁通信的场景。

- [有栈协程和无栈协程](https://mthli.xyz/stackful-stackless/)有栈和无栈的本质区别在于是否可以在任意嵌套函数中被挂起。一般有栈可以被挂起，无栈则不行。有栈比较适用于功能强大，支持嵌套调用和复杂控制流，灵活的操作上下文的需求，比如`boost.COntext`；无栈由于存储在内存中，适用于内存占用少，实现简单的场景，比如JavaScript `async`/`await` 和 `Promise`，Erlang 和 Go的`Goroutine`。



这里我们的协程类，采用的是非对称模型，有栈协程。因此可以推导出所需要的私有成员：

```cpp
private:
    uint64_t m_id = 0;
    State m_state = READY;

    ucontext_t m_ctx;

    uint32_t m_stacksze = 0;    // 栈大小
    void *m_stack = nullptr;    // 栈空间

    std::function<void()> m_cb; // 运行函数
    bool m_runInScheduler;
```

对于协程类，我们需要一个主协程和其它的用户协程，以及一个协程调度器。对于主协程则是直接无参构造函数直接创建，（由于只能创建一次，因此私有），有参构造函数创建其它协程。同时需要设置`resume,yield`其它函数调度协程的运行。大概这个样子：

```cpp
  // 线程局部变量，当前线程正在运行的协程
  static thread_local Fiber *t_fiber = nullptr;
  // 线程局部变量，当前线程的主协程，切换到这个协程，相当于切换到主线程
  static thread_local std::shared_ptr<Fiber> t_thread_fiber = nullptr;
  static thread_local Fiber *t_scheduler_fiber = nullptr;
```





## 协程调度

一个线程只有一个协程，一个协程类中会包含三个协程，分别是主协程（main），调度协程和任务协程。其中任务协程是由协程类自主创建，主协程和调度协程都是静态变量，在多种类中其实只存在一个实体。

协程调度致力于封装一些操作，因为调度协程本身需要创建协程，协程任务的执行顺序，如何利用多线程或者调度协程池保证效率，在协程任务结束之后也需要停止调度器释放资源。如果建立一个scheduler类封装这些操作，那么为用户开放的仅仅只有**启动线程池，关闭线程池，添加任务**三种操作了。

其中main主协程可以选择是否参与调度，如果不参与，那么比如在main开始调度时创建其它协程进行协程调度；如果参与，多线程的情况下和不参与相同。如果是单线程，那么只能等到main结束时开始调度其它协程。

虽然main是主协程（caller协程），不过main函数所在的线程也能执行任务，在实现相同调度能力的情况下，线程数越少，线程切换的开销也就更小。

最终过程：

1. main函数主协程运行，创建调度器
2. 向调度器添加任务，开始协程调度，main让出执行权，调度协程执行任务队列中的任务
3. 每次执行任务时，调度协程都要让出执行权，再回到调度协程继续下一个任务
4. 所有任务执行完后，调度协程让出执行权切回main函数主协程结束。





## 协程IO

## 定时器

## hook



# 参考

1. 代码随想录协程库项目精讲