// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Actor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Actor::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	for(UObject* Object : InObjects)
	{
		if (AActor* Actor = Cast<AActor>(Object))
		{
			if (Actor->OpenAssetEditor())
			{
				return;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
