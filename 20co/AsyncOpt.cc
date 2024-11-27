#include <iostream>
#include <coroutine>
#include <optional>

struct AsyncOptionalExit
{
  struct promise_type
  {
    std::optional<int> value;

    AsyncOptionalExit get_return_object()
    {
      return AsyncOptionalExit{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(std::optional<int> v)
    {
      value = v;
      return {};
    }

    void return_void() {}
    void unhandled_exception() { std::exit(1); }
  };

  std::coroutine_handle<promise_type> handle;

  explicit AsyncOptionalExit(std::coroutine_handle<promise_type> h) : handle(h) {}
  ~AsyncOptionalExit()
  {
    if (handle)
      handle.destroy();
  }

  std::optional<int> get_value()
  {
    return handle.promise().value;
  }
};

AsyncOptionalExit generate_until_optional()
{
  co_yield std::nullopt;           // 协程开始时，返回一个空值
  co_yield 10;                     // 返回值 10
  co_yield 20;                     // 返回值 20
  co_yield std::make_optional(30); // 提前退出，返回值 30
  co_return;                       // 协程结束
}

int main()
{
  AsyncOptionalExit result = generate_until_optional();
  auto value = result.get_value();
  if (value)
  {
    std::cout << "Generated value: " << *value << std::endl;
  }
  else
  {
    std::cout << "No value generated" << std::endl;
  }
  return 0;
}
