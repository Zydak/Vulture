#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace Vulture
{
	class FunctionQueue
	{
	public:
		FunctionQueue() = default;

		explicit FunctionQueue(const FunctionQueue& other) = delete;
		explicit FunctionQueue(FunctionQueue&& other) noexcept = delete;
		FunctionQueue& operator=(const FunctionQueue& other) = delete;
		FunctionQueue& operator=(FunctionQueue&& other) noexcept = delete;

		template<typename T, typename ...Args>
		void PushTask(T&& task, Args&& ... args)
		{
			m_Tasks.emplace(std::bind(std::forward<T>(task), std::forward<Args>(args)...));
		}

		void RunTasks()
		{
			while (m_Tasks.size() != 0)
			{
				m_Tasks.front()();
				m_Tasks.pop();
			}
		}

		inline std::queue<std::function<void()>>* GetQueue() { return &m_Tasks; }

	private:
		std::queue<std::function<void()>> m_Tasks;
	};
}