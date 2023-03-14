// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
