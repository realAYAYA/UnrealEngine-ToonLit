// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGAssetExporter.h"

#include "Engine/World.h"

#include "PCGLevelToAsset.generated.h"

class UPackage;

UCLASS(BlueprintType, Blueprintable, meta = (ShowWorldContextPin))
class PCGEDITOR_API UPCGLevelToAsset : public UPCGAssetExporter
{
	GENERATED_BODY()

public:
	/** Creates/updates a PCG Asset per given world. Allows exporter subclassing by passing in a Subclass. */
	static void CreateOrUpdatePCGAssets(const TArray<FAssetData>& WorldAssets, const FPCGAssetExporterParameters& Parameters = FPCGAssetExporterParameters(), TSubclassOf<UPCGLevelToAsset> Subclass = {});

	/** Creates/Updates a PCG Asset for a specific world. Allows exporter subclassing by passing in a Subclass. Will return null if it fails, or the package that was modified on success. */
	static UPackage* CreateOrUpdatePCGAsset(TSoftObjectPtr<UWorld> World, const FPCGAssetExporterParameters& Parameters = FPCGAssetExporterParameters(), TSubclassOf<UPCGLevelToAsset> Subclass = {});

	/** Creates/Updates a PCG Asset for a specific world. Allows exporter subclassing (and settings creation by extension). Will return null if it fails, or the package that was modified on success. */
	static UPackage* CreateOrUpdatePCGAsset(UWorld* World, const FPCGAssetExporterParameters& Parameters = FPCGAssetExporterParameters(), TSubclassOf<UPCGLevelToAsset> Subclass = {});

	/** Parses the world and fills in the provided data asset. Implement this in BP to drive the generation in a custom manner. */
	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "Export World", ForceAsFunction))
	bool BP_ExportWorld(UWorld* World, const FString& PackageName, UPCGDataAsset* Asset);

	UFUNCTION(BlueprintCallable, Category="PCG|IO")
	void SetWorld(UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "PCG|IO")
	UWorld* GetWorld() const;

protected:
	//~Being UPCGAssetExporter interface
	virtual bool ExportAsset(const FString& PackageName, UPCGDataAsset* Asset) override;
	virtual UPackage* UpdateAsset(const FAssetData& PCGAsset) override;
	//~End UPCGAssetExporter interface

	UWorld* WorldToExport = nullptr;
};