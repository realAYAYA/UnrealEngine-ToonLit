// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventManager.h"

namespace Chaos
{
	void FEventManager::Reset()
	{
		ContainerLock.WriteLock();
		for (FEventContainerBasePtr Container : EventContainers)
		{
			delete Container;
			Container = nullptr;
		}
		EventContainers.Reset();
		ContainerLock.WriteUnlock();
	}

	void FEventManager::UnregisterEvent(const EEventType& EventType)
	{
		const FEventID EventID = (FEventID)EventType;
		ContainerLock.WriteLock();
		if (EventID < EventContainers.Num())
		{
			delete EventContainers[EventID];
			EventContainers.RemoveAt(EventID);
		}
		ContainerLock.WriteUnlock();
	}

	void FEventManager::UnregisterHandler(const EEventType& EventType, const void* InHandler)
	{
		const FEventID EventID = (FEventID)EventType;
		ContainerLock.ReadLock();
		checkf(EventID < EventContainers.Num(), TEXT("Unregistering event Handler for an event ID that does not exist"));
		EventContainers[EventID]->UnregisterHandler(InHandler);
		ContainerLock.ReadUnlock();
	}

	void FEventManager::FillProducerData(const Chaos::FPBDRigidsSolver* Solver, bool bResetData)
	{
		if (BufferMode == EMultiBufferMode::Double)
		{
			ResourceLock.ReadLock();
		}
		ContainerLock.ReadLock();
		for (FEventContainerBasePtr EventContainer : EventContainers)
		{
			if (EventContainer)
			{
				EventContainer->InjectProducerData(Solver, bResetData);
			}
		}
		ContainerLock.ReadUnlock();
		if (BufferMode == EMultiBufferMode::Double)
		{
			ResourceLock.ReadUnlock();
		}
	}

	void FEventManager::FlipBuffersIfRequired()
	{
		if (BufferMode == EMultiBufferMode::Single)
		{
			return;
		}
		else if (BufferMode == EMultiBufferMode::Double)
		{
			ResourceLock.WriteLock();
		}

		ContainerLock.ReadLock();
		for (FEventContainerBasePtr EventContainer : EventContainers)
		{
			if (EventContainer)
			{
				EventContainer->ResetConsumerBuffer();
				EventContainer->FlipBufferIfRequired();
			}
		}
		ContainerLock.ReadUnlock();

		if (BufferMode == EMultiBufferMode::Double)
		{
			ResourceLock.WriteUnlock();
		}
	}

	void FEventManager::DispatchEvents()
	{
		if (BufferMode == EMultiBufferMode::Double)
		{
			ResourceLock.ReadLock();
		}

		ContainerLock.ReadLock();
		for (FEventContainerBasePtr EventContainer : EventContainers)
		{
			if (EventContainer)
			{
				EventContainer->DispatchConsumerData();
			}
		}
		ContainerLock.ReadUnlock();

		if (BufferMode == EMultiBufferMode::Double)
		{
			ResourceLock.ReadUnlock();
		} 
		else if (BufferMode == EMultiBufferMode::Single)
		{
			for (FEventContainerBasePtr EventContainer : EventContainers)
			{
				if (EventContainer)
				{
					EventContainer->ResetConsumerBuffer();
					EventContainer->FlipBufferIfRequired();
				}
			}
		}
	}

	void FEventManager::InternalRegisterInjector(const FEventID& EventID, const FEventContainerBasePtr& Container)
	{
		if (EventID > EventContainers.Num())
		{
			for (int i = EventContainers.Num(); i < EventID; i++)
			{
				EventContainers.Push(nullptr);
			}
		}

		EventContainers.EmplaceAt(EventID, Container);
	}

	int32 FEventManager::EncodeCollisionIndex(int32 ActualCollisionIndex, bool bSwapOrder)
	{
		return bSwapOrder ? (ActualCollisionIndex | (1 << 31)) : ActualCollisionIndex;
	}

	int32 FEventManager::DecodeCollisionIndex(int32 EncodedCollisionIdx, bool& bSwapOrder)
	{
		bSwapOrder = EncodedCollisionIdx & (1 << 31);
		return EncodedCollisionIdx & ~(1 << 31);
	}
}
