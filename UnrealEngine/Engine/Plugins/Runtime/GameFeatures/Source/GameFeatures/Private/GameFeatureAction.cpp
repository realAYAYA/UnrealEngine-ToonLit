// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureAction.h"
#include "GameFeatureData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction)

UGameFeatureData* UGameFeatureAction::GetGameFeatureData() const
{
	for (UObject* Obj = GetOuter(); Obj; Obj = Obj->GetOuter())
	{
		if (UGameFeatureData* GFD = Cast<UGameFeatureData>(Obj))
		{
			return GFD;
		}
	}

	return nullptr;
}

void UGameFeatureAction::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	// Call older style if not overridden
	OnGameFeatureActivating();
}

bool UGameFeatureAction::IsGameFeaturePluginRegistered() const
{
	UGameFeatureData* GameFeatureData = GetGameFeatureData();
	return !!GameFeatureData ? GameFeatureData->IsGameFeaturePluginRegistered() : false;
}

bool UGameFeatureAction::IsGameFeaturePluginActive() const
{
	UGameFeatureData* GameFeatureData = GetGameFeatureData();
	return !!GameFeatureData ? GameFeatureData->IsGameFeaturePluginActive() : false;
}
