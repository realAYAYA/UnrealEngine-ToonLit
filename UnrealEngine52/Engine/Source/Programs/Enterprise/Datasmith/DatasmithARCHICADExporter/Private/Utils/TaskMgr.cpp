// Copyright Epic Games, Inc. All Rights Reserved.

#include "TaskMgr.h"
#include "CurrentOS.h"

#include "Thread.hpp"
#include "Guard.hpp"
#include "System.hpp"

#include <stdexcept>

BEGIN_NAMESPACE_UE_AC

static FTaskMgr* STaskMgr;

FTaskMgr* FTaskMgr::GetMgr()
{
	if (STaskMgr == nullptr)
	{
		STaskMgr = new FTaskMgr();
	}
	return STaskMgr;
}

void FTaskMgr::DeleteMgr()
{
	if (STaskMgr)
	{
		FTaskMgr* Tmp = STaskMgr;
		STaskMgr = nullptr;
		delete Tmp;
	}
}

// Class that implement FTaskMgr thread using GS::Thread
class FTaskMgr::FThread
{
  public:
	// Constructor
	FThread(FTaskMgr* InTaskMgr, const GS::UniString& inThreadName)
		: Task(new FTaskRunner(InTaskMgr))
		, Thread(Task, inThreadName)
	{
		Thread.Start();
	}

	// Start the thread
	void Start() { Thread.Start(); }

	// Join the thread
	void Join() { Thread.Join(); }

  private:
	// Implement a GS::Runnable that will run tasks
	class FTaskRunner : public GS::Runnable
	{
	  public:
		// Constructor
		FTaskRunner(FTaskMgr* InTaskMgr)
			: TaskMgr(InTaskMgr)
		{
		}

		// Run task after task
		void Run()
		{
#if PLATFORM_WINDOWS
			SetThreadName(GS::Thread::GetCurrent().GetName().ToUtf8());
#else
			pthread_setname_np(GS::Thread::GetCurrent().GetName().ToUtf8());
#endif
			FTask* ATask = TaskMgr->GetTask();
			while (ATask)
			{
				try
				{
					ATask->Run();
					delete ATask;
				}
				catch (std::exception& e)
				{
					UE_AC_DebugF("FTaskMgr::FTaskRunner::Run - Catch std exception %s\n", e.what());
				}
				catch (GS::GSException& gs)
				{
					UE_AC_DebugF("FTaskMgr::FTaskRunner::Run - Catch gs exception %s\n", gs.GetMessage().ToUtf8());
				}
				catch (...)
				{
					UE_AC_DebugF("FTaskMgr::FTaskRunner::Run - Catch unknown exception\n");
				}
				ATask = TaskMgr->GetTask();
			}
		}

	  private:
		FTaskMgr* TaskMgr;
	};

	GS::RunnableTask Task;
	GS::Thread		 Thread;
};

// Constructor
FTaskMgr::FTaskMgr()
	: bTerminate(false)
	// Temporarily disabling multi-threading to fix UE-127453 for 5.0
	// #ue_archicad : Must revisit multi-threading to find proper solution for UE-127453 
	, bThreadingEnabled(false)
	, NbTaskWaiting(0)
	, CV(AccessControl)
{
	if (bThreadingEnabled)
	{
		// One thread by processor
		unsigned nbProcessors = GS::System::GetNumberOfActiveProcessors();
		if (nbProcessors == 0)
		{
			bThreadingEnabled = false;
		}
		Threads.resize(nbProcessors);
		for (unsigned i = 0; i < nbProcessors; i++)
		{
			Threads[i].reset(new FThread(this, GS::UniString::Printf("Datasmith Exporter Task #%d", i)));
		}
	}
}

// Destructor
FTaskMgr::~FTaskMgr()
{
	{
		GS::Guard< GS::Lock > lck(AccessControl);
		bTerminate = true;
	}
	CV.NotifyAll();

	size_t nbThreads = Threads.size();
	for (size_t i = 0; i < nbThreads; i++)
	{
		Threads[i]->Join();
	}

	// Delete all not processed task. This happen when Join has'n been called on the thread manager
	if (!TaskQueue.empty())
	{
		FTask* Task = TaskQueue.front();
		TaskQueue.pop();
		delete Task;
	}
}

// Add task
void FTaskMgr::AddTask(FTask* InTask, ERunMode inMode)
{
	if (bThreadingEnabled && inMode == kSchedule)
	{
		GS::Guard< GS::Lock > lck(AccessControl);
		if (bTerminate)
		{
			throw std::runtime_error("Adding task to a terminated FTaskMgr");
		}
		TaskQueue.push(InTask);
		CV.Notify();
	}
	else
	{
		InTask->Run();
		delete InTask;
	}
}

// Wait until all task have been processed
void FTaskMgr::Join(FProgression* inProgression)
{
	GS::Guard< GS::Lock > lck(AccessControl);
	while (!TaskQueue.empty() || NbTaskWaiting != Threads.size())
	{
		CV.NotifyAll();
		CV.Wait(10, true, nullptr);
		if (inProgression)
		{
			inProgression->Update();
		}
	}
#if 0
	UE_AC_TraceF("FTaskMgr::Join %lu %u\n", (unsigned long)mTaskQueue.size(), mNbTaskWaiting);
#endif
}

// Get the next task to be run
FTaskMgr::FTask* FTaskMgr::GetTask()
{
	CV.NotifyAll();
	GS::Guard< GS::Lock > lck(AccessControl);
	NbTaskWaiting++;
	FTask* Task = nullptr;
	while (Task == nullptr && !bTerminate)
	{
		CV.Wait(100, true, nullptr);
		if (!TaskQueue.empty())
		{
			Task = TaskQueue.front();
			TaskQueue.pop();
			NbTaskWaiting--;
		}
	}
	return Task;
}

END_NAMESPACE_UE_AC
