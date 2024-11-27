#include <iostream>
#include <coroutine>
#include <optional>

/*
* promise_type 定义协程的行为，管理协程生命周期中的各种状态。
* Generator 提供协程控制器和迭代器接口，方便协程的使用。
* sequence 是一个协程函数，用来生成一个值序列。
*/

// 协程的返回类型
struct Generator
{
  struct promise_type
  {
    std::optional<int> current_value;

    /* C++编译器通过函数返回值识别协程函数。
    * 返回类型result中有一个子类型承诺对象（promise），
    * 通过std::coroutine_handle<promise_type>::from_promise()
    * 可以得到协程句柄（coroutine handle）。
    */
   // 生成协程函数的返回对象
    Generator get_return_object()
    {
      return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    std::suspend_always yield_value(int value)
    {
      current_value = value;
      return {};
    }

    void return_void() {}
    void unhandled_exception()
    {
      std::exit(1); // 未处理异常时退出程序
    }
  };

  // 协程句柄
  std::coroutine_handle<promise_type> handle;

  explicit Generator(std::coroutine_handle<promise_type> h) : handle(h) {}
  ~Generator()
  {
    if (handle)
      handle.destroy();
  }

  struct Iterator
  {
    std::coroutine_handle<promise_type> handle;

    Iterator &operator++()
    {
      handle.resume();
      return *this;
    }

    int operator*() const
    {
      return *handle.promise().current_value;
    }

    bool operator==(std::default_sentinel_t) const
    {
      return handle.done();
    }
  };

  Iterator begin()
  {
    handle.resume();
    return Iterator{handle};
  }

  std::default_sentinel_t end()
  {
    return {};
  }
};

// 协程函数：生成一个从 1 到 n 的序列
Generator sequence(int n)
{
  for (int i = 1; i <= n; ++i)
  {
    co_yield i; // 协程暂停并返回值
  }
}

int main()
{
  for (int value : sequence(5))
  {
    std::cout << value << " ";
  }
  return 0;
}
