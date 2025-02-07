#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>


class TaskQueue
{
public:

	// adds a task to the task queue
	void AddTask(std::function<void()>&& task)
	{
		{
			std::lock_guard<std::mutex> lock(_mutex);
			tasks.push(std::move(task));
			taskCount++;
		}
		condition.notify_one();
	}

	// grabs a task and pops it off the task queue.
	bool GetTask(std::function<void()>& task)
	{
		std::unique_lock<std::mutex> lock(_mutex);
		if (tasks.empty()) return false;

		task = std::move(tasks.front());
		tasks.pop();
		return true;
	}

	// checks if the tasks queue is empty;
	bool Empty()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		return tasks.empty();
	}

	// gets the condition
	std::condition_variable& GetCondition()
	{
		return condition;
	}

	// gets mutex
	std::mutex& GetMutex()
	{
		return _mutex;
	}

	// lets the thread work on other things
	void WaitUntilDone()
	{
		while (taskCount > 0)
		{
			std::this_thread::yield();
		}
	}

	// reduces the task count
	void CompleteTask()
	{
		taskCount--;
	}

private:
	std::queue<std::function<void()>> tasks;
	std::mutex _mutex;
	std::condition_variable condition;
	std::atomic<int> taskCount = 0;
};



class TaskSystem
{
public:

	TaskSystem(size_t threadCount) : working(true)
	{
		for (size_t i = 0; i < threadCount; i++)
		{
			workers.emplace_back([this]() 
			{ 
				while (working)
				{
					std::cout << "Here !!!\n";
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(taskQueue.GetMutex());
						taskQueue.GetCondition().wait(lock, [this] { return !taskQueue.Empty() || !working; });
						if (!working && taskQueue.Empty()) return;
					}
					if (taskQueue.GetTask(task))
					{
						task();
					}
				}
			});
		}
	}

	void SubmitTask(std::function<void()>&& task)
	{
		taskQueue.AddTask(std::move(task));
	}

	void Shutdown()
	{
		working = false;
		taskQueue.GetCondition().notify_all();
		for (std::thread& worker : workers)
		{
			if (worker.joinable())
			{
				worker.join();
			}
		}
	}

	~TaskSystem()
	{
		Shutdown();
	}

private:
	std::vector<std::thread> workers;
	TaskQueue taskQueue;
	std::atomic<bool> working = true;
	//const size_t threadCount = std::thread::hardware_concurrency();
};

void Job()
{
	std::cout << sqrt(500);
}


int main()
{
	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;
	const size_t threadCount = std::thread::hardware_concurrency();
	TaskSystem taskSystem(threadCount);

	std::cout << "Started with threads: " << threadCount << "\n";
	auto t1 = high_resolution_clock::now();


	for (int i = 0; i < 1000; i++)
	{
		taskSystem.SubmitTask([i]() {
			std::cout << "Task: " << i << " executed by thread " << std::this_thread::get_id() << std::endl;
			});
	}


	auto t2 = high_resolution_clock::now();
	duration<double, std::milli> ms_double = t2 - t1;
	std::cout << "Execution time: " << ms_double.count() << "ms\n";
	std::cout << "Ended\n";

	std::this_thread::sleep_for(std::chrono::seconds(1));
	taskSystem.Shutdown();
}

