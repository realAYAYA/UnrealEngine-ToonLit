// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaWorkManager.h"
#include "UbaEvent.h"
#include "UbaPlatform.h"
#include "UbaThread.h"

namespace uba
{
	u32 WorkManager::TrackWorkStart(const tchar* desc)
	{
		if (auto t = m_workTracker.load())
			return t->TrackWorkStart(desc);
		return 0;
	}

	void WorkManager::TrackWorkEnd(u32 id)
	{
		if (auto t = m_workTracker.load())
			return t->TrackWorkEnd(id);
	}


	struct WorkManagerImpl::Worker
	{
		Worker(WorkManagerImpl& manager) : m_workAvailable(false)
		{
			m_loop = true;
			m_thread.Start([&]() { ThreadWorker(manager); return 0; });
		}
		~Worker()
		{
		}

		void Stop()
		{
			m_loop = false;
			m_workAvailable.Set();
			m_thread.Wait();
		}

		void ThreadWorker(WorkManagerImpl& manager)
		{
			manager.PushWorker(this);
			while (true)
			{
				if (!m_workAvailable.IsSet())
					break;
				if (!m_loop)
					break;

				while (true)
				{
					while (true)
					{
						SCOPED_WRITE_LOCK(manager.m_workLock, lock);
						if (manager.m_work.empty())
							break;
						Work work = manager.m_work.front();
						manager.m_work.pop_front();
						lock.Leave();

						TrackWorkScope tws(manager, work.desc.c_str());
						work.func();
					}

					SCOPED_WRITE_LOCK(manager.m_availableWorkersLock, lock1);
					SCOPED_READ_LOCK(manager.m_workLock, lock2);
					if (!manager.m_work.empty())
						continue;

					manager.PushWorkerNoLock(this);
					break;
				}
			}
		}

		Worker* m_nextWorker = nullptr;
		Worker* m_prevWorker = nullptr;
		Atomic<bool> m_loop;
		Event m_workAvailable;
		Thread m_thread;
	};

	WorkManagerImpl::WorkManagerImpl(u32 workerCount)
	{
		m_workers.resize(workerCount);
		m_activeWorkerCount = workerCount;
		for (u32 i = 0; i != workerCount; ++i)
			m_workers[i] = new Worker(*this);
	}

	WorkManagerImpl::~WorkManagerImpl()
	{
		for (auto worker : m_workers)
			worker->Stop();
		for (auto worker : m_workers)
			delete worker;
	}


	void WorkManagerImpl::AddWork(const Function<void()>& work, u32 count, const tchar* desc)
	{
		SCOPED_WRITE_LOCK(m_workLock, lock);
		for (u32 i = 0; i != count; ++i)
		{
			m_work.push_back({ work });
			if (m_workTracker.load())
				m_work.back().desc = desc;
		}
		lock.Leave();

		SCOPED_WRITE_LOCK(m_availableWorkersLock, lock2);
		while (count--)
		{
			Worker* worker = PopWorkerNoLock();
			if (!worker)
				break;
			worker->m_workAvailable.Set();
		}
	}

	u32 WorkManagerImpl::GetWorkerCount()
	{
		return u32(m_workers.size());
	}

	void WorkManagerImpl::PushWorker(Worker* worker)
	{
		SCOPED_WRITE_LOCK(m_availableWorkersLock, lock);
		PushWorkerNoLock(worker);
	}

	void WorkManagerImpl::PushWorkerNoLock(Worker* worker)
	{
		if (m_firstAvailableWorker)
			m_firstAvailableWorker->m_prevWorker = worker;
		worker->m_prevWorker = nullptr;
		worker->m_nextWorker = m_firstAvailableWorker;
		m_firstAvailableWorker = worker;
		--m_activeWorkerCount;
	}

	void WorkManagerImpl::DoWork(u32 count)
	{
		while (count--)
		{
			SCOPED_WRITE_LOCK(m_workLock, lock);
			if (m_work.empty())
				break;
			Work work = m_work.front();
			m_work.pop_front();
			lock.Leave();

			TrackWorkScope tws(*this, work.desc.c_str());
			work.func();
		}
	}

	void WorkManagerImpl::FlushWork()
	{
		while (true)
		{
			SCOPED_READ_LOCK(m_workLock, lock);
			bool workEmpty = m_work.empty();
			lock.Leave();
			if (workEmpty)
				break;
			Sleep(5);
		}
		while (m_activeWorkerCount)
			Sleep(5);
	}

	WorkManagerImpl::Worker* WorkManagerImpl::PopWorkerNoLock()
	{
		Worker* worker = m_firstAvailableWorker;
		if (!worker)
			return nullptr;
		m_firstAvailableWorker = worker->m_nextWorker;
		if (m_firstAvailableWorker)
			m_firstAvailableWorker->m_prevWorker = nullptr;
		++m_activeWorkerCount;
		return worker;
	}
}
