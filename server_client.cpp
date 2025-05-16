#include <iostream>
#include <queue>
#include <future>
#include <thread>
#include <chrono>
#include <cmath>
#include <cassert>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <map>
#include <any>
#include <atomic>
#include <tuple>
#include <stop_token>

// Forward declaration
class TaskWrapperBase;

// Очередь задач
std::queue<int> tasks;
std::unordered_map<int, std::unique_ptr<TaskWrapperBase>> results;
std::mutex mut;
std::condition_variable cond_var;
std::atomic<int> nextid{0};
std::jthread server_thread_obj;

// Base class for type erasure
class TaskWrapperBase {
public:
    virtual ~TaskWrapperBase() = default;
    virtual void execute() = 0;
    virtual std::any get_result() = 0;
};

template<typename F, typename... Args>
class TaskWrapper : public TaskWrapperBase {
    F func;
    std::tuple<Args...> args;
    std::any result;
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;

public:
    TaskWrapper(F f, Args... as) 
        : func(std::move(f)), args(std::forward<Args>(as)...) {}

    void execute() override {
        auto res = std::apply(func, args);
        {
            std::lock_guard<std::mutex> lock(mtx);
            result = std::move(res);
            ready = true;
        }
        cv.notify_all();
    }

    std::any get_result() override {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return ready; });
        return result;
    }

    bool is_ready() const {
        return ready;
    }
};

void server_thread(std::stop_token stoken) {
    int task_id;
    std::unique_lock<std::mutex> lock(mut, std::defer_lock);

    while (!stoken.stop_requested()) {
        lock.lock();
        cond_var.wait(lock, [&stoken] { 
            return !tasks.empty() || stoken.stop_requested(); 
        });
        
        if (stoken.stop_requested()) {
            break;
        }

        if (!tasks.empty()) {
            task_id = tasks.front();
            tasks.pop();
            lock.unlock();
        
            auto it = results.find(task_id);
            if (it != results.end()) {
                it->second->execute();
            }
        }
    }
    std::cout << "Server stop!\n";
}

class Server {
private:
    std::jthread server_t;

public:
    Server() = default;
    ~Server() { stop(); }

    void start() {
        server_t = std::jthread(server_thread);
    }
    
    void stop() {
        if (server_t.joinable()) {
            server_t.request_stop();
            cond_var.notify_all();
        }
    }
};

template<typename F, typename... Args>
int add_task(F&& func, Args&&... args) {
    using WrapperType = TaskWrapper<std::decay_t<F>, std::decay_t<Args>...>;
        
    int task_id = nextid++;
    {
        std::lock_guard<std::mutex> lock(mut);
        results[task_id] = std::make_unique<WrapperType>(
            std::forward<F>(func), 
            std::forward<Args>(args)...
        );
        tasks.push(task_id);
    }
    cond_var.notify_one();
    return task_id;
}

template<typename T>
T request_result(int task_id) {
    std::unique_ptr<TaskWrapperBase> task;
    
    auto it = results.find(task_id);    
    if (it == results.end()) {
        throw std::runtime_error("Task ID not found");
    }

    T res = std::any_cast<T>(it->second->get_result());

    {
        std::lock_guard<std::mutex> lock(mut);
        task = std::move(it->second);
        results.erase(it);
    }

    return res;
}

template<typename T>
T f_sq(T x) 
{
    //std::this_thread::sleep_for(std::chrono::seconds(2));
    return x * x;
}

template<typename T>
T f_sqrt(T x) 
{
    //std::this_thread::sleep_for(std::chrono::seconds(1));
    return std::sqrt(x);
}

template<typename T>
T f_sin(T x) 
{
    //std::this_thread::sleep_for(std::chrono::seconds(2));
    return std::sin(x);
}

template<typename T>
T f_smthlse(T a, T b, T c) 
{
    return a * b + c;
}

void client(int N)
{
    std::map<int, int> map;

    while (N > 0)
    {
        int arg1 = rand();
        int arg2 = rand();
        int arg3 = rand();

        int id1 = add_task(f_sq<int>, arg1);
        int id2 = add_task(f_sqrt<int>, arg2);
        int id3 = add_task(f_sin<int>, arg3);
        
        map[id1] = (arg1 * arg1);
        map[id2] = std::sqrt(arg2);
        map[id3] = std::sin(arg3);
        
        N -= 3;
    }

    
    for (auto i : map)
    {
        int result = request_result<int>(i.first);
        if (result != map[i.first])
        {
            std::cout << result << " != " << map[i.first] << std::endl;
            exit(13);
            //throw std::exception("Wrong result");
        }
    }
    
}

int main() {
    std::cout << "Start\n";
    Server server;
    server.start();

    std::cout << "Running 10000 tasks (Thread 1)" << std::endl;
    std::thread client1(client, 10000);
    std::cout << "Running 10000 tasks (Thread 2)" << std::endl;
    std::thread client2(client, 10000);
    std::cout << "Running 10000 tasks (Thread 3)" << std::endl;
    std::thread client3(client, 10000);

    int task1 = add_task(f_smthlse<int>, 2, 2, 2);
    int res1 = request_result<int>(task1);
    std::cout << res1 << std::endl;

    client1.join();
    std::cout << "Thread 1 joined" << std::endl;
    client2.join();
    std::cout << "Thread 2 joined" << std::endl;
    client3.join();
    std::cout << "Thread 3 joined" << std::endl;
    
    server.stop();
    std::cout << "End\n";
}
