// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Elements/PCGActorSelector.h"
#include "Containers/Array.h"

struct FPCGContext;
class UPCGComponent;

/**
* Simple helper class to factorize the logic for gathering dynamic tracking keys and pushing them to the component.
* Only work for settings that override CanDynamicalyTrackKeys.
*/
struct PCG_API FPCGDynamicTrackingHelper
{
public:
	/** Enable dynamic tracking, will cache the weak ptr of the component and optionally resize the array for keys. */
	void EnableAndInitialize(const FPCGContext* InContext, int32 OptionalNumElements = 0);

	/** Add the key to the tracking, will be uniquely added to the array. */
	void AddToTracking(FPCGSelectionKey&& InKey, bool bIsCulled);

	/** Push all the tracked keys to the ceched component if still valid and the same as the context. */
	void Finalize(const FPCGContext* InContext);

	/** Convenient function to push just a single tracking key to the component. */
	static void AddSingleDynamicTrackingKey(FPCGContext* InContext, FPCGSelectionKey&& InKey, bool bIsCulled);

private:
	bool bDynamicallyTracked = false;
	TWeakObjectPtr<UPCGComponent> CachedComponent;
	TArray<TPair<FPCGSelectionKey, bool>, TInlineAllocator<16>> DynamicallyTrackedKeysAndCulling;
};

#endif // WITH_EDITOR
