// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/DebugDrawService.h"
#include "UObject/Package.h"
#include "Engine/Canvas.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DebugDrawService)

FCriticalSection UDebugDrawService::DelegatesLock;
#if ENABLE_MT_DETECTOR
FRWRecursiveAccessDetector UDebugDrawService::DelegatesDetector;
#endif

TArray<TArray<FDebugDrawDelegate> > UDebugDrawService::Delegates;
FEngineShowFlags UDebugDrawService::ObservedFlags(ESFIM_Editor);

UDebugDrawService::UDebugDrawService(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Delegates.Reserve(sizeof(FEngineShowFlags)*8);
}

FDelegateHandle UDebugDrawService::Register(const TCHAR* Name, const FDebugDrawDelegate& NewDelegate)
{
	const int32 Index = FEngineShowFlags::FindIndexByName(Name);

	FDelegateHandle Result;
	if (Index != INDEX_NONE)
	{
		FScopeLock PathLock(&DelegatesLock);
		UE_MT_SCOPED_WRITE_ACCESS(DelegatesDetector);
		if (Index >= Delegates.Num())
		{
			Delegates.AddZeroed(Index - Delegates.Num() + 1);
		}
		Delegates[Index].Add(NewDelegate);
		Result = Delegates[Index].Last().GetHandle();
		ObservedFlags.SetSingleFlag(Index, true);
	}
	return Result;
}

void UDebugDrawService::Unregister(const FDelegateHandle HandleToRemove)
{
	UE_MT_SCOPED_WRITE_ACCESS(DelegatesDetector);
	TArray<FDebugDrawDelegate>* DelegatesArray = Delegates.GetData();
	for (int32 Flag = 0; Flag < Delegates.Num(); ++Flag, ++DelegatesArray)
	{
		check(DelegatesArray); //it shouldn't happen, but to be sure
		const uint32 Index = DelegatesArray->IndexOfByPredicate([=](const FDebugDrawDelegate& Delegate){ return Delegate.GetHandle() == HandleToRemove; });
		if (Index != INDEX_NONE)
		{
			DelegatesArray->RemoveAtSwap(Index, 1, false);
			if (DelegatesArray->Num() == 0)
			{
				ObservedFlags.SetSingleFlag(Flag, false);
			}
		}
	}	
}

void UDebugDrawService::Draw(const FEngineShowFlags Flags, FViewport* Viewport, FSceneView* View, FCanvas* Canvas, UCanvas* CanvasObject)
{
	if (CanvasObject == nullptr)
	{
		CanvasObject = FindObject<UCanvas>(GetTransientPackage(), TEXT("DebugCanvasObject"));
		if (CanvasObject == nullptr)
		{
			CanvasObject = NewObject<UCanvas>(GetTransientPackage(), TEXT("DebugCanvasObject"));
			CanvasObject->AddToRoot();
		}
	}

	// Canvas must be initialized every draw because the FCanvas passed in is on the stack in some scenarios.
	CanvasObject->Init(View->UnscaledViewRect.Width(), View->UnscaledViewRect.Height(), View, Canvas);

	CanvasObject->Update();	
	CanvasObject->SetView(View);

	// PreRender the player's view.
	Draw(Flags, CanvasObject);	
}

void UDebugDrawService::Draw(const FEngineShowFlags Flags, UCanvas* Canvas)
{
	if (Canvas == nullptr || Canvas->Canvas == nullptr)
	{
		return;
	}

	UE_MT_SCOPED_WRITE_ACCESS(DelegatesDetector);
	for (int32 FlagIndex = 0; FlagIndex < Delegates.Num(); ++FlagIndex)
	{
		if (Flags.GetSingleFlag(FlagIndex) && ObservedFlags.GetSingleFlag(FlagIndex) && Delegates[FlagIndex].Num() > 0)
		{
			for (int32 i = Delegates[FlagIndex].Num() - 1; i >= 0; --i)
			{
				FDebugDrawDelegate& Delegate = Delegates[FlagIndex][i];

				if (Delegate.IsBound())
				{
					Delegate.Execute(Canvas, nullptr);
				}
				else
				{
					Delegates[FlagIndex].RemoveAtSwap(i, 1, /*bAllowShrinking=*/false);
				}
			}
		}
	}
}

