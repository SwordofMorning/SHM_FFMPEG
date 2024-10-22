#include <coroutine>
#include <iostream>
#include <thread>
#include <future>

class IntReader
{
private:
    int value_;

public:
    bool await_ready()
    {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle)
    {
        std::thread thread([this, handle]()
        {
            std::srand(static_cast<unsigned int>(std::time(nullptr)));
            value_ = std::rand();

            handle.resume();
        });
        thread.detach();
    }

    int await_resume()
    {
        std::cout << "val: " << std::to_string(value_) << std::endl;
        return value_;
    }
};

class Task
{
public:
    class promise_type
    {
    private:
        int value_{};
    public:
        Task get_return_object()
        {
            return Task{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        std::suspend_always yield_value(int value)
        {
            value_ = value;
            return {};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
        void return_void() {}

        int GetValue() const
        {
            return value_;
        }
    };

private:
    std::coroutine_handle<promise_type> coroutine_handle_;

public:
    Task(std::coroutine_handle<promise_type> handle) : coroutine_handle_(handle) {}

    int GetValue() const { return coroutine_handle_.promise().GetValue(); }

    void Next() { coroutine_handle_.resume(); }
};

Task GetInt()
{
    while (true)
    {
        IntReader reader;
        int retval = co_await reader;
        co_yield retval;
    }
}

int main()
{
    Task task = GetInt();

    for (int i = 0; i < 10; ++i)
    {
        std::cout << "loop: " << i << std::endl;
        std::cout << "total: " << task.GetValue() << std::endl;
        task.Next();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}