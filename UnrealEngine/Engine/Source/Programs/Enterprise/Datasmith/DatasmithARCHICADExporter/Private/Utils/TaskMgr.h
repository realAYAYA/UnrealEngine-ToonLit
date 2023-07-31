// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

#include "Progression.h"

#include "Condition.hpp"
#include "Lock.hpp"

#include <queue>
#include <memory>

BEGIN_NAMESPACE_UE_AC

class FTaskMgr
{
  public:
	// Task class, your class must inherit from this class.
	class FTask
	{
	  public:
		virtual void Run() = 0;
		virtual ~FTask() {}
	};

	// Constructor
	FTaskMgr();

	// Destructor
	~FTaskMgr();

	// Task can be added for schedule or run immediately in the current thread).
	enum ERunMode
	{
		kSchedule,
		kRunNow
	};

	// Add task (if inRunNow, the task is run immediately in current thread)
	void AddTask(FTask* InTask, ERunMode InMode = kSchedule);

	// Wait until all task have been processed
	void Join(FProgression* InProgression = nullptr);

	bool EnableThreading(bool InEnable)
	{
		bool tmp = bThreadingEnabled;
		bThreadingEnabled = InEnable;
		return tmp;
	}

	static FTaskMgr* GetMgr();

	static void DeleteMgr();

  private:
	// Get the next task to be run
	FTask* GetTask();

	// Class that implement CTaskMgr thread
	class FThread;

	// Threads used (≈ number of cpu)
	std::vector< std::unique_ptr< FThread > > Threads;

	// Fifo tasks queue
	std::queue< FTask* > TaskQueue;

	// Control access on this object (for queue operations)
	GS::Lock AccessControl;

	// Condition variable
	GS::Condition CV;

	// Flag to signal that we are deleting this task mgr
	volatile bool bTerminate;

	// Number of thread waiting
	volatile int NbTaskWaiting;

	// False: tasks to run in main threads, true: Task run in thread
	bool bThreadingEnabled;
};

END_NAMESPACE_UE_AC
