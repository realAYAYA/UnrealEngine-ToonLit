// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaMemory.h"

namespace uba
{
	class WorkTracker
	{
	public:
		virtual u32 TrackWorkStart(const tchar* desc) { return 0; }
		virtual void TrackWorkEnd(u32 id) {}
	};

	class WorkManager
	{
	public:
		virtual void AddWork(const Function<void()>& work, u32 count, const tchar* desc) = 0;
		virtual u32 GetWorkerCount() = 0;

		u32 TrackWorkStart(const tchar* desc);
		void TrackWorkEnd(u32 id);

		void SetWorkTracker(WorkTracker* workTracker) { m_workTracker = workTracker; }
		WorkTracker* GetWorkTracker() { return m_workTracker; }

	protected:
		Atomic<WorkTracker*> m_workTracker;
	};


	class WorkManagerImpl : public WorkManager
	{
	public:
		WorkManagerImpl(u32 workerCount);
		virtual ~WorkManagerImpl();
		virtual void AddWork(const Function<void()>& work, u32 count, const tchar* desc) override;
		virtual u32 GetWorkerCount() override;
		void DoWork(u32 count = 1);
		void FlushWork();

	private:
		struct Worker;
		void PushWorker(Worker* worker);
		void PushWorkerNoLock(Worker* worker);
		Worker* PopWorkerNoLock();

		Vector<Worker*> m_workers;
		struct Work { Function<void()> func; TString desc; };
		ReaderWriterLock m_workLock;
		List<Work> m_work;
		Atomic<u32> m_activeWorkerCount;
		Atomic<u32> m_workCounter;

		ReaderWriterLock m_availableWorkersLock;
		Worker* m_firstAvailableWorker = nullptr;
	};

	struct TrackWorkScope
	{
		TrackWorkScope(WorkManager& wm, const tchar* desc) : workManager(wm), workIndex(wm.TrackWorkStart(desc)) {}
		~TrackWorkScope() { workManager.TrackWorkEnd(workIndex); }
		WorkManager& workManager;
		u32 workIndex;
	};
}
