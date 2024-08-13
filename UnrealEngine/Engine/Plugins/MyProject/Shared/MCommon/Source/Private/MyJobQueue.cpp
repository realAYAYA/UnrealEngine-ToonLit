#include "MyJobQueue.h"

FMyJobQueue::FMyJobQueue()
{
}

FMyJobQueue::~FMyJobQueue()
{
}

void FMyJobQueue::PostJob(const TFunction<void()>& Func)
{
	const auto Job = MakeShared<FJob, ESPMode::ThreadSafe>();
	Job->Task = Func;

	{
		FScopeLock Lock(&Mutex);
		IncomingQueue.Enqueue(Job);
	}
}

void FMyJobQueue::PostQuitJob(const TFunction<void()>& Func)
{
	const auto Job = MakeShared<FJob, ESPMode::ThreadSafe>();
	Job->Task = Func;
	Job->bQuit = true;

	{
		FScopeLock Lock(&Mutex);
		IncomingQueue.Enqueue(Job);
	}	
}

uint32 FMyJobQueue::DoWork()
{
	bool bQuit = false;
	uint32 Num = 0;
	do
	{
		ReloadWorkQueue();
		if (WorkQueue.IsEmpty())
			break;

		FJobPtr Job;
		while (WorkQueue.Dequeue(Job))
		{
			Job->Task();
			++Num;
			
			if (Job->bQuit)
			{
				bQuit = true;
				break;
			}
			Job.Reset();
		}

		if (bQuit)
		{
			break;
		}
	} while(false);

	if (bQuit)
	{
		WorkQueue.Empty();
		ReloadWorkQueue();
		WorkQueue.Empty();
		bRunning = false;
	}
	
	return Num;
}

void FMyJobQueue::Clear()
{
	FScopeLock Lock(&Mutex);
	IncomingQueue.Empty();
	WorkQueue.Empty();
}

bool FMyJobQueue::IsRunning() const
{
	return bRunning;
}

void FMyJobQueue::ReloadWorkQueue()
{
	if (!WorkQueue.IsEmpty())
		return;

	{
		FScopeLock Lock(&Mutex);
		Swap(IncomingQueue, WorkQueue);
	}
}
