// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorActionUtility.h"

#include "EditorUtilityAssetPrototype.h"
#include "EditorUtilityBlueprint.h"

#define LOCTEXT_NAMESPACE "ActorActionUtility"

void UActorActionUtility::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	
	const bool IsUpToDate =
		!GetClass()->IsFunctionImplementedInScript("GetSupportedClass");

	if (IsUpToDate)
	{
		FAssetActionUtilityPrototype::AddTagsFor_Version(OutTags);
	}
	
	FAssetActionUtilityPrototype::AddTagsFor_SupportedClasses(SupportedClasses, OutTags);
	FAssetActionUtilityPrototype::AddTagsFor_CallableFunctions(this, OutTags);
}

#undef LOCTEXT_NAMESPACE