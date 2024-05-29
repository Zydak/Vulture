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

		explicit FunctionQueue(const FunctionQueue& other)
		{
			m_Tasks = other.m_Tasks;
		}

		explicit FunctionQueue(FunctionQueue&& other) noexcept
		{
			m_Tasks = std::move(other.m_Tasks);
		}

		FunctionQueue& operator=(const FunctionQueue& other)
		{
			m_Tasks = other.m_Tasks;
			return *this;
		}

		FunctionQueue& operator=(FunctionQueue&& other) noexcept
		{
			m_Tasks = std::move(other.m_Tasks);
			return *this;
		}

		template<typename T, typename ...Args>
		void PushTask(T&& task, Args&& ... args)
		{
			m_Tasks.emplace(std::bind(std::forward<T>(task), std::forward<Args>(args)...));
		}

		void RunTasks()
		{
			while (!m_Tasks.empty())
			{
				m_Tasks.front()();
				m_Tasks.pop();
			}
		}

		inline std::queue<std::function<void()>>* GetQueue() { return &m_Tasks; }

		void Merge(const FunctionQueue& other)
		{
			for (int i = 0; i < other.m_Tasks.size(); i++)
			{
				m_Tasks.push(other.m_Tasks.front());
			}
		}

	private:
		std::queue<std::function<void()>> m_Tasks;
	};
}