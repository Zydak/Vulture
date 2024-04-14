#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace Vulture
{
	class ThreadPool
	{
	public:
		struct CreateInfo
		{
			uint32_t threadCount;
		};

		ThreadPool() = default;
		ThreadPool(const CreateInfo& createInfo);
		~ThreadPool();

		void Init(const CreateInfo& createInfo);
		void Destroy();

		explicit ThreadPool(const ThreadPool& other);
		explicit ThreadPool(ThreadPool&& other) noexcept = delete;
		ThreadPool& operator=(const ThreadPool& other);
		ThreadPool& operator=(ThreadPool&& other) noexcept = delete;

		template<typename T, typename ...Args>
		void PushTask(T&& task, Args&& ... args)
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_Tasks.emplace(std::bind(std::forward<T>(task), std::forward<Args>(args)...));
			lock.unlock();
			m_CV.notify_one();
		}

		inline uint32_t GetThreadCount() const { return (uint32_t)m_WorkerThreads.size(); }

	private:
		std::vector<std::thread> m_WorkerThreads;
		std::queue<std::function<void()>> m_Tasks;

		std::mutex m_Mutex;
		std::condition_variable m_CV;
		bool m_Stop = false;

		bool m_Initialized = false;
	};
}