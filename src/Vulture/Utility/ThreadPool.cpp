// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "ThreadPool.h"
#include "Vulkan/Device.h"

namespace Vulture
{

	ThreadPool::ThreadPool(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	ThreadPool::ThreadPool(const ThreadPool& other)
	{
		Init({ (uint32_t)other.m_WorkerThreads.size() });
	}

	ThreadPool& ThreadPool::operator=(const ThreadPool& other)
	{
		Init(CreateInfo{ other.GetThreadCount() });
		return *this;
	}

	ThreadPool::~ThreadPool()
	{
		Destroy();
	}

	void ThreadPool::Init(const CreateInfo& createInfo)
	{
		for (uint32_t i = 0; i < createInfo.threadCount; i++)
		{
			m_WorkerThreads.emplace_back([this] {
				Device::CreateCommandPoolForThread();
				while (true)
				{
					std::unique_lock<std::mutex> lock(m_Mutex);
					m_CV.wait(lock, [this] { return m_Stop || !m_Tasks.empty(); });
					if (m_Stop && m_Tasks.empty())
						return;

					auto task = std::move(m_Tasks.front());
					m_Tasks.pop();
					lock.unlock();
					task();
				}
				});
		}

		m_Initialized = true;
	}

	void ThreadPool::Destroy()
	{
		if (!m_Initialized)
			return;

		std::unique_lock<std::mutex> lock(m_Mutex);
		m_Stop = true;
		lock.unlock();
		m_CV.notify_all();
		for (auto& worker : m_WorkerThreads)
		{
			worker.join();
		}

		Reset();
	}

	void ThreadPool::Reset()
	{
		m_WorkerThreads.clear();
		while (!m_Tasks.empty())
			m_Tasks.pop();

		m_Stop = false;
		m_Initialized = false;
	}

}