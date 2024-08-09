// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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

		~FunctionQueue()
		{
			m_Tasks.clear();
		}

		template<typename T, typename ...Args>
		void PushTask(T&& task, Args&& ... args)
		{
			m_Tasks.emplace_back(std::bind(std::forward<T>(task), std::forward<Args>(args)...));
		}

		void RunTasks()
		{
			for (int i = 0; i < m_Tasks.size(); i++)
			{
				m_Tasks[i]();
			}
		}

		void Clear()
		{
			m_Tasks.clear();
		}

		inline std::vector<std::function<void()>>* GetQueue() { return &m_Tasks; }

		void Merge(const FunctionQueue& other)
		{
			for (int i = 0; i < other.m_Tasks.size(); i++)
			{
				m_Tasks.push_back(other.m_Tasks.front());
			}
		}

	private:
		std::vector<std::function<void()>> m_Tasks;
	};
}