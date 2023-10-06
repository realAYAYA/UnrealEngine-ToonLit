// Copyright Epic Games, Inc. All Rights Reserved.

#include "View/MVVMBindingSubsystem.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/MemStack.h"
#include "SlateGlobals.h"
#include "Stats/Stats2.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBindingSubsystem)


DECLARE_CYCLE_STAT(TEXT("MVVM Bindings"), STAT_MVVMBindingTick, STATGROUP_Slate);

void UMVVMBindingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().AddUObject(this, &UMVVMBindingSubsystem::HandlePreTick);
	}
}

void UMVVMBindingSubsystem::Deinitialize()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().RemoveAll(this);
	}
	Super::Deinitialize();
}

void UMVVMBindingSubsystem::HandlePreTick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_MVVMBindingTick);

	if (EveryTickBindings.Num() > 0)
	{
		FMemMark Mark(FMemStack::Get());
		TArray<const UMVVMView*, TMemStackAllocator<>> ToTick;
		ToTick.Reserve(EveryTickBindings.Num());
		for (TWeakObjectPtr<const UMVVMView> View : EveryTickBindings)
		{
			if (const UMVVMView* ViewPtr = View.Get())
			{
				ToTick.Add(ViewPtr);
			}
		}

		for (const UMVVMView* ViewPtr : ToTick)
		{
			ViewPtr->ExecuteEveryTickBindings();
		}
	}

	if (DelayedBindings.Num() > 0)
	{
		FDelayedMap AllDelayedBindingsExecutedThisFrame;
		AllDelayedBindingsExecutedThisFrame.Reserve(DelayedBindings.Num());

		FDelayedMap DelayedBindingsWhileTicking = MoveTemp(DelayedBindings);
		DelayedBindings = FDelayedMap();

		do 
		{
			for (const auto& DelayedBindingsPair : DelayedBindingsWhileTicking)
			{
				if (const UMVVMView* View = DelayedBindingsPair.Key.Get())
				{
					ensure(DelayedBindingsPair.Value.Num() > 0);
					FDelayedBindingList& AllBindingList = AllDelayedBindingsExecutedThisFrame.FindOrAdd(DelayedBindingsPair.Key);
					for (const FMVVMViewDelayedBinding& DelayedBinding : DelayedBindingsPair.Value)
					{
						View->ExecuteDelayedBinding(DelayedBinding);
						AllBindingList.AddUnique(DelayedBinding);
					}
				}
			}

			DelayedBindingsWhileTicking.Reset();

			// Test new binding added while executing the current binding list
			for (const auto& DelayedBindingPair : DelayedBindings)
			{
				if (DelayedBindingPair.Key.Get())
				{
					if (FDelayedBindingList* AllDelayedBindingsListPtr = AllDelayedBindingsExecutedThisFrame.Find(DelayedBindingPair.Key))
					{
						for (const FMVVMViewDelayedBinding& NewCompiledBinding : DelayedBindingPair.Value)
						{
							if (!AllDelayedBindingsListPtr->Find(NewCompiledBinding))
							{
								// It was not executed. Add it to be executed this frame.
								DelayedBindingsWhileTicking.FindOrAdd(DelayedBindingPair.Key).AddUnique(NewCompiledBinding);
							}
						}
					}
				}
			}
		} while (DelayedBindingsWhileTicking.Num() > 0);
	}
}

void UMVVMBindingSubsystem::AddViewWithEveryTickBinding(const UMVVMView* InView)
{
	check(!EveryTickBindings.Contains(InView));
	ensureMsgf(FSlateApplication::IsInitialized(), TEXT("The Slate Application is not initialized. This is probably because you are running a server. The Delayed and Tick binding will not execute."));

	EveryTickBindings.Add(InView);
}

void UMVVMBindingSubsystem::RemoveViewWithEveryTickBinding(const UMVVMView* InView)
{
	EveryTickBindings.RemoveSingleSwap(InView);
}

void UMVVMBindingSubsystem::AddDelayedBinding(const UMVVMView* View, FMVVMViewDelayedBinding InCompiledBinding)
{
	ensureMsgf(FSlateApplication::IsInitialized(), TEXT("The Slate Application is not initialized. This is probably because you are running a server. The Delayed and Tick binding will not execute."));
	DelayedBindings.FindOrAdd(View).AddUnique(InCompiledBinding);
}
