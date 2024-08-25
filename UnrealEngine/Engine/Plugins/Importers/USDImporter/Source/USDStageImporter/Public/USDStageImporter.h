// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FUsdStageImportContext;

class USDSTAGEIMPORTER_API UUsdStageImporter
{
public:
	void ImportFromFile(FUsdStageImportContext& ImportContext);

	bool ReimportSingleAsset(
		FUsdStageImportContext& ImportContext,
		UObject* OriginalAsset,
		const FString& OriginalPrimPath,
		UObject*& OutReimportedAsset
	);
};
