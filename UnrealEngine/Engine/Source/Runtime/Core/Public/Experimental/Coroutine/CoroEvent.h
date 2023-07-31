// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Coroutine.h"

#if WITH_CPP_COROUTINES

/*
* FCoroEvent is an implementation of Coroutine Version of FEvent
*/
class FCoroEvent
{
	struct FNode : public TConcurrentLinearObject<FNode, CoroTask_Detail::FCoroBlockAllocationTag>
	{
		FLockedTask DummyTask;
		FNode* Next;
	};

	FORCEINLINE static FNode* InvalidNode()
	{
		return reinterpret_cast<FNode*>(~uintptr_t(0));
	}

	std::atomic<FNode*> Head {nullptr};

public:
	FCoroEvent()
	{
	}

	/*
	* trigger the event and unlocking all current and future awaiters
	*/
	void Trigger()
	{
		FNode* LocalNode = Head.exchange(InvalidNode(), std::memory_order_acquire);
		while (LocalNode && LocalNode != InvalidNode())
		{
			while(!LocalNode->DummyTask.HasSubsequent())
			{
				//This is only spinning for a very short time because the prerequisite is only set after we already left await_suspend but the Event is already in the List by this point.
				FPlatformProcess::Yield();
			}
			LocalNode->DummyTask.Unlock();
			FNode* Deleted = LocalNode;
			LocalNode = LocalNode->Next;
			delete Deleted;
		}
	}

	/*
	* trigger and reset the event and unlocking all current awaiters
	*/
	void Reset()
	{
		FNode* LocalNode = Head.exchange(nullptr, std::memory_order_acquire);
		while (LocalNode && LocalNode != InvalidNode())
		{
			while(!LocalNode->DummyTask.HasSubsequent())
			{
				//This is only spinning for a very short time because the prerequisite is only set after we already left await_suspend but the Event is already in the List by this point.
				FPlatformProcess::Yield();
			}
			LocalNode->DummyTask.Unlock();
			FNode* Deleted = LocalNode;
			LocalNode = LocalNode->Next;
			delete Deleted;
		}
	}

	inline bool await_ready() const noexcept
	{
		//returning false because we atomically determine if we need to suspend
		//and for this we need the Continuation which is only available in await_suspend
		return false;
	}

	template<typename PromiseType>
	inline bool await_suspend(coroutine_handle<PromiseType> Continuation) noexcept
	{
		FLockedTask DummyTask = FLockedTask::Create();
		const CoroTask_Detail::FPromise* DummyPromise = DummyTask.GetPromise();

		FNode* Node = new FNode();
		Node->DummyTask = MoveTemp(DummyTask);
		FNode* LocalHead = Head.load(std::memory_order_relaxed);
		do
		{
			if (LocalHead == InvalidNode())
			{
				delete Node;
				return false;
			}		
			Node->Next = LocalHead;
		} while(!Head.compare_exchange_weak(LocalHead, Node, std::memory_order_release));
		Continuation.promise().Suspend(DummyPromise);
		return true;
	}

	inline void await_resume() noexcept
	{
		//waiting does not return any values
	}
};

#endif