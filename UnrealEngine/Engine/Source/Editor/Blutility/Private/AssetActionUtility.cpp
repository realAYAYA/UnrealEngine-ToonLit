// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetActionUtility.h"

#include "EditorUtilityAssetPrototype.h"
#include "EditorUtilityBlueprint.h"
#include "AssetRegistry/AssetData.h"
#include "JsonObjectConverter.h"
#include "Misc/DataValidation.h"
#include "Serialization/JsonSerializerMacros.h"
#include "UObject/UObjectThreadContext.h"

#define LOCTEXT_NAMESPACE "AssetActionUtility"

void UAssetActionUtility::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	
	const bool IsUpToDate =
		!GetClass()->IsFunctionImplementedInScript("GetSupportedClass");
	
	if (IsUpToDate)
	{
		FAssetActionUtilityPrototype::AddTagsFor_Version(OutTags);
	}
	
	FAssetActionUtilityPrototype::AddTagsFor_SupportedClasses(SupportedClasses, OutTags);
	FAssetActionUtilityPrototype::AddTagsFor_SupportedConditions(SupportedConditions, OutTags);
	if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		FAssetActionUtilityPrototype::AddTagsFor_IsActionForBlueprints(IsActionForBlueprints(), OutTags);
	}
	FAssetActionUtilityPrototype::AddTagsFor_CallableFunctions(this, OutTags);
}
			
#undef LOCTEXT_NAMESPACE