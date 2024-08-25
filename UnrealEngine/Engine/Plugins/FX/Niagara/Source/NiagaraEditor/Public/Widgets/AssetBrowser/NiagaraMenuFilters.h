// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FrontendFilterBase.h"
#include "NiagaraAssetTagDefinitions.h"
#include "NiagaraMenuFilters.generated.h"

UCLASS()
class UNiagaraTagsContentBrowserFilterData : public UObject
{
	GENERATED_BODY()

public:
	void AddTagGuid(const FGuid& InGuid);
	void RemoveTagGuid(const FGuid& InGuid);
	bool ContainsActiveTagGuid(const FGuid& InGuid);
	int32 GetNumActiveTagGuids() const { return ActiveTagGuids.Num(); }
	const TArray<FGuid>& GetActiveTagGuids() const { return ActiveTagGuids; }
	void ResetActiveTagGuids() { ActiveTagGuids.Reset(); }

	FSimpleMulticastDelegate& OnActiveTagGuidsChanged() { return OnActiveTagGuidsChangedDelegate; }
private:
	UPROPERTY()
	TArray<FGuid> ActiveTagGuids;

	FSimpleMulticastDelegate OnActiveTagGuidsChangedDelegate;
};

UCLASS()
class UNiagaraTagsContentBrowserFilterContext : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UNiagaraTagsContentBrowserFilterData> FilterData;
};


struct FNiagaraAssetBrowserMainFilter
{
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsAssetRecent, const FAssetData&)
	
	enum class EFilterMode
	{
		All,
		NiagaraAssetTag,
		NiagaraAssetTagDefinitionsAsset,
		Recent,
		Custom
	};

	FNiagaraAssetBrowserMainFilter(EFilterMode InFilterMode) : FilterMode(InFilterMode) {}
	
	EFilterMode FilterMode;
	TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> ChildFilters;

	// If in mode Asset Tag, this is the tag to filter against
	FNiagaraAssetTagDefinition AssetTagDefinition;
	// If in mode Asset Tag Definitions Asset, this is the asset to filter against (should include all assets that match any tag within the asset)
	TObjectPtr<const UNiagaraAssetTagDefinitions> AssetTagDefinitionsAsset;
	// If in mode Recent, this is set and used to determine if an asset is recent
	FIsAssetRecent IsAssetRecentDelegate;
	// If in mode Custom, this needs to be set
	FOnShouldFilterAsset CustomShouldFilterAsset;

	/** The custom guid should always be set the same for a specific main filter.
	 * Should be used if there are multiple items with the same filter mode that aren't niagara tags (i.e. set to Custom) */
	FGuid CustomGuid;
	
	FText CustomDisplayName;

	FName GetIdentifier() const;
	FText GetDisplayName() const;
	
	bool IsAssetRecent(const FAssetData& AssetCandidate) const;
	bool ShouldCustomFilterAsset(const FAssetData& AssetCandidate) const;
	bool DoesAssetHaveTag(const FAssetData& AssetCandidate) const;
	bool DoesAssetHaveAnyTagFromTagDefinitionsAsset(const FAssetData& AssetCandidate) const;

	bool operator==(const FNiagaraAssetBrowserMainFilter& Other) const
	{
		if(FilterMode != Other.FilterMode)
		{
			return false;
		}

		if(FilterMode == EFilterMode::NiagaraAssetTag)
		{
			return AssetTagDefinition == Other.AssetTagDefinition;
		}

		if(FilterMode == EFilterMode::NiagaraAssetTagDefinitionsAsset)
		{
			return AssetTagDefinitionsAsset == Other.AssetTagDefinitionsAsset;
		}

		if(CustomGuid.IsValid())
		{
			return CustomGuid == Other.CustomGuid;
		}
		
		return true;
	}
};
