// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionGuidViewModelRegistry.h"
#include "Extensions/IAvaTransitionGuidExtension.h"
#include "ViewModels/AvaTransitionViewModel.h"

bool TAvaTransitionViewModelRegistryKey<FGuid>::IsValid(const FGuid& InKey)
{
	return InKey.IsValid();
}

bool TAvaTransitionViewModelRegistryKey<FGuid>::TryGetKey(const TSharedRef<FAvaTransitionViewModel>& InViewModel, FGuid& OutKey)
{
	if (IAvaTransitionGuidExtension* GuidExtension = InViewModel->CastTo<IAvaTransitionGuidExtension>())
	{
		OutKey = GuidExtension->GetGuid();
		return true;
	}
	return false;
}
