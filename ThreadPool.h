/*
Copyright (C) 2020 - 2021, Beijing Chen An Co.Ltd.All rights reserved.
File Name: ThreadPool.h
Description: Thread pool class in Image Pro
Change History:
Date         Version        Changed By      Changes
2020/10/08    1.0.0.1       Gao             Create
2022/03/03    1.0.0.1       dwj             Modify
*/

#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

#define  MAX_THREAD_NUM 256

class ThreadPool
{
	using Task = std::function<void()>;
	
public:
	inline ThreadPool(unsigned short size = 4) :stoped{ false }
	{
		idlThrNum = 0;
		createTask(size);
	}
	inline ~ThreadPool()
	{
		stoped.store(true);
		cv_task.notify_all();
		for (std::thread& thread : pool) {
			if (thread.joinable())
				thread.join();
		}
	}

public:
	template<class F, class... Args>
	auto addJob(F&& f, Args&&... args) ->std::future<decltype(f(args...))>
	{
		if (stoped.load())    
			throw std::runtime_error("commit on ThreadPool is stopped.");

		using RetType = decltype(f(args...)); 
		auto task = std::make_shared<std::packaged_task<RetType()> >(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);    
		std::future<RetType> future = task->get_future();
		{    
			std::lock_guard<std::mutex> lock{ m_lock };
			tasks.emplace(
				[task]()
			{ 
				(*task)();
			}
			);
		}
		cv_task.notify_one(); 

		return future;
	}

	int idlCount() { return idlThrNum; }

	void createTask(unsigned short create_num)
	{
		std::atomic<int> size;
		for (size.store(idlThrNum); size < create_num; ++size)
		{
			idlThrNum++;
			pool.emplace_back(
				[this]
			{
				while (!this->stoped)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock{ this->m_lock };
						this->cv_task.wait(lock,  //第二个参数 false 的时候阻塞 ，ture的时候才会被notify激活
							[this] {
							return this->stoped.load() || !this->tasks.empty();
						}
						);
						if (this->stoped && this->tasks.empty())
							return;
						task = std::move(this->tasks.front());
						this->tasks.pop();
					}
					idlThrNum--;
					task();
					break;
					//idlThrNum++;
				}
			}
			);
		}
	}

private:

	std::vector<std::thread> pool;

	std::queue<Task> tasks;

	std::mutex m_lock;

	std::condition_variable cv_task;

	std::atomic<bool> stoped;

	std::atomic<int> idlThrNum;

};