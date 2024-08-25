// Copyright (C) 2016 Dmitry Vyukov <dvyukov@google.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

// This implementation is based on EventCount.h
// included in the Eigen library but almost everything has been
// rewritten.

#pragma once 

#include "Async/Fundamental/TaskShared.h"
#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "HAL/Event.h"

#include <atomic>

namespace LowLevelTasks::Private
{
	enum class EWaitState
	{
		NotSignaled = 0,
		Waiting,
		Signaled,
	};

	/*
	* the struct is naturally 64 bytes aligned, the extra alignment just
	* re-enforces this assumption and will error if it changes in the future
	*/
	struct alignas(64) FWaitEvent
	{
		std::atomic<uint64_t>     Next{ 0 };
		uint64_t                  Epoch{ 0 };
		std::atomic<EWaitState>   State{ EWaitState::NotSignaled };
		FEventRef                 Event{ EEventMode::ManualReset };
	};

	class FWaitingQueue
	{
		// State_ layout:
		// - low kWaiterBits is a stack of waiters committed wait
		//   (indexes in NodesArray are used as stack elements,
		//   kStackMask means empty stack).
		// - next kWaiterBits is count of waiters in prewait state.
		// - next kWaiterBits is count of pending signals.
		// - remaining bits are ABA counter for the stack.
		//   (stored in Waiter node and incremented on push).
		static constexpr uint64_t WaiterBits = 14;
		static constexpr uint64_t StackMask = (1ull << WaiterBits) - 1;
		static constexpr uint64_t WaiterShift = WaiterBits;
		static constexpr uint64_t WaiterMask = ((1ull << WaiterBits) - 1) << WaiterShift;
		static constexpr uint64_t WaiterInc = 1ull << WaiterShift;
		static constexpr uint64_t SignalShift = 2 * WaiterBits;
		static constexpr uint64_t SignalMask = ((1ull << WaiterBits) - 1) << SignalShift;
		static constexpr uint64_t SignalInc = 1ull << SignalShift;
		static constexpr uint64_t EpochShift = 3 * WaiterBits;
		static constexpr uint64_t EpochBits = 64 - EpochShift;
		static constexpr uint64_t EpochMask = ((1ull << EpochBits) - 1) << EpochShift;
		static constexpr uint64_t EpochInc = 1ull << EpochShift;

		std::atomic<uint64_t>      State{ StackMask };
		TAlignedArray<FWaitEvent>& NodesArray;

	public:
		FWaitingQueue(TAlignedArray<FWaitEvent>& NodesArray)
			: NodesArray(NodesArray)
		{
		}

		CORE_API void Init();
		CORE_API void Shutdown();
		CORE_API void PrepareWait(FWaitEvent* Node);
		CORE_API bool CommitWait(FWaitEvent* Node, FOutOfWork& OutOfWork, int32 SpinCycles, int32 WaitCycles);

		// returns true if we need to wake up a new worker
		CORE_API bool CancelWait(FWaitEvent* Node);

		int32 Notify(int32 Count = 1)
		{
			return NotifyInternal(Count);
		}

		int32 NotifyAll()
		{
			return NotifyInternal(MAX_int32);
		}

	private:
		CORE_API int32 NotifyInternal(int32 Count);
		CORE_API void  Park(FWaitEvent* Node, FOutOfWork& OutOfWork, int32 SpinCycles, int32 WaitCycles);
		CORE_API int32 Unpark(FWaitEvent* InNode);
		CORE_API void  CheckState(uint64_t state, bool waiter = false);
	};

} // namespace LowLevelTasks::Private