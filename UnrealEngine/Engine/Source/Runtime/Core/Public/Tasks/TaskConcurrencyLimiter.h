// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/Task.h"
#include "Containers/LockFreeList.h"
#include "Templates/SharedPointer.h"
#include "Experimental/ConcurrentLinearAllocator.h"
#include "CoreTypes.h"

#include <AtomicQueue.h>

namespace UE::Tasks
{
	namespace TaskConcurrencyLimiter_Private
	{
		// a queue of free slots in range [0 .. max_concurrency). Initially contains all slots in the range.
		class FConcurrencySlots
		{
		public:
			explicit FConcurrencySlots(uint32 MaxConcurrency)
				: FreeSlots(MaxConcurrency)
			{
				for (uint32 Index = IndexOffset; Index < MaxConcurrency + IndexOffset; ++Index)
				{
					FreeSlots.push(Index);
				}
			}

			bool Alloc(uint32& Slot)
			{
				if (FreeSlots.try_pop(Slot))
				{
					Slot -= IndexOffset;
					return true;
				}

				return false;
			}

			void Release(uint32 Slot)
			{
				FreeSlots.push(Slot + IndexOffset);
			}

		private:
			// this queue uses 0 as a special "null" value. to work around this, slots are shifted by one for storage, thus ending up in 
			// [1 .. max_concurrency] range
			static constexpr int32 IndexOffset = 1;
			atomic_queue::AtomicQueueB<uint32> FreeSlots; // a bounded lock-free FIFO queue
		};

		// an implementation details of FTaskConcurrenctyLimiter
		class FPimpl : public TSharedFromThis<FPimpl>
		{
		public:
			explicit FPimpl(uint32 InMaxConcurrency, ETaskPriority InTaskPriority)
				: ConcurrencySlots(InMaxConcurrency)
				, TaskPriority(InTaskPriority)
			{
			}

			CORE_API ~FPimpl();

			template<typename TaskFunctionType>
			void Push(const TCHAR* DebugName, TaskFunctionType&& TaskFunction)
			{
				TSharedPtr<LowLevelTasks::FTask> Task = MakeShared<LowLevelTasks::FTask>();

				Task->Init(
					DebugName,
					TaskPriority,
					[
						TaskFunction = MoveTemp(TaskFunction),
						this,
						Pimpl = TSharedFromThis<FPimpl>::AsShared(), // to keep it alive
						Task  // self-destruct
					]()
					{
						// We can't pass the ConcurrencySlot in the lambda during creation as
						// it's not actually acquired yet. The value will be passed using
						// the user data when the task is launched.
						uint32 ConcurrencySlot = (uint32)(UPTRINT)Task->GetUserData();

						TaskFunction(ConcurrencySlot);
						CompleteWorkItem(ConcurrencySlot);
					}
				);

				AddWorkItem(Task.Get());
			}

			bool CORE_API Wait(FTimespan Timeout);

		private:
			void CORE_API AddWorkItem(LowLevelTasks::FTask* Task);
			void CORE_API ProcessQueue(uint32 ConcurrencySlot, bool bSkipFirstWakeUp);
			void CORE_API ProcessQueueFromWorker(uint32 ConcurrencySlot);
			void CORE_API ProcessQueueFromPush(uint32 ConcurrencySlot);
			void CORE_API CompleteWorkItem(uint32 ConcurrencySlot);

			FConcurrencySlots ConcurrencySlots; // free slots queue. used also to limit concurrency
			ETaskPriority TaskPriority;
			TLockFreePointerListFIFO<LowLevelTasks::FTask, PLATFORM_CACHE_LINE_SIZE> WorkQueue; // a queue of user-provided task functions
			std::atomic<uint32> NumWorkItems { 0 };
			std::atomic<FEvent*> CompletionEvent { nullptr };
		};

	} // namespace TaskConcurrencyLimiter_Private

	/**
	* A lightweight construct that limits the concurrency of tasks pushed into it. 
	*
	* @note This class supports being destroyed before the tasks it contains are finished.
	*/
	class FTaskConcurrencyLimiter
	{
	public:
		/**
		 * Constructor.
		 *
		 * @param MaxConcurrency     How wide the processing can go.
		 * @param TaskPriority       Priority the tasks will be launched with.
		 */
		explicit FTaskConcurrencyLimiter(uint32 MaxConcurrency, ETaskPriority TaskPriority = ETaskPriority::Default)
			: Pimpl(MakeShared<TaskConcurrencyLimiter_Private::FPimpl>(MaxConcurrency, TaskPriority))
		{
		}

		/**
		 * Push a new task.
		 *
		 * @param DebugName    Helps to identify the task in debugger and profiler.
		 * @param TaskFunction A callable with a slot parameter, usually a lambda but can be also a functor object 
		 *                     or a pointer to a function. The slot parameter is an index in [0..max_concurrency) range, 
		 *                     unique at any moment of time, that can be used in user code to index a fixed-size buffer. 
		 *                     See `TaskConcurrencyLimiterStressTest()` for an example.
		 */
		template<typename TaskFunctionType>
		void Push(const TCHAR* DebugName, TaskFunctionType&& TaskFunction)
		{
			Pimpl->Push(DebugName, MoveTemp(TaskFunction));
		}

		/**
		 * Waits for task's completion with timeout.
		 *
		 * @param  Timeout Maximum amount of time to wait for tasks to finish before returning.
		 * @return true if all tasks are completed, false otherwise.
		 * 
		 * @note   A wait is satisfied once the internal task counter reaches 0 and is never reset
		 *         afterward when more tasks are added. A new FTaskConcurrencyLimiter can be used
		 *         for such a use case.
		 */
		bool Wait(FTimespan Timeout = FTimespan::MaxValue())
		{
			return Pimpl->Wait(Timeout);
		}

	private:
		TSharedRef<TaskConcurrencyLimiter_Private::FPimpl> Pimpl;
	};

} // namespace UE::Tasks