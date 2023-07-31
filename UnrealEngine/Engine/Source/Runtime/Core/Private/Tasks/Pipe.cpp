// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/Pipe.h"

#include "Containers/Array.h"
#include "Misc/ScopeExit.h"

namespace UE::Tasks
{
	Private::FTaskBase* FPipe::PushIntoPipe(Private::FTaskBase& Task)
	{
		// critical section: between exchanging tasks and adding the previous LastTask to prerequisites.
		// `LastTask` can be in the process of destruction. Before this it will try to clear itself from the pipe
		// we need to let it know we're still using it so destruction should wait until we finished
		PushingThreadsNum.fetch_add(1, std::memory_order_relaxed);
		ON_SCOPE_EXIT{ PushingThreadsNum.fetch_sub(1, std::memory_order_relaxed); };

		Private::FTaskBase* LastTask_Local = LastTask.exchange(&Task, std::memory_order_release);
		checkf(LastTask_Local != &Task, TEXT("Dependency cycle: adding itself as a prerequisite (or use after destruction)"));

		if (LastTask_Local == nullptr || !LastTask_Local->AddSubsequent(Task))
		{
			return nullptr;
		}

		LastTask_Local->AddRef(); // keep it alive, must be called before leaving this function, it's the caller's duty to release it
		return LastTask_Local;
	}

	void FPipe::ClearTask(Private::FTaskBase& Task)
	{
		// clears `Task` from the pipe if it's still the `LastTask`. if it's not, another task already has set itself as the `LastTask` 
		// and can be in the process of adding this `Task` as a prerequisite. We need to wait for this to complete before returning 
		// from the call as the `Task` can be destroyed immediately after this
		Private::FTaskBase* Task_Local = &Task;
		if (!LastTask.compare_exchange_strong(Task_Local, nullptr, std::memory_order_acquire, std::memory_order_relaxed))
		{
			while (PushingThreadsNum.load(std::memory_order_relaxed) != 0)
			{
			}
		}
	}

	// Maintains pipe callstack. Due to busy waiting tasks from multiple pipes can be executed nested.
	class FPipeCallStack
	{
	public:
		static void Push(const FPipe& Pipe)
		{
			CallStack.Add(&Pipe);
		}

		static void Pop(const FPipe& Pipe)
		{
			check(CallStack.Last() == &Pipe);
			CallStack.Pop(/*bAllowShrinking=*/ false);
		}

		// returns true if a task from the given pipe is being being executed on the top of the stack.
		// the method deliberately doesn't look deeper because even if the pipe is there and technically it's safe to assume
		// accessing a resource protected by a pipe is thread-safe, logically it's a bug because it's an accidental condition
		static bool IsOnTop(const FPipe& Pipe)
		{
			return CallStack.Num() != 0 && CallStack.Last() == &Pipe;
		}

	private:
		static thread_local TArray<const FPipe*> CallStack;
	};

	thread_local TArray<const FPipe*> FPipeCallStack::CallStack;

	void FPipe::ExecutionStarted()
	{
		FPipeCallStack::Push(*this);
	}

	void FPipe::ExecutionFinished()
	{
		FPipeCallStack::Pop(*this);
	}

	bool FPipe::IsInContext() const
	{
		return FPipeCallStack::IsOnTop(*this);
	}
}