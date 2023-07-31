// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/UnrealEdEngine.h"

struct FAssetData;
struct FDomainDatabase;
struct FDomainData;

class FDomainAssetReferenceFilter : public IAssetReferenceFilter
{
public:
	FDomainAssetReferenceFilter(const FAssetReferenceFilterContext& Context, TSharedPtr<FDomainDatabase> InDomainDB);
	~FDomainAssetReferenceFilter();

	//~IAssetReferenceFilter interface
	virtual bool PassesFilter(const FAssetData& AssetData, FText* OutOptionalFailureReason = nullptr) const override;
	//~End of IAssetReferenceFilter

	// Update any cached information for all filters
	static void UpdateAllFilters();
private:
	bool PassesFilterImpl(const FAssetData& AssetData, FText& OutOptionalFailureReason) const;

	void DetermineReferencingDomain();

	/** Heuristic to find actual assets from preview assets (i.e., the material editor's preview material) */
	void TryGetAssociatedAssetsFromPossiblyPreviewObject(UObject* PossiblyPreviewObject, TArray<FAssetData>& InOutAssetsToConsider) const;

private:
	static TArray<FDomainAssetReferenceFilter*> FilterInstances;

	TSharedPtr<FDomainDatabase> DomainDB;

	TArray<FAssetData> OriginalReferencingAssets;
	TSet<TSharedPtr<FDomainData>> ReferencingDomains;

	FText Failure_CouldNotDetermineDomain;
};
