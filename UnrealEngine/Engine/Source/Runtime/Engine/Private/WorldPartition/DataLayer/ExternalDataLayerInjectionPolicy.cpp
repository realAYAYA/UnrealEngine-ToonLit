// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/ExternalDataLayerInjectionPolicy.h"
#include "WorldPartition/DataLayer/ExternalDataLayerEngineSubsystem.h"
#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"

#define LOCTEXT_NAMESPACE "ExternalDataLayerInjectionPolicy"

#if WITH_EDITOR
bool UExternalDataLayerInjectionPolicy::CanInject(const UWorld* InWorld, const UExternalDataLayerAsset* InExternalDataLayerAsset, const UObject* InClient, FText* OutFailureReason) const
{
	if (IsRunningCookCommandlet())
	{
		if (!UExternalDataLayerEngineSubsystem::Get().IsExternalDataLayerAssetRegistered(InExternalDataLayerAsset, InClient))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FText::Format(LOCTEXT("CantInjectNotRegisteredExternalDataLayerAsset", "External Data Layer Asset {0} not registered"), FText::FromString(InExternalDataLayerAsset->GetName()));
			}
			return false;
		}
	}
	// Only allow injection if active
	else if (!UExternalDataLayerEngineSubsystem::Get().IsExternalDataLayerAssetActive(InExternalDataLayerAsset, InClient))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FText::Format(LOCTEXT("CantInjectNotActiveExternalDataLayerAsset", "External Data Layer Asset {0} not active"), FText::FromString(InExternalDataLayerAsset->GetName()));
		}
		return false;
	}
	return true;
}
#endif

#undef LOCTEXT_NAMESPACE 