// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/AssetTypeActions_ChaosSolver.h"

#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Chaos/ChaosSolver.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_ChaosSolver::GetSupportedClass() const
{
	return UChaosSolver::StaticClass();
}

UThumbnailInfo* FAssetTypeActions_ChaosSolver::GetThumbnailInfo(UObject* Asset) const
{
	UChaosSolver* ChaosSolver = CastChecked<UChaosSolver>(Asset);
	return NewObject<USceneThumbnailInfo>(ChaosSolver, NAME_None, RF_Transactional);
}

void FAssetTypeActions_ChaosSolver::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
}

#undef LOCTEXT_NAMESPACE
