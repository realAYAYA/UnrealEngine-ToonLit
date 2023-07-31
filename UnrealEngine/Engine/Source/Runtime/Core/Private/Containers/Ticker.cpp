// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/Ticker.h"
#include "Stats/Stats.h"
#include "Misc/TimeGuard.h"

FTSTicker& FTSTicker::GetCoreTicker()
{
	static FTSTicker CoreTicker;
	return CoreTicker;
}

FTSTicker::FDelegateHandle FTSTicker::AddTicker(const FTickerDelegate& InDelegate, float InDelay)
{
	FElementPtr NewElement{ new FElement{ CurrentTime + InDelay, InDelay, InDelegate } };
	AddedElements.Enqueue(NewElement);
	return NewElement;
}

FTSTicker::FDelegateHandle FTSTicker::AddTicker(const TCHAR* InName, float InDelay, TFunction<bool(float)> Function)
{
	FElementPtr NewElement{ new FElement{ CurrentTime + InDelay, InDelay, FTickerDelegate::CreateLambda(Function) } };
	AddedElements.Enqueue(NewElement);
	return NewElement;
}

uint32 GetThreadId(uint64 State)
{
	return (uint32)(State >> 32);
}

void FTSTicker::RemoveTicker(FDelegateHandle Handle)
{
	if (FElementPtr Element = Handle.Pin())
	{
		// mark the element as removed and if it's being ticked atm, spin-wait until its execution is finished
		uint64 PrevState = Element->State.fetch_or(FElement::RemovedState, std::memory_order_acquire); // "acquire" to prevent potential 
		// resource release after RemoveTicker() to be reordered before it
		uint32 ExecutingThreadId = GetThreadId(PrevState);
		
		while (ExecutingThreadId != 0 && // is being executed right now
			FPlatformTLS::GetCurrentThreadId() != ExecutingThreadId) // and is not removed from inside its execution
		{
			FPlatformProcess::Yield();
			ExecutingThreadId = GetThreadId(Element->State.load(std::memory_order_relaxed));
		}
	}
}

void FTSTicker::Tick(float DeltaTime)
{
	SCOPE_TIME_GUARD(TEXT("FTSTicker::Tick"));

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FTicker_Tick);

	const uint64 CurrentThreadId = ((uint64)FPlatformTLS::GetCurrentThreadId()) << 32; // already prepared to be used in `FElement::State`

	// take in all new elements
	auto PumpAddedElementsQueue = [this]() {
		while (TOptional<FElementPtr> AddedElement = AddedElements.Dequeue())
		{
			uint64 State = AddedElement.GetValue()->State.load(std::memory_order_relaxed);
			checkf(GetThreadId(State) == 0, TEXT("Invalid state %u"), State);
			if (State == FElement::DefaultState)
			{
				Elements.Add(MoveTemp(AddedElement.GetValue()));
			}
		}
	};

	PumpAddedElementsQueue();

	if (!Elements.Num())
	{
		return;
	}

	CurrentTime += DeltaTime;

	TArray<FElementPtr> TickedElements;
	int32 ElementIdx = 0;
	// ticking delegates can add more tickers that must be executed in the same tick call to be backward compatible with the old
	// implementation. keep transfering new tickers to the main list and executing them
	do
	{
		for (; ElementIdx < Elements.Num(); ++ElementIdx)
		{
			FElementPtr& Element = Elements[ElementIdx];
			// set the execution state
			uint64 PrevState = Element->State.fetch_add(CurrentThreadId, std::memory_order_acquire); // "acquire" to prevent anything between this and `ClearExecutionFlag()` to be reordered outside of this scope, is coupled with "release" in `ClearExecutionFlag`
			checkf(GetThreadId(PrevState) == 0, TEXT("Invalid state %u"), PrevState);
			auto ClearExecutionFlag = [CurrentThreadId](FElementPtr& Element)
			{
				return (uint8)(Element->State.fetch_sub(CurrentThreadId, std::memory_order_release) - CurrentThreadId);
			};

			if (PrevState == FElement::RemovedState)
			{
				ClearExecutionFlag(Element);
				continue;
			}

			if (Element->FireTime > CurrentTime)
			{
				ClearExecutionFlag(Element);
				TickedElements.Add(MoveTemp(Element));
				continue;
			}

			if (Element->Fire(DeltaTime))
			{
				PrevState = ClearExecutionFlag(Element); // it can be removed during execution
				if (PrevState != FElement::RemovedState)
				{
					checkf(PrevState == FElement::DefaultState, TEXT("Invalid state %u"), PrevState);
					Element->FireTime = CurrentTime + Element->DelayTime;
					TickedElements.Add(MoveTemp(Element));
				}
			}
			else
			{
				PrevState = ClearExecutionFlag(Element);
				checkf(PrevState == FElement::DefaultState || PrevState == FElement::RemovedState, TEXT("Invalid state %u"), PrevState);
			}
		}

		// See if there were new elements added while ticking. If so, tick them this frame as well
		PumpAddedElementsQueue();
	} while (ElementIdx < Elements.Num());

	Elements.Reset();
	Exchange(TickedElements, Elements);

	if (this == &GetCoreTicker())
	{
		// tick also deprecated FTicker for backward compatibility
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FTicker::GetCoreTicker().Tick(DeltaTime);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

void FTSTicker::Reset()
{
	while (AddedElements.Dequeue())
	{
	}
	CurrentTime = 0.0;
	Elements.Reset();
}

FTSTicker::FElement::FElement()
	: FireTime(0.0)
	, DelayTime(0.0f)
{}

FTSTicker::FElement::FElement(double InFireTime, float InDelayTime, const FTickerDelegate& InDelegate)
	: FireTime(InFireTime)
	, DelayTime(InDelayTime)
	, Delegate(InDelegate)
{}

bool FTSTicker::FElement::Fire(float DeltaTime)
{
	return Delegate.IsBound() && Delegate.Execute(DeltaTime);
}

FTSTickerObjectBase::FTSTickerObjectBase(float InDelay, FTSTicker& InTicker)
{
	// Register delegate for ticker callback
	FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FTSTickerObjectBase::Tick);
	TickHandle = InTicker.AddTicker(TickDelegate, InDelay);
}

FTSTickerObjectBase::~FTSTickerObjectBase()
{
	FTSTicker::RemoveTicker(TickHandle);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deprecated non thread-safe version

PRAGMA_DISABLE_DEPRECATION_WARNINGS

FTicker::FTicker()
	: CurrentTime(0.0)
	, bInTick(false)
	, bCurrentElementRemoved(false)
{
}

FTicker::~FTicker()
{
}

FDelegateHandle FTicker::AddTicker(const FTickerDelegate& InDelegate, float InDelay)
{
	// We can add elements safely even during tick.
	Elements.Emplace(CurrentTime + InDelay, InDelay, InDelegate);
	// @todo this needs a unique handle for each add call to allow you to register the same delegate twice with a different delay safely.
	// because of this, RemoveTicker removes all elements that use this handle.
	return InDelegate.GetHandle();
}


FDelegateHandle FTicker::AddTicker(const TCHAR* InName, float InDelay, TFunction<bool(float)> Function)
{
	checkf(IsInGameThread(), TEXT("FTicker is not thread-safe and will be deprecated, please use FTSTicker"));

	// todo - use InName for profiling. Added in sig to be forward looking..

	FTickerDelegate Delegate = FTickerDelegate::CreateLambda(Function);

	// We can add elements safely even during tick.
	Elements.Emplace(CurrentTime + InDelay, InDelay, Delegate);

	return Delegate.GetHandle();
}

void FTicker::RemoveTicker(FDelegateHandle Handle)
{
	checkf(IsInGameThread(), TEXT("FTicker is not thread-safe and will be deprecated, please use FTSTicker"));

	// must remove the handle from both arrays because we could be in the middle of a tick, 
	// and may be removing ourselves or something else we've already considered this Tick
	auto CompareHandle = [=](const FElement& Element){ return Element.Delegate.GetHandle() == Handle; };
	// We can remove elements safely even during tick.
	Elements.RemoveAllSwap(CompareHandle);
	TickedElements.RemoveAllSwap(CompareHandle);
	// if we are ticking, we must check for the edge case of the CurrentElement removing itself.
	if (bInTick && CompareHandle(CurrentElement))
	{
		// Technically it's possible for someone to try to remove CurrentDelegate multiple times, so make sure we never set this value to false in here.
		bCurrentElementRemoved = true;
	}
}

void FTicker::Tick(float DeltaTime)
{
	SCOPE_TIME_GUARD(TEXT("FTicker::Tick"));

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FTicker_Tick);
	if (!Elements.Num())
	{
		return;
	}

	// make sure we scope the "InTick" state
	TGuardValue<bool> TickGuard(bInTick, true);

	CurrentTime += DeltaTime;
	// we will keep popping off elements until the array is empty.
	// We cannot keep references or iterators into any of these containers because the act of firing a delegate could
	// add or remove any other delegate, including itself. 
	// As we pop off elements, we track the CurrentElement in case it tries to remove itself.
	// When finished, we put it onto a TickedElements list. This allows another delegate in the list to remove it,
	// while still keeping track of elements we've ticked by popping off the Elements array.
	// See RemoveTicker for details on how this state is used.
	while (Elements.Num())
	{
		// as an optimization, if the Element is not ready to fire, move it immediately to the TickedElements list.
		// This is safe because no one can mutate the arrays while we do this.
		if (Elements.Last().FireTime > CurrentTime)
		{
			TickedElements.Add(Elements.Pop(false));
		}
		else
		{
			// we now know the element is ready to fire, so pop it off the list.
			CurrentElement = Elements.Pop(false);
			// reset this state every time we reassign to CurrentElement.
			bCurrentElementRemoved = false;
			// fire the delegate. It can return false to tell us to remove it immediately.
			bool bRemoveElement = !CurrentElement.Fire(DeltaTime);
			// The act of firing the delegate could also have caused it to remove itself.
			// if either happens, we just won't add this to the TickedElements array.
			if (!bRemoveElement && !bCurrentElementRemoved)
			{
				// update the fire time.
				// Note this is where Timer skew occurs. Use FireTime += DelayTime if skew is not wanted.
				CurrentElement.FireTime = CurrentTime + CurrentElement.DelayTime;
				// push this element onto the TickedElement list so we know we already ticked it.
				TickedElements.Push(CurrentElement);
			}
		}
	}

	// Now that we've considered all the delegates, we swap it back into the Elements array.
	Exchange(TickedElements, Elements);
	// Also clear the CurrentElement delegate as our tick is done
	CurrentElement.Delegate.Unbind();
}

FTicker::FElement::FElement() 
	: FireTime(0.0)
	, DelayTime(0.0f)
{}

FTicker::FElement::FElement(double InFireTime, float InDelayTime, const FTickerDelegate& InDelegate) 
	: FireTime(InFireTime)
	, DelayTime(InDelayTime)
	, Delegate(InDelegate)
{}

bool FTicker::FElement::Fire(float DeltaTime)
{
	if (Delegate.IsBound())
	{
		return Delegate.Execute(DeltaTime);
	}
	return false;
}

#if PLATFORM_WINDOWS && PLATFORM_32BITS
// Workaround for ICE on VC++ 2017 14.13.26128 for UnrealGame Win32
PRAGMA_DISABLE_OPTIMIZATION
#endif

FTickerObjectBase::FTickerObjectBase(float InDelay, FTicker& InTicker)
	: Ticker(InTicker)
{
	// Register delegate for ticker callback
	FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FTickerObjectBase::Tick);
	TickHandle = Ticker.AddTicker(TickDelegate, InDelay);
}

#if PLATFORM_WINDOWS && PLATFORM_32BITS
PRAGMA_ENABLE_OPTIMIZATION
#endif

FTickerObjectBase::~FTickerObjectBase()
{
	// Unregister ticker delegate
	if (TickHandle != FDelegateHandle())
	{
		Ticker.RemoveTicker(TickHandle);
		TickHandle = FDelegateHandle();
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
