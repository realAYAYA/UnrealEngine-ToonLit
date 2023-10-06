// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "AssetRegistry/AssetData.h"
#include "Containers/Array.h"
#include "Templates/Function.h"

#include "AssetFilteringAndSortingFunctionLibrary.generated.h"

struct FAssetData;

UENUM(BlueprintType)
enum class ESortOrder : uint8
{
	Ascending,
	Descending
};

UENUM(BlueprintType)
enum class EAssetTagMetaDataSortType : uint8
{
	String,
	Numeric,
	DateTime
};

DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FAssetSortingPredicate, const FAssetData&, Left, const FAssetData&, Right);

/** This library's purpose is to facilitate Blueprints to discover assets using some filters and sort them. */
UCLASS(BlueprintType)
class VIRTUALCAMERA_API UAssetFilteringAndSortingFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/** Gets all assets which have the given tags. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips")
	static TArray<FAssetData> GetAllAssetsByMetaDataTags(const TSet<FName>& RequiredTags, const TSet<UClass*>& AllowedClasses);

	/** Sorts the assets based on a custom Blueprint delegate.
	 * @param SortingPredicate Implements a Left <= Right relation
	 */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips")
	static void SortByCustomPredicate(UPARAM(Ref) TArray<FAssetData>& Assets, FAssetSortingPredicate SortingPredicate, ESortOrder SortOrder = ESortOrder::Ascending);

	/** Sorts the assets by their asset name. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips")
	static void SortByAssetName(UPARAM(Ref) TArray<FAssetData>& Assets, ESortOrder SortOrder = ESortOrder::Ascending);
	
	/**
	 * Sorts the assets based on their meta data's type.
	 * Supported types: FString, int, float, FDateTime.
	 * 
	 * @param Assets The assets to sort
	 * @param MetaDataTag The on which the sort is based
	 * @param SortOrder Whether to sort ascending or descending
	 * @return Whether it was possible to compare all the meta data successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips")
	static bool SortByMetaData(UPARAM(Ref) TArray<FAssetData>& Assets, FName MetaDataTag, EAssetTagMetaDataSortType MetaDataType, ESortOrder SortOrder = ESortOrder::Ascending);
	
	/**
	 * Util that does the actual sorting
	 * @param Predicate Implements a Left <= Right relation
	 */
	static void SortAssets(TArray<FAssetData>& Assets, TFunctionRef<bool(const FAssetData& Left, const FAssetData& Right)> Predicate, ESortOrder SortOrder);
};
