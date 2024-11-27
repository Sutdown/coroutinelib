#include <iostream>
#include <coroutine>

struct AsyncSingleValue
{
  struct promise_type
  {
    int value;

    AsyncSingleValue get_return_object()
    {
      return AsyncSingleValue{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void return_value(int v) { value = v; }
    void unhandled_exception() { std::exit(1); }
  };

  std::coroutine_handle<promise_type> handle;

  explicit AsyncSingleValue(std::coroutine_handle<promise_type> h) : handle(h) {}
  ~AsyncSingleValue()
  {
    if (handle)
    {
      handle.resume(); // Ensure the coroutine completes before destroying
      handle.destroy();
    }
  }

  int get_value()
  {
    return handle.promise().value;
  }
};

AsyncSingleValue generate_single_value()
{
  co_return 42; // 协程返回一个值
}

int main()
{
  AsyncSingleValue result = generate_single_value();
  std::cout << "Generated value: " << result.get_value() << std::endl;
  return 0;
}
