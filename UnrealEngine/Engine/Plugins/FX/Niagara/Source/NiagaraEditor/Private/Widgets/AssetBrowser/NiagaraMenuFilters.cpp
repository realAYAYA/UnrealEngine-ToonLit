// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetBrowser/NiagaraMenuFilters.h"

#define LOCTEXT_NAMESPACE "NiagaraMenuFilters"

void UNiagaraTagsContentBrowserFilterData::AddTagGuid(const FGuid& InGuid)
{
	ActiveTagGuids.AddUnique(InGuid);
	OnActiveTagGuidsChangedDelegate.Broadcast();
}

void UNiagaraTagsContentBrowserFilterData::RemoveTagGuid(const FGuid& InGuid)
{
	ActiveTagGuids.Remove(InGuid);
	OnActiveTagGuidsChangedDelegate.Broadcast();
}

bool UNiagaraTagsContentBrowserFilterData::ContainsActiveTagGuid(const FGuid& InGuid)
{
	return ActiveTagGuids.Contains(InGuid);
}

FName FNiagaraAssetBrowserMainFilter::GetIdentifier() const
{
	return FName(FText::AsCultureInvariant(GetDisplayName()).ToString());
}

FText FNiagaraAssetBrowserMainFilter::GetDisplayName() const
{
	if(FilterMode == EFilterMode::All)
	{
		return LOCTEXT("NiagaraMainFilterLabel_All", "All");
	}
	else if(FilterMode == EFilterMode::NiagaraAssetTag)
	{
		return FText::FromString(AssetTagDefinition.AssetTag.ToString());
	}
	else if(FilterMode == EFilterMode::NiagaraAssetTagDefinitionsAsset)
	{
		return AssetTagDefinitionsAsset->GetDisplayName();
	}
	else if(FilterMode == EFilterMode::Recent)
	{
		return LOCTEXT("NiagaraMainFilterLabel_Recent", "Recent");
	}
	else if(FilterMode == EFilterMode::Custom)
	{
		return CustomDisplayName;
	}

	return FText::FromString("Unknown Name");
}

bool FNiagaraAssetBrowserMainFilter::IsAssetRecent(const FAssetData& AssetCandidate) const
{
	return IsAssetRecentDelegate.Execute(AssetCandidate);
}

bool FNiagaraAssetBrowserMainFilter::ShouldCustomFilterAsset(const FAssetData& AssetCandidate) const
{
	return CustomShouldFilterAsset.Execute(AssetCandidate);
}

bool FNiagaraAssetBrowserMainFilter::DoesAssetHaveTag(const FAssetData& AssetCandidate) const
{
	return AssetTagDefinition.DoesAssetDataContainTag(AssetCandidate);
}

bool FNiagaraAssetBrowserMainFilter::DoesAssetHaveAnyTagFromTagDefinitionsAsset(const FAssetData& AssetCandidate) const
{
	return AssetTagDefinitionsAsset->DoesAssetDataContainAnyTag(AssetCandidate);
}

#undef LOCTEXT_NAMESPACE
