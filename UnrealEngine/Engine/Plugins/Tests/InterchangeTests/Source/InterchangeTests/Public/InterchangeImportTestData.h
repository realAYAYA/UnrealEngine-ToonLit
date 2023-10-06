// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"

class UInterchangeImportTestPlan;
class UInterchangeResultsContainer;


struct FInterchangeImportTestData
{
	FString DestAssetPackagePath;
	FString DestAssetFilePath;
	FAssetData AssetData;
	UInterchangeImportTestPlan* TestPlan = nullptr;
	UInterchangeResultsContainer* InterchangeResults = nullptr;
	TArray<UObject*> ResultObjects;
	TArray<FAssetData> ImportedAssets;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
