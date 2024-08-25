// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/Pipe.h"

#include "Containers/Array.h"
#include "Misc/ScopeExit.h"

namespace UE::Tasks
{
	Private::FTaskBase* FPipe::PushIntoPipe(Private::FTaskBase& Task)
	{
		Task.AddRef(); // the pipe holds a ref to the last task, until it's replaced by the next task or cleared on completion
		Private::FTaskBase* LastTask_Local = LastTask.exchange(&Task, std::memory_order_acq_rel); // `acq_rel` to order task construction before
		// its usage by a thread that replaces it as the last piped task
		checkf(LastTask_Local != &Task, TEXT("Dependency cycle: adding itself as a prerequisite (or use after destruction)"));

		if (LastTask_Local == nullptr)
		{
			return nullptr;
		}

		if (!LastTask_Local->AddSubsequent(Task))
		{
			// the last task doesn't accept subsequents anymore because it's already completed (happened concurrently after we replaced it as
			// the last pipe's task
			LastTask_Local->Release(); // the pipe doesn't need it anymore
			return nullptr;
		}

		return LastTask_Local; // transfer the reference to the caller that must release it
	}

	bool FPipe::TryClearTask(Private::FTaskBase& Task)
	{
		Private::FTaskBase* Task_Local = &Task;
		// try clearing the task if it's still pipe's "last task". if succeeded, release the ref accounted for pipe's last task. otherwise whoever replaced it
		// as the last task will do this
		if (LastTask.compare_exchange_strong(Task_Local, nullptr, std::memory_order_acquire, std::memory_order_relaxed))
		{
			Task.Release(); // it was still pipe's last task. now that we cleared it, release the reference
			return true;
		}

		return false;
	}

	bool FPipe::WaitUntilEmpty(FTimespan Timeout/* = FTimespan::MaxValue()*/)
	{
		struct FPlaceholderTask final : Private::FTaskBase
		{
			FPlaceholderTask()
				// InitRefCount: one for the initial local ref, and one for the internal reference
				: Private::FTaskBase(/*InitRefCount =*/ 2)
			{
				Init(
					TEXT("Pipe::WaitUntilEmpty() placeholder"),
					ETaskPriority::Normal, // doesn't matter
					EExtendedTaskPriority::TaskEvent, // no need for scheduling or execution for this dummy task
					ETaskFlags::None
				);
			}

			virtual void ExecuteTask() override final
			{
				checkNoEntry(); // the method won't be called because the task was initialized with `EExtendedTaskPriority::TaskEvent`
			}
		};

		bool bRes = true;

		TRefCountPtr<FPlaceholderTask> PlaceholderTask{ new FPlaceholderTask, /*bAddRef =*/ false }; // the initial ref was already accounted for
		Private::FTaskBase* LastTask_Local = PushIntoPipe(*PlaceholderTask);
		if (LastTask_Local)
		{
			LastTask_Local->Release(); // release pipe's ref to the last task
			// when the placeholder replaced "the last task" in the pipe, it became its subsequent, so the last task will "complete" it
			bRes = PlaceholderTask->Wait(FTimeout{ Timeout });
		}
		else
		{
			// there was no "last task" in the pipe, so we have to launch the placeholder to complete it to not assert on its destruction
			PlaceholderTask->TryLaunch(/*TaskSize =*/ 0);
			checkSlow(PlaceholderTask->IsCompleted());
		}

		verifyf(TryClearTask(*PlaceholderTask), TEXT("More tasks were launched (concurrently) in the pipe after `WaitUntilEmpty()` was called"));
		
		return bRes;
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
			CallStack.Pop(EAllowShrinking::No);
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