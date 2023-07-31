// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/Framework/MultiBufferResource.h"
#include "Chaos/Framework/PhysicsProxyBase.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "Misc/ScopeLock.h"
#include "Chaos/Defines.h"

namespace Chaos
{

	//#todo : need to enable disable events, or not register them if disabled.
	//#todo : add timers
	//#todo : warning if trying to add same event ID twice
	//#todo : need sparse array for EventID -> EventContainer?

	/**
	 * Predefined System Event Types
	 */
	enum class EEventType : int32
	{
		Collision = 0,
		Breaking = 1,
		Trailing = 2,
		Sleeping = 3,
		Removal = 4,
		Crumbling = 5,
	};

	typedef int32 FEventID;


	/**
	 * Interface for event handler 
	 */
	class IEventHandler
	{
		template<typename PayloadType>
		friend class TEventContainer;

	public:
		virtual ~IEventHandler() {}
		virtual void HandleEvent(const void* EventData) const = 0;

	protected:
		// internal use only
		virtual void* GetHandler() const = 0;
	};

	/** Instance event handler */
	template<typename PayloadType, typename HandlerType>
	class TRawEventHandler : public IEventHandler
	{
	public:
		typedef void (HandlerType::*FHandlerFunction)(const PayloadType&);

		TRawEventHandler(HandlerType* InHandler, FHandlerFunction InFunction)
			: Handler(InHandler)
			, HandlerFunction(InFunction)
		{
			check(Handler);
			check(HandlerFunction);
		}

		virtual void HandleEvent(const void* EventData) const override
		{
			(Handler->*HandlerFunction)(*(const PayloadType*)EventData);
		}

	protected:
		void* GetHandler() const override
		{
			return Handler;
		}

	private:
		HandlerType* Handler;
		FHandlerFunction HandlerFunction;
	};

	/**
	 * Pointer to the event handler
	 */
	typedef IEventHandler* FEventHandlerPtr;

	/**
	 * Interface for the injected producer function and associated data buffer
	 */
	class FEventContainerBase
	{
	public:
		virtual ~FEventContainerBase() {}
		/**
		 * Register the delegate function that will handle the events on the game thread
		 */
		virtual void RegisterHandler(const FEventHandlerPtr& Handler) = 0;

		/**
		 * Unregister the delegate function that handles the events on the game thread
		 */
		virtual void UnregisterHandler(const void* Handler) = 0;

		/*
		 * Inject data from the physics solver into the producer side of the buffer
		 */
		virtual void InjectProducerData(const FPBDRigidsSolver* Solver, bool bResetData) = 0;

		/**
		 * Flips the buffer if the buffer type is double or triple
		 */
		virtual void FlipBufferIfRequired() = 0;

		/**
		 * Dispatch events to the registered handlers
		 */
		virtual void DispatchConsumerData() = 0;
	};

	/**
	 * Class that owns the injected producer function and its associated data buffer
	 */
	template<typename PayloadType>
	class TEventContainer : public FEventContainerBase
	{
	public:
		/**
		 * Regular constructor
		 */
		TEventContainer(const Chaos::EMultiBufferMode& BufferMode, TFunction<void(const FPBDRigidsSolver* Solver, PayloadType& EventDataInOut, bool bResetData)> InFunction)
			: InjectedFunction(InFunction)
			, EventBuffer(Chaos::FMultiBufferFactory<PayloadType>::CreateBuffer(BufferMode))
		{
		}

		/**
		 * Copy constructor
		 */
		TEventContainer(TEventContainer<PayloadType>& Other)
		{
			InjectedFunction = Other.InjectedFunction;
			EventBuffer = MoveTemp(Other.EventBuffer);
		}

		/**
		 * Destructor cleans up memory
		 */
		~TEventContainer()
		{
			for (FEventHandlerPtr Handler : HandlerArray)
			{
				delete Handler;
				Handler = nullptr;
			}
		}
#
		/**
		 * Register the delegate function that will handle the events on the game thread
		 */
		virtual void RegisterHandler(const FEventHandlerPtr& Handler)
		{
			HandlerArray.AddUnique(Handler);
		}

		/**
		 * Unregister the delegate function that handles the events on the game thread
		 */
		virtual void UnregisterHandler(const void* InHandler)
		{
			for (int i = 0; i < HandlerArray.Num(); i++)
			{
				if (HandlerArray[i]->GetHandler() == InHandler)
				{
					DeleteHandler(HandlerArray[i]);
					HandlerArray.RemoveAt(i);
					break;
				}
			}
		}

		/*
		 * Inject data from the physics solver into the producer side of the buffer
		 */
		virtual void InjectProducerData(const FPBDRigidsSolver* Solver, bool bResetData)
		{
			InjectedFunction(Solver, *EventBuffer->AccessProducerBuffer(), bResetData);
		}

		
		virtual void DestroyStaleEvents(TFunction<void(PayloadType & EventDataInOut)> InFunction)
		{
			InFunction(*EventBuffer->AccessProducerBuffer());
		}

		virtual void AddEvent(TFunction<void(PayloadType& EventDataInOut)> InFunction)
		{
			InFunction(*EventBuffer->AccessProducerBuffer());
		}

		/**
		 * Flips the buffer if the buffer type is double or triple
		 */
		virtual void FlipBufferIfRequired()
		{
			EventBuffer->FlipProducer();
		}

		/**
		 * Dispatch events to the registered handlers
		 */
		virtual void DispatchConsumerData()
		{
			for (FEventHandlerPtr Handler : HandlerArray)
			{
				Handler->HandleEvent(EventBuffer.Get()->GetConsumerBuffer());
			}
		}

	private:

		void DeleteHandler(FEventHandlerPtr& HandlerPtr)
		{
			delete HandlerPtr;
			HandlerPtr = nullptr;
		}

		/**
		 * The function that handles filling the event data buffer
		 */
		TFunction<void(const FPBDRigidsSolver* Solver, PayloadType& EventData, bool bResetData)> InjectedFunction;

		/**
		 * The data buffer that is filled by the producer and read by the consumer
		 */
		TUniquePtr<IBufferResource<PayloadType>> EventBuffer;

		/**
		 * Delegate function registered to handle this event when it is dispatched
		 */
		TArray<FEventHandlerPtr> HandlerArray;
	};

	/**
	 * Pointer to event data buffer & injector functionality
	 */
	using FEventContainerBasePtr = FEventContainerBase*;

	class CHAOS_API FEventManager
	{
		friend class FPBDRigidsSolver;

	public:

		FEventManager(const Chaos::EMultiBufferMode& BufferModeIn) 
			: BufferMode(BufferModeIn)
			, bCurrentlyDispatchingEvents(false)
			{}

		~FEventManager()
		{
			Reset();
		}

		/**
		 * Clears out every handler and container calling destructors on held items
		 */
		void Reset();

		/**
		 * Set the buffer mode to be used within the event containers
		 */
		void SetBufferMode(const Chaos::EMultiBufferMode& BufferModeIn)
		{
			BufferMode = BufferModeIn;
		}

		/**
		 * Register a new event into the system, providing the function that will fill the producer side of the event buffer
		 */
		template<typename PayloadType>
		void RegisterEvent(const EEventType& EventType, TFunction<void(const Chaos::FPBDRigidsSolver* Solver, PayloadType& EventData, bool bResetData)> InFunction)
		{
			ContainerLock.WriteLock();
			InternalRegisterInjector(FEventID(EventType), new TEventContainer<PayloadType>(BufferMode, InFunction));
			ContainerLock.WriteUnlock();
		}

		/**
		 * Modify the producer side of the event buffer
		 */
		template<typename PayloadType>
		void ClearEvents(const EEventType& EventType, TFunction<void(PayloadType & EventData)> InFunction)
		{
			ContainerLock.ReadLock();

			if (TEventContainer<PayloadType>* EventContainer = StaticCast<TEventContainer<PayloadType>*>(EventContainers[FEventID(EventType)]))
			{
				EventContainer->DestroyStaleEvents(InFunction);
			}
			ContainerLock.ReadUnlock();
		}

		/**
		 * Unregister specified event from system
		 */
		void UnregisterEvent(const EEventType& EventType);

		/**
		 * Register a handler that will receive the dispatched events
		 */
		template<typename PayloadType, typename HandlerType>
		void RegisterHandler(const EEventType& EventType, HandlerType* Handler, typename TRawEventHandler<PayloadType, HandlerType>::FHandlerFunction HandlerFunction)
		{
			FScopeLock ScopeLock(&AccessDeferredHandlersLock);

			const FEventID EventID = FEventID(EventType);
			
			// If we are currently dispatching events, defer handler registration until completion of dispatch to avoid deadlock
			if (bCurrentlyDispatchingEvents)
			{
				DeferredHandlers.Add(TPair<FEventID, IEventHandler*>(EventID, new TRawEventHandler<PayloadType, HandlerType>(Handler, HandlerFunction)));
			}
			else
			{
				ContainerLock.WriteLock();
				checkf(EventID < EventContainers.Num(), TEXT("Registering event Handler for an event ID that does not exist"));
				EventContainers[EventID]->RegisterHandler(new TRawEventHandler<PayloadType, HandlerType>(Handler, HandlerFunction));
				ContainerLock.WriteUnlock();
			}	
		}

		/**
		 * Unregister the specified event handler
		 */
		void UnregisterHandler(const EEventType& EventType, const void* InHandler);

		/**
		 * Called by the solver to invoke the functions that fill the producer side of all the event data buffers
		 */
		void FillProducerData(const Chaos::FPBDRigidsSolver* Solver, bool bResetData = true);

		/**
		 * Flips the event data buffer if it is of double or triple buffer type
		 */
		void FlipBuffersIfRequired();

		/**
		 * // Dispatch events to the registered handlers
		 */
		void DispatchEvents();

		/** Returns encoded collision index. */
		static int32 EncodeCollisionIndex(int32 ActualCollisionIndex, bool bSwapOrder);
		/** Returns decoded collision index. */
		static int32 DecodeCollisionIndex(int32 EncodedCollisionIdx, bool& bSwapOrder);


		template<typename PayloadType>
		void AddEvent(const EEventType& EventType, TFunction<void(PayloadType& EventData)> InFunction)
		{
			ContainerLock.ReadLock();
			if (TEventContainer<PayloadType>* EventContainer = StaticCast<TEventContainer<PayloadType>*>(EventContainers[FEventID(EventType)]))
			{
				EventContainer->AddEvent(InFunction);
			}
			ContainerLock.ReadUnlock();
		}

	private:

		void InternalRegisterInjector(const FEventID& EventID, const FEventContainerBasePtr& Container);

		Chaos::EMultiBufferMode BufferMode;			// specifies the buffer type to be constructed, single, double, triple
		TArray<FEventContainerBasePtr> EventContainers;	// Array of event types
		FRWLock ResourceLock;
		FRWLock ContainerLock;
		FCriticalSection AccessDeferredHandlersLock;

		/** Defer handler registration if we are currently dispatching events. */
		bool bCurrentlyDispatchingEvents;
		TArray<TPair<FEventID, IEventHandler*>> DeferredHandlers;
	};

}