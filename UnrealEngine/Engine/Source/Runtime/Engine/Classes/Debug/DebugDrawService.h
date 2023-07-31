// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ShowFlags.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Misc/MTAccessDetector.h"
#include "HAL/CriticalSection.h"
#include "DebugDrawService.generated.h"

class FCanvas;
class FSceneView;

/** 
 * 
 */
DECLARE_DELEGATE_TwoParams(FDebugDrawDelegate, class UCanvas*, class APlayerController*);

UCLASS(config=Engine)
class ENGINE_API UDebugDrawService : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	static FDelegateHandle Register(const TCHAR* Name, const FDebugDrawDelegate& NewDelegate);
	static void Unregister(FDelegateHandle HandleToRemove);

	// Draws debug canvas that has already been initialized to a viewport
	static void Draw(const FEngineShowFlags Flags, class UCanvas* Canvas);

	// Initialize a debug canvas object then calls above draw. If CanvasObject is null it will find/create it for you
	static void Draw(const FEngineShowFlags Flags, class FViewport* Viewport, FSceneView* View, FCanvas* Canvas, class UCanvas* CanvasObject = nullptr);

private:

	/**
	 * Synchronization object for delegate registration since it can happen from multiple threads from
	 * primitives added in batch through
	 *		ParallelFor(AddPrimitiveBatches.Num(), [&](int32 Index)
	 *			{
	 *				FOptionalTaskTagScope Scope(ETaskTag::EParallelGameThread);
	 *				Scene->AddPrimitive(...);
	 *			}
	 *
	 * Critical section used only for registration since it should not be required for the other accesses (i.e. Unregister &Draw).
	 * In those cases we use the AccessDetector.
	 */
	static FCriticalSection DelegatesLock;
#if ENABLE_MT_DETECTOR
	static FRWRecursiveAccessDetector DelegatesDetector;
#endif
	static TArray<TArray<FDebugDrawDelegate> > Delegates;
	static FEngineShowFlags ObservedFlags;
};
