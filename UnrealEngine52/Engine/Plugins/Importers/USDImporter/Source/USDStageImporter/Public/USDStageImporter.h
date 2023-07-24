// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDStageActor.h"

#include "CoreMinimal.h"

class UUsdAssetImportData;
struct FUsdStageImportContext;

class USDSTAGEIMPORTER_API UUsdStageImporter
{
public:
	void ImportFromFile(FUsdStageImportContext& ImportContext);

	bool ReimportSingleAsset(FUsdStageImportContext& ImportContext, UObject* OriginalAsset, UUsdAssetImportData* OriginalImportData, UObject*& OutReimportedAsset);
};
