// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_AnimBlueprintInterface.h"

void FAssetTypeActions_AnimBlueprintInterface::BuildBackendFilter(FARFilter& InFilter)
{
	FAssetTypeActions_AnimBlueprint::BuildBackendFilter(InFilter);

	InFilter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UBlueprint, BlueprintType), FString(TEXT("BPTYPE_Interface")));
}

FName FAssetTypeActions_AnimBlueprintInterface::GetFilterName() const
{
	FName SuperName = FAssetTypeActions_AnimBlueprint::GetFilterName();
	return *(SuperName.ToString() + TEXT("Interface"));
}