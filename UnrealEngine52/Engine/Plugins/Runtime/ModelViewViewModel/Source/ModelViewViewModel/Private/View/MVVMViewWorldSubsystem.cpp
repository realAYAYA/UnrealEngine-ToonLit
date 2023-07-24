// Copyright Epic Games, Inc. All Rights Reserved.

#include "View/MVVMViewWorldSubsystem.h"

#include "Misc/MemStack.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMViewWorldSubsystem)

void UMVVMViewWorldSubsystem::Tick(float DeltaTime)
{
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

TStatId UMVVMViewWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMVVMViewWorldSubsystem, STATGROUP_Tickables);
}

void UMVVMViewWorldSubsystem::AddViewWithEveryTickBinding(const UMVVMView* InView)
{
	check(!EveryTickBindings.Contains(InView));
	EveryTickBindings.Add(InView);
}

void UMVVMViewWorldSubsystem::RemoveViewWithEveryTickBinding(const UMVVMView* InView)
{
	EveryTickBindings.RemoveSingleSwap(InView);
}

void UMVVMViewWorldSubsystem::AddDelayedBinding(const UMVVMView* View, FMVVMViewDelayedBinding InCompiledBinding)
{
	DelayedBindings.FindOrAdd(View).AddUnique(InCompiledBinding);
}
