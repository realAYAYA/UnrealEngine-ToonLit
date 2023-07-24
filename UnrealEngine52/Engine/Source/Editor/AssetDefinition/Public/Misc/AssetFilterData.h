// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetCategoryPath.h"

#include "AssetFilterData.generated.h"

USTRUCT()
struct FAssetFilterData
{
	GENERATED_BODY()

public:
	FString Name;
	
	FText DisplayText;

	TArray<FAssetCategoryPath> FilterCategories;
	
	FARFilter Filter;
};
