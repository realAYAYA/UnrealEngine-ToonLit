// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "AssetRegistry/AssetData.h"

UExternalDataLayerAsset::UExternalDataLayerAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataLayerType = EDataLayerType::Runtime;
	bSupportsActorFilters = false;
}

#if WITH_EDITOR

void UExternalDataLayerAsset::OnCreated()
{
	Super::OnCreated();
	UID.Value = FExternalDataLayerUID::NewUID();
}

void UExternalDataLayerAsset::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	UID.Value = FExternalDataLayerUID::NewUID();
}

namespace ExternalDataLayerAsset
{
	static const FName NAME_ExternalDataLayerUID(TEXT("ExternalDataLayerUID"));
};

void UExternalDataLayerAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);
	Context.AddTag(FAssetRegistryTag(ExternalDataLayerAsset::NAME_ExternalDataLayerUID, *UID.ToString(), FAssetRegistryTag::TT_Hidden));
}

bool UExternalDataLayerAsset::GetAssetRegistryInfoFromPackage(const FAssetData& InAsset, FExternalDataLayerUID& OutExternalDataLayerUID)
{
	FString Value;
	if (InAsset.GetTagValue(ExternalDataLayerAsset::NAME_ExternalDataLayerUID, Value))
	{
		return FExternalDataLayerUID::Parse(Value, OutExternalDataLayerUID);
	}
	return false;
}

#endif