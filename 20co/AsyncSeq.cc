#include <iostream>
#include <coroutine>

struct AsyncSequence
{
  struct promise_type
  {
    int current_value;

    AsyncSequence get_return_object()
    {
      return AsyncSequence{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(int value)
    {
      current_value = value;
      return {};
    }

    void return_void() {}
    void unhandled_exception() { std::exit(1); }
  };

  std::coroutine_handle<promise_type> handle;

  explicit AsyncSequence(std::coroutine_handle<promise_type> h) : handle(h) {}
  ~AsyncSequence()
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
      return handle.promise().current_value;
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

  std::default_sentinel_t end() { return {}; }
};

AsyncSequence generate_sequence(int n)
{
  for (int i = 1; i <= n; ++i)
  {
    co_yield i; // 延迟生成值
  }
}

int main()
{
  for (int value : generate_sequence(5))
  {
    std::cout << value << " ";
  }
  return 0;
}
