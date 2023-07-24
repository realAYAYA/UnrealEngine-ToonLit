// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/DebugDrawQueue.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"

#if CHAOS_DEBUG_DRAW

namespace Chaos
{
	void FDebugDrawQueue::SetConsumerActive(void* Consumer, bool bConsumerActive)
	{
		FScopeLock Lock(&ConsumersCS);
	
		if(bConsumerActive)
		{
			Consumers.AddUnique(Consumer);
		}
		else
		{
			Consumers.Remove(Consumer);
		}
	
		NumConsumers = Consumers.Num();
	}
	
	FDebugDrawQueue& FDebugDrawQueue::GetInstance()
	{
		static FDebugDrawQueue* PSingleton = nullptr;
		if(PSingleton == nullptr)
		{
			static FDebugDrawQueue Singleton;
			PSingleton = &Singleton;
		}
		return *PSingleton;
	}
}

#endif
