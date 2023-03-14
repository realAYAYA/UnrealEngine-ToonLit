// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/QuartzQuantizationUtilities.h"
#include "Containers/MpscQueue.h"

// forwards
class UQuartzSubsystem;
class UQuartzClockHandle;
class FQuartzTickableObject;
struct FQuartzTickableObjectsManager;

namespace Audio
{
	struct FQuartzGameThreadSubscriber;
	class FQuartzClockProxy;

	// Struct used to communicate command state back to the game play thread
	struct ENGINE_API FQuartzQuantizedCommandDelegateData : public FQuartzCrossThreadMessage
	{
		EQuartzCommandType CommandType;
		EQuartzCommandDelegateSubType DelegateSubType;

		// ID so the clock handle knows which delegate to fire
		int32 DelegateID{ -1 };

	}; // struct FQuartzQuantizedCommandDelegateData

	// Struct used to communicate metronome events back to the game play thread
	struct ENGINE_API FQuartzMetronomeDelegateData : public FQuartzCrossThreadMessage
	{
		int32 Bar;
		int32 Beat;
		float BeatFraction;
		EQuartzCommandQuantization Quantization;
		FName ClockName;
	}; // struct FQuartzMetronomeDelegateData

	//Struct used to queue events to be sent to the Audio Render thread closer to their start time
	struct ENGINE_API FQuartzQueueCommandData : public FQuartzCrossThreadMessage
	{
		FAudioComponentCommandInfo AudioComponentCommandInfo;
		FName ClockName;

		FQuartzQueueCommandData(const FAudioComponentCommandInfo& InAudioComponentCommandInfo, FName InClockName);
	}; // struct FQuartzQueueCommandData


	// old non-template\ version of TQuartzShareableCommandQueue
	class UE_DEPRECATED(5.1, "Message") FShareableQuartzCommandQueue;
	class ENGINE_API FShareableQuartzCommandQueue
	{
	};

	/**
	*	Template class for mono-directional MPSC command queues 
	* 
	*   in order to enforce thread-safe access to the object executing the commands,
	*	"listener type" is the type of the object that is being accessed in the commands
	*	that object will have to provide a 'this' ptr (of type ListenerType) in order to 
	*   invoke the commands on itself. (The lambdas do NOT and should NOT cache a ptr or
	*	reference to the target).
	* 
	*	User-provided lambdas can take any (single) argument type T in PushEvent()
	*	but there must exist a ListenerType::ExecCommand(T) overload for any PushEvent(T) instantiated. 
	* 
	*	(see FQuartzTickableObject and FQuartzClock as examples)
	* 
	**/
	template <class ListenerType>
	class TQuartzShareableCommandQueue
	{
	public:
		// ctor
		TQuartzShareableCommandQueue() {}

		// dtor
		~TQuartzShareableCommandQueue() {}

		// static helper to create a new sharable queue
		static TSharedPtr<TQuartzShareableCommandQueue<ListenerType>, ESPMode::ThreadSafe> Create()
		{
			return MakeShared<TQuartzShareableCommandQueue<ListenerType>, ESPMode::ThreadSafe>();
		}

		// note: ListenerType must have a ExecCommand() overload for each instantiation of this method
		template <typename T>
		void PushEvent(const T& Data)
		{
			CommandQueue.Enqueue([InData = Data](ListenerType* Listener) { Listener->ExecCommand(InData); });
		}

		void PushCommand(TFunction<void(ListenerType*)> InCommand)
		{
			CommandQueue.Enqueue([=](ListenerType* Listener) { InCommand(Listener); });
		}

		void PumpCommandQueue(ListenerType* InListener)
		{
			// gather move all the current commands into our temp container
			// (in case the commands themselves alter the container)
			TOptional<TFunction<void(ListenerType*)>> Function = CommandQueue.Dequeue();
			while (Function.IsSet())
			{
				TempCommandQueue.Emplace(Function.GetValue());
				Function = CommandQueue.Dequeue();
			}

			for (auto& Command : TempCommandQueue)
			{
				Command(InListener);
			}

			TempCommandQueue.Reset();
		}

	private:
		TMpscQueue<TFunction<void(ListenerType*)>> CommandQueue;
		TArray<TFunction<void(ListenerType*)>> TempCommandQueue;
	};

} // namespace Audio

using FQuartzGameThreadCommandQueue = Audio::TQuartzShareableCommandQueue<FQuartzTickableObject>;
using FQuartzGameThreadCommandQueuePtr = TSharedPtr<FQuartzGameThreadCommandQueue, ESPMode::ThreadSafe>;

/**
 *	FQuartzTickableObject
 *
 *		This is the base class for non-Audio Render Thread objects that want to receive
 *		callbacks for Quartz events.
 *
 *		It is a wrapper around TQuartzShareableCommandQueue.
 *		(see UQuartzClockHandle or UAudioComponent as implementation examples)
 */
class ENGINE_API FQuartzTickableObject
{
public:
	// ctor
	FQuartzTickableObject() {}

	// explicitly defaulted ctors (to disable deprecation warnings in compiler-generated functions)
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    FQuartzTickableObject(const FQuartzTickableObject& Other) = default;
    FQuartzTickableObject& operator=(const FQuartzTickableObject&) = default;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// dtor
	virtual ~FQuartzTickableObject();

	FQuartzTickableObject* Init(UWorld* InWorldPtr);

	// called by the associated QuartzSubsystem
	void QuartzTick(float DeltaTime);

	bool QuartzIsTickable() const;

	UE_DEPRECATED(5.1, "Derived classes should have access to their own UWorld, this function will always return null")
	UWorld* QuartzGetWorld() const { return nullptr; }

	void AddMetronomeBpDelegate(EQuartzCommandQuantization InQuantizationBoundary, const FOnQuartzMetronomeEventBP& OnQuantizationEvent);


	bool IsInitialized() const { return TickableObjectManagerPtr.IsValid(); }

	Audio::FQuartzGameThreadSubscriber GetQuartzSubscriber();

	int32 AddCommandDelegate(const FOnQuartzCommandEventBP& InDelegate);

	UE_DEPRECATED(5.1, "use FQuartzTickableObject::AddCommandDelegate(const FOnQuartzCommandEventBP& InDelegate) insead")
	int32 AddCommandDelegate(const FOnQuartzCommandEventBP& InDelegate, TArray<FQuartzGameThreadCommandQueuePtr>& TargetSubscriberArray){ return -1; }

	UE_DEPRECATED(5.1, "This object no longer holds any UObject references. Caller should have their own UWorld* and use static 'UQuartzSubsystem::Get()' instead.")
	UQuartzSubsystem* GetQuartzSubsystem() const;

	// required by TQuartzShareableCommandQueue template
	void ExecCommand(const Audio::FQuartzQuantizedCommandDelegateData& Data);
	void ExecCommand(const Audio::FQuartzMetronomeDelegateData& Data);
	void ExecCommand(const Audio::FQuartzQueueCommandData& Data);

	// virtual interface (ExecCommand will forward the data to derived classes' ProcessCommand() call)
	virtual void ProcessCommand(const Audio::FQuartzQuantizedCommandDelegateData& Data) {}
	virtual void ProcessCommand(const Audio::FQuartzMetronomeDelegateData& Data) {}
	virtual void ProcessCommand(const Audio::FQuartzQueueCommandData& Data) {}

	const Audio::FQuartzOffset& GetQuartzOffset() const { return NotificationOffset; }

protected:
	void SetNotificationAnticipationAmountMilliseconds(const double Milliseconds);
	void SetNotificationAnticipationAmountMusicalDuration(const EQuartzCommandQuantization Duration,  const double Multiplier);

private:
	struct FMetronomeDelegateGameThreadData
	{
		FOnQuartzMetronomeEvent MulticastDelegate;
	};

	struct FCommandDelegateGameThreadData
	{
		FOnQuartzCommandEvent MulticastDelegate;
		FThreadSafeCounter RefCount;
	};

	// delegate containers
	FMetronomeDelegateGameThreadData MetronomeDelegates[static_cast<int32>(EQuartzCommandQuantization::Count)];
	TArray<FCommandDelegateGameThreadData> QuantizedCommandDelegates;

	TArray<TFunction<void(FQuartzTickableObject*)>> TempCommandQueue;

public:
	// deprecate public access
	UE_DEPRECATED(5.1, "FQuartzTickableObject::CommandQueuePtr should no longer be accessed directly. (This member will be private in a future engine version).  Use GetQuartzSubscriber() instead")
	TSharedPtr<Audio::TQuartzShareableCommandQueue<FQuartzTickableObject>, ESPMode::ThreadSafe> CommandQueuePtr;

private:
	Audio::FQuartzOffset NotificationOffset;
	TWeakPtr<FQuartzTickableObjectsManager> TickableObjectManagerPtr;

}; // class FQuartzTickableObject

