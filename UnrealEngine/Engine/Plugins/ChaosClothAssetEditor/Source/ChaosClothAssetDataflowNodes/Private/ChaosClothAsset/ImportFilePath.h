// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportFilePath.generated.h"

USTRUCT()
struct FChaosClothAssetImportFilePath
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Import File Path")
	FString FilePath;

	UPROPERTY(EditAnywhere, Category = "Import File Path")
	bool bForceReimport = false;
};
