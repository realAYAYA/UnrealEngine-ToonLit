// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TimerManager.h: Global gameplay timer facility
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/Object.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/EngineTypes.h"
#endif
#include "Engine/TimerHandle.h"
#include "Templates/Function.h"

class UGameInstance;
enum class ELevelCollectionType : uint8;
struct FTimerSourceList;

// using "not checked" user policy (means race detection is disabled) because this delegate is stored in a TSparseArray and causes its reallocation
// from inside delegate's execution. This is incompatible with race detection that needs to access the delegate instance after its execution
using FTimerDelegate = TDelegate<void(), FNotThreadSafeNotCheckedDelegateUserPolicy>;

#ifndef UE_ENABLE_TRACKING_TIMER_SOURCES
#define UE_ENABLE_TRACKING_TIMER_SOURCES !UE_BUILD_SHIPPING
#endif

/** Simple interface to wrap a timer delegate that can be either native or dynamic. */
struct FTimerUnifiedDelegate
{
	/** Holds the delegate to call. */
	FTimerDelegate FuncDelegate;
	/** Holds the dynamic delegate to call. */
	FTimerDynamicDelegate FuncDynDelegate;
	/** Holds the TFunction callback to call. */
	TFunction<void(void)> FuncCallback;

	FTimerUnifiedDelegate() {};
	FTimerUnifiedDelegate(FTimerDelegate const& D) : FuncDelegate(D) {};
	FTimerUnifiedDelegate(FTimerDynamicDelegate const& D) : FuncDynDelegate(D) {};
	FTimerUnifiedDelegate(TFunction<void(void)>&& Callback) : FuncCallback(MoveTemp(Callback)) {}
	
	inline void Execute()
	{
		if (FuncDelegate.IsBound())
		{
			FScopeCycleCounterUObject Context(FuncDelegate.GetUObject());
			FuncDelegate.Execute();
		}
		else if (FuncDynDelegate.IsBound())
		{
			// stat scope is handled by UObject::ProcessEvent for the UFunction.
			FuncDynDelegate.ProcessDelegate<UObject>(nullptr);
		}
		else if ( FuncCallback )
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FTimerUnifiedDelegate_Execute);
			FuncCallback();
		}
	}

	inline bool IsBound() const
	{
		return ( FuncDelegate.IsBound() || FuncDynDelegate.IsBound() || FuncCallback );
	}

	inline const void* GetBoundObject() const
	{
		if (FuncDelegate.IsBound())
		{
			return FuncDelegate.GetObjectForTimerManager();
		}
		else if (FuncDynDelegate.IsBound())
		{
			return FuncDynDelegate.GetUObject();
		}

		return nullptr;
	}

	inline void Unbind()
	{
		FuncDelegate.Unbind();
		FuncDynDelegate.Unbind();
		FuncCallback = nullptr;
	}

	/** Utility to output info about delegate as a string. */
	FString ToString() const;

	// Movable only
	FTimerUnifiedDelegate(FTimerUnifiedDelegate&&) = default;
	FTimerUnifiedDelegate(const FTimerUnifiedDelegate&) = delete;
	FTimerUnifiedDelegate& operator=(FTimerUnifiedDelegate&&) = default;
	FTimerUnifiedDelegate& operator=(const FTimerUnifiedDelegate&) = delete;
};

enum class ETimerStatus : uint8
{
	Pending,
	Active,
	Paused,
	Executing,
	ActivePendingRemoval
};

struct FTimerData
{
	/** If true, this timer will loop indefinitely.  Otherwise, it will be destroyed when it expires. */
	uint8 bLoop : 1;

	/** If true for a looping timer, it will run a maximum of once per frame when it expires. Otherwise, it will run as many times as can fit in
	 *  the current frame's delta time (rounding down).
	 */
	uint8 bMaxOncePerFrame : 1;

	/** If true, this timer was created with a delegate to call (which means if the delegate becomes invalid, we should invalidate the timer too). */
	uint8 bRequiresDelegate : 1;

	/** Timer Status */
	ETimerStatus Status;
	
	/** Time between set and fire, or repeat frequency if looping. */
	float Rate;

	/** 
	 * Time (on the FTimerManager's clock) that this timer should expire and fire its delegate. 
	 * Note when a timer is paused, we re-base ExpireTime to be relative to 0 instead of the running clock, 
	 * meaning ExpireTime contains the remaining time until fire.
	 */
	double ExpireTime;

	/** Holds the delegate to call. */
	FTimerUnifiedDelegate TimerDelegate;

	/** Handle representing this timer */
	FTimerHandle Handle;

	/** This is the key to the TimerIndicesByObject map - this is kept so that we can look up even if the referenced object is expired */
	const void* TimerIndicesByObjectKey = nullptr;

	/** The level collection that was active when this timer was created. Used to set the correct context before executing the timer's delegate. */
	ELevelCollectionType LevelCollection;

	ENGINE_API FTimerData();

	// Movable only
	FTimerData(FTimerData&&) = default;
	FTimerData(const FTimerData&) = delete;
	FTimerData& operator=(FTimerData&&) = default;
	FTimerData& operator=(const FTimerData&) = delete;
};

/**
 * Structure to support specialized looping behavior.
 */
struct FTimerManagerTimerParameters
{
	bool bLoop = false;
	bool bMaxOncePerFrame = false;
	float FirstDelay = -1.f;
};

/** 
 * Class to globally manage timers.
 */
class FTimerManager : public FNoncopyable
{
public:

	ENGINE_API void Tick(float DeltaTime);
	ENGINE_API TStatId GetStatId() const;

	// ----------------------------------
	// Timer API

	ENGINE_API explicit FTimerManager(UGameInstance* GameInstance = nullptr);
	ENGINE_API virtual ~FTimerManager();

	/**
	 * Called from crash handler to provide more debug information.
	 */
	ENGINE_API virtual void OnCrash();

	/**
	 * Sets a timer to call the given native function at a set interval.  If a timer is already set
	 * for this handle, it will replace the current timer.
	 *
	 * @param InOutHandle			If the passed-in handle refers to an existing timer, it will be cleared before the new timer is added. A new handle to the new timer is returned in either case.
	 * @param InObj					Object to call the timer function on.
	 * @param InTimerMethod			Method to call when timer fires.
	 * @param InRate				The amount of time (in seconds) between set and firing.  If <= 0.f, clears existing timers.
	 * @param InbLoop				true to keep firing at Rate intervals, false to fire only once.
	 * @param InFirstDelay			The time (in seconds) for the first iteration of a looping timer. If < 0.f InRate will be used.
	 */
	template< class UserClass >
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TMethodPtr< UserClass > InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(InObj, InTimerMethod) ), InRate, InbLoop, InFirstDelay);
	}
	template< class UserClass >
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TConstMethodPtr< UserClass > InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate( FTimerDelegate::CreateUObject(InObj, InTimerMethod) ), InRate, InbLoop, InFirstDelay);
	}

	/** Version that takes any generic delegate. */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, FTimerDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(InDelegate), InRate, InbLoop, InFirstDelay);
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, FTimerDynamicDelegate const& InDynDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(InDynDelegate), InRate, InbLoop, InFirstDelay);
	}
	/*** Version that doesn't take a delegate */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, float InRate, bool InbLoop, float InFirstDelay = -1.f)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(), InRate, InbLoop, InFirstDelay);
	}
	/** Version that takes a TFunction */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, TFunction<void(void)>&& Callback, float InRate, bool InbLoop, float InFirstDelay = -1.f )
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(MoveTemp(Callback)), InRate, InbLoop, InFirstDelay);
	}

	/* Preferred versions of the above methods that now take a more configurable structure that specifies timer looping and delay behavior. */

	/**
	 * Sets a timer to call the given native function at a set interval.  If a timer is already set
	 * for this handle, it will replace the current timer.
	 *
	 * @param InOutHandle			If the passed-in handle refers to an existing timer, it will be cleared before the new timer is added. A new handle to the new timer is returned in either case.
	 * @param InObj					Object to call the timer function on.
	 * @param InTimerMethod			Method to call when timer fires.
	 * @param InRate				The amount of time (in seconds) between set and firing.  If <= 0.f, clears existing timers.
	 * @param InTimerParameters		The structure with additional timer looping and delay parameters.
	 */
	template <class UserClass>
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TMethodPtr<UserClass> InTimerMethod, float InRate, const FTimerManagerTimerParameters& InTimerParameters)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(InObj, InTimerMethod)), InRate, InTimerParameters);
	}
	template< class UserClass >
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TConstMethodPtr<UserClass> InTimerMethod, float InRate, const FTimerManagerTimerParameters& InTimerParameters)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(InObj, InTimerMethod)), InRate, InTimerParameters);
	}

	/** Version that takes any generic delegate. */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, FTimerDelegate const& InDelegate, float InRate, const FTimerManagerTimerParameters& InTimerParameters)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(InDelegate), InRate, InTimerParameters);
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, FTimerDynamicDelegate const& InDynDelegate, float InRate, const FTimerManagerTimerParameters& InTimerParameters)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(InDynDelegate), InRate, InTimerParameters);
	}
	/*** Version that doesn't take a delegate */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, float InRate, const FTimerManagerTimerParameters& InTimerParameters)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(), InRate, InTimerParameters);
	}
	/** Version that takes a TFunction */
	FORCEINLINE void SetTimer(FTimerHandle& InOutHandle, TFunction<void(void)>&& Callback, float InRate, const FTimerManagerTimerParameters& InTimerParameters)
	{
		InternalSetTimer(InOutHandle, FTimerUnifiedDelegate(MoveTemp(Callback)), InRate, InTimerParameters);
	}

	/**
	 * Sets a timer to call the given native function on the next tick.
	 *
	 * @param inObj					Object to call the timer function on.
	 * @param inTimerMethod			Method to call when timer fires.
	 */
	template< class UserClass >
	FORCEINLINE FTimerHandle SetTimerForNextTick(UserClass* inObj, typename FTimerDelegate::TMethodPtr< UserClass > inTimerMethod)
	{
		return InternalSetTimerForNextTick(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
	}
	template< class UserClass >
	FORCEINLINE FTimerHandle SetTimerForNextTick(UserClass* inObj, typename FTimerDelegate::TConstMethodPtr< UserClass > inTimerMethod)
	{
		return InternalSetTimerForNextTick(FTimerUnifiedDelegate(FTimerDelegate::CreateUObject(inObj, inTimerMethod)));
	}

	/** Version that takes any generic delegate. */
	FORCEINLINE FTimerHandle SetTimerForNextTick(FTimerDelegate const& InDelegate)
	{
		return InternalSetTimerForNextTick(FTimerUnifiedDelegate(InDelegate));
	}
	/** Version that takes a dynamic delegate (e.g. for UFunctions). */
	FORCEINLINE FTimerHandle SetTimerForNextTick(FTimerDynamicDelegate const& InDynDelegate)
	{
		return InternalSetTimerForNextTick(FTimerUnifiedDelegate(InDynDelegate));
	}
	/** Version that takes a TFunction */
	FORCEINLINE FTimerHandle SetTimerForNextTick(TFunction<void(void)>&& Callback)
	{
		return InternalSetTimerForNextTick(FTimerUnifiedDelegate(MoveTemp(Callback)));
	}

	/**
	* Clears a previously set timer, identical to calling SetTimer() with a <= 0.f rate.
	* Invalidates the timer handle as it should no longer be used.
	*
	* @param InHandle The handle of the timer to clear.
	*/
	FORCEINLINE void ClearTimer(FTimerHandle& InHandle)
	{
		if (const FTimerData* TimerData = FindTimer(InHandle))
		{
			InternalClearTimer(InHandle);
		}
		InHandle.Invalidate();
	}

	/** Clears all timers that are bound to functions on the given object. */
	FORCEINLINE void ClearAllTimersForObject(void const* Object)
	{
		if (Object)
		{
			InternalClearAllTimers( Object );
		}
	}

	/**
	 * Pauses a previously set timer.
	 *
	 * @param InHandle The handle of the timer to pause.
	 */
	ENGINE_API void PauseTimer(FTimerHandle InHandle);

	/**
	 * Unpauses a previously set timer
	 *
	 * @param InHandle The handle of the timer to unpause.
	 */
	ENGINE_API void UnPauseTimer(FTimerHandle InHandle);

	/**
	 * Gets the current rate (time between activations) for the specified timer.
	 *
	 * @param InHandle The handle of the timer to return the rate of.
	 * @return The current rate or -1.f if timer does not exist.
	 */
	FORCEINLINE float GetTimerRate(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerRate(TimerData);
	}

	/**
	 * Returns true if the specified timer exists and is not paused
	 *
	 * @param InHandle The handle of the timer to check for being active.
	 * @return true if the timer exists and is active, false otherwise.
	 */
	FORCEINLINE bool IsTimerActive(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer( InHandle );
		return TimerData && TimerData->Status != ETimerStatus::Paused;
	}

	/**
	* Returns true if the specified timer exists and is paused
	*
	* @param InHandle The handle of the timer to check for being paused.
	* @return true if the timer exists and is paused, false otherwise.
	*/
	FORCEINLINE bool IsTimerPaused(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return TimerData && TimerData->Status == ETimerStatus::Paused;
	}

	/**
	* Returns true if the specified timer exists and is pending
	*
	* @param InHandle The handle of the timer to check for being pending.
	* @return true if the timer exists and is pending, false otherwise.
	*/
	FORCEINLINE bool IsTimerPending(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return TimerData && TimerData->Status == ETimerStatus::Pending;
	}

	/**
	* Returns true if the specified timer exists
	*
	* @param InHandle The handle of the timer to check for existence.
	* @return true if the timer exists, false otherwise.
	*/
	FORCEINLINE bool TimerExists(FTimerHandle InHandle) const
	{
		return FindTimer(InHandle) != nullptr;
	}

	/**
	 * Gets the current elapsed time for the specified timer.
	 *
	 * @param InHandle The handle of the timer to check the elapsed time of.
	 * @return The current time elapsed or -1.f if the timer does not exist.
	 */
	FORCEINLINE float GetTimerElapsed(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerElapsed(TimerData);
	}

	/**
	 * Gets the time remaining before the specified timer is called
	 *
	 * @param InHandle The handle of the timer to check the remaining time of.
	 * @return	 The current time remaining, or -1.f if timer does not exist
	 */
	FORCEINLINE float GetTimerRemaining(FTimerHandle InHandle) const
	{
		FTimerData const* const TimerData = FindTimer(InHandle);
		return InternalGetTimerRemaining(TimerData);
	}

	bool FORCEINLINE HasBeenTickedThisFrame() const
	{
		return (LastTickedFrame == GFrameCounter);
	}

	/**
	 * Finds a handle to a timer bound to a particular dynamic delegate.
	 * This function is intended to be used only by the K2 system.
	 *
	 * @param  InDynamicDelegate  The dynamic delegate to search for.
	 *
	 * @return A handle to the found timer - !IsValid() if no such timer was found.
	 */
	ENGINE_API FTimerHandle K2_FindDynamicTimerHandle(FTimerDynamicDelegate InDynamicDelegate) const;

	/** Debug command to output info on all timers currently set to the log. */
	ENGINE_API void ListTimers() const;

private:
	ENGINE_API void SetGameInstance(UGameInstance* InGameInstance);

// This should be private, but needs to be public for testing.
public:
	/** Generates a handle for a timer at a given index */
	ENGINE_API FTimerHandle GenerateHandle(int32 Index);

// These should be private, but need to be protected so IMPLEMENT_GET_PROTECTED_FUNC works for testing.
protected:
	/** Will find a timer in the active, paused, or pending list. */
	FORCEINLINE FTimerData const* FindTimer(FTimerHandle const& InHandle) const
	{
		return const_cast<FTimerManager*>(this)->FindTimer(InHandle);
	}
	ENGINE_API FTimerData* FindTimer( FTimerHandle const& InHandle );

private:
	ENGINE_API void InternalSetTimer( FTimerHandle& InOutHandle, FTimerUnifiedDelegate&& InDelegate, float InRate, bool bInLoop, float InFirstDelay );
	ENGINE_API void InternalSetTimer( FTimerHandle& InOutHandle, FTimerUnifiedDelegate&& InDelegate, float InRate, const FTimerManagerTimerParameters& InTimerParameters );
	ENGINE_API FTimerHandle InternalSetTimerForNextTick( FTimerUnifiedDelegate&& InDelegate );
	ENGINE_API void InternalClearTimer( FTimerHandle InDelegate );
	ENGINE_API void InternalClearAllTimers( void const* Object );
	ENGINE_API float InternalGetTimerRate( FTimerData const* const TimerData ) const;
	ENGINE_API float InternalGetTimerElapsed( FTimerData const* const TimerData ) const;
	ENGINE_API float InternalGetTimerRemaining( FTimerData const* const TimerData ) const;

	/** Will get a timer in the active, paused, or pending list.  Expected to be given a valid, non-stale handle */
	FORCEINLINE const FTimerData& GetTimer(const FTimerHandle& InHandle) const
	{
		return const_cast<FTimerManager*>(this)->GetTimer(InHandle);
	}
	ENGINE_API FTimerData& GetTimer(FTimerHandle const& InHandle);

	/** Adds a timer from the Timers list, also updating the TimerIndicesByObject map.  Returns the insertion index. */
	ENGINE_API FTimerHandle AddTimer(FTimerData&& TimerData);
	/** Removes a timer from the Timers list at the given index, also cleaning up the TimerIndicesByObject map */
	ENGINE_API void RemoveTimer(FTimerHandle Handle);
	ENGINE_API bool WillRemoveTimerAssert(FTimerHandle Handle) const;

	/** The array of timers - all other arrays will index into this */
	TSparseArray<FTimerData> Timers;
	/** Heap of actively running timers. */
	TArray<FTimerHandle> ActiveTimerHeap;
	/** Set of paused timers. */
	TSet<FTimerHandle> PausedTimerSet;
	/** Set of timers added this frame, to be added after timer has been ticked */
	TSet<FTimerHandle> PendingTimerSet;
	/** A map of object pointers to timers with delegates bound to those objects, for quick lookup */
	TMap<const void*, TSet<FTimerHandle>> ObjectToTimers;

	/** An internally consistent clock, independent of World.  Advances during ticking. */
	double InternalTime;

	/** Index to the timer delegate currently being executed, or INDEX_NONE if none are executing.  Used to handle "timer delegates that manipulate timers" cases. */
	FTimerHandle CurrentlyExecutingTimer;

	/** Set this to GFrameCounter when Timer is ticked. To figure out if Timer has been already ticked or not this frame. */
	uint64 LastTickedFrame;

	/** The last serial number we assigned from this timer manager */
	static ENGINE_API uint64 LastAssignedSerialNumber;

	/** The game instance that created this timer manager. May be null if this timer manager wasn't created by a game instance. */
	UGameInstance* OwningGameInstance;

#if UE_ENABLE_TRACKING_TIMER_SOURCES
	/** Debugging/tracking information used when TimerManager.BuildTimerSourceList is set */
	TUniquePtr<FTimerSourceList> TimerSourceList;
#endif
};

