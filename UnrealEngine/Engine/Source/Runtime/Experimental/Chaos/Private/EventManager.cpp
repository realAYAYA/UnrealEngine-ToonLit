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
		ContainerLock.WriteLock();
		checkf(EventID < EventContainers.Num(), TEXT("Unregistering event Handler for an event ID that does not exist"));
		EventContainers[EventID]->UnregisterHandler(InHandler);
		ContainerLock.WriteUnlock();
	}

	void FEventManager::FillProducerData(const Chaos::FPBDRigidsSolver* Solver, bool bResetData)
	{
		ContainerLock.ReadLock();
		for (FEventContainerBasePtr EventContainer : EventContainers)
		{
			if (EventContainer)
			{
				EventContainer->InjectProducerData(Solver, bResetData);
			}
		}
		ContainerLock.ReadUnlock();
	}

	void FEventManager::FlipBuffersIfRequired()
	{
		if (BufferMode == EMultiBufferMode::Double)
		{
			ResourceLock.WriteLock();
		}

		ContainerLock.ReadLock();
		for (FEventContainerBasePtr EventContainer : EventContainers)
		{
			if (EventContainer)
			{
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
		FScopeLock ScopeLock(&AccessDeferredHandlersLock);

		check(!bCurrentlyDispatchingEvents);
		bCurrentlyDispatchingEvents = true;
		
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
		
		ensure(bCurrentlyDispatchingEvents);
		bCurrentlyDispatchingEvents = false;

		// If we deferred any handler registration in RegisterHandler, complete it now. 
		if (DeferredHandlers.Num() > 0)
		{
			ContainerLock.WriteLock();
			for (TPair<FEventID, IEventHandler*>& DeferredHandler : DeferredHandlers)
			{
				checkf(DeferredHandler.Key < EventContainers.Num(), TEXT("Registering event Handler for an event ID that does not exist"));
				EventContainers[DeferredHandler.Key]->RegisterHandler(DeferredHandler.Value);
			}
			DeferredHandlers.Reset();
			ContainerLock.WriteUnlock();
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
