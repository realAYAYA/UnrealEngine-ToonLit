// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EditorFramework/AssetImportData.h"
#include "GroomAssetImportData.generated.h"

UCLASS()
class HAIRSTRANDSCORE_API UGroomAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	TObjectPtr<class UGroomImportOptions> ImportOptions;
};
