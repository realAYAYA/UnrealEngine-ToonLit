// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGAssetExporter.h"

#include "AssetRegistry/AssetData.h"

#include "PCGAssetExporterUtils.generated.h"

UCLASS()
class PCGEDITOR_API UPCGAssetExporterUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PCG|IO")
	static UPackage* CreateAsset(UPCGAssetExporter* Exporter, FPCGAssetExporterParameters Parameters = FPCGAssetExporterParameters());

	UFUNCTION(BlueprintCallable, Category = "PCG|IO")
	static void UpdateAssets(const TArray<FAssetData>& PCGAssets, FPCGAssetExporterParameters Parameters = FPCGAssetExporterParameters());
};