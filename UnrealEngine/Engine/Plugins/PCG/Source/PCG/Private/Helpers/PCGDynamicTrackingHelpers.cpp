// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGDynamicTrackingHelpers.h"

#if WITH_EDITOR

#include "PCGComponent.h"
#include "PCGContext.h"

void FPCGDynamicTrackingHelper::EnableAndInitialize(const FPCGContext* InContext, int32 OptionalNumElements)
{
	if (InContext && InContext->SourceComponent.IsValid())
	{
		const UPCGSettings* Settings = InContext->GetOriginalSettings<UPCGSettings>();
		if (Settings && Settings->CanDynamicallyTrackKeys())
		{
			CachedComponent = InContext->SourceComponent;
			bDynamicallyTracked = true;
			DynamicallyTrackedKeysAndCulling.Reserve(OptionalNumElements);
		}
	}
}

void FPCGDynamicTrackingHelper::AddToTracking(FPCGSelectionKey&& InKey, bool bIsCulled)
{
	if (bDynamicallyTracked)
	{
		DynamicallyTrackedKeysAndCulling.AddUnique(TPair<FPCGSelectionKey, bool>(std::forward<FPCGSelectionKey>(InKey), bIsCulled));
	}
}

void FPCGDynamicTrackingHelper::Finalize(const FPCGContext* InContext)
{
	if (InContext && bDynamicallyTracked && CachedComponent == InContext->SourceComponent)
	{
		if (UPCGComponent* SourceComponent = CachedComponent.Get())
		{
			SourceComponent->RegisterDynamicTracking(InContext->GetOriginalSettings<UPCGSettings>(), DynamicallyTrackedKeysAndCulling);
		}
	}
}

void FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(FPCGContext* InContext, FPCGSelectionKey&& InKey, bool bIsCulled)
{
	if (InContext)
	{
		if (UPCGComponent* SourceComponent = InContext->SourceComponent.Get())
		{
			TPair<FPCGSelectionKey, bool> NewPair(std::forward<FPCGSelectionKey>(InKey), bIsCulled);
			SourceComponent->RegisterDynamicTracking(InContext->GetOriginalSettings<UPCGSettings>(), MakeArrayView(&NewPair, 1));
		}
	}
}

#endif // WITH_EDITOR
