// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "MSAssetImportData.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"

#include "Materials/MaterialInstanceConstant.h"

class FMaterialUtils
{
public:
	static UMaterialInstanceConstant* CreateInstanceMaterial(const FString& MasterMaterialPath, const FString& InstanceDestination, const FString& AssetName);
	static bool ShouldOverrideMaterial(const FString& AssetType);
	static TArray<AStaticMeshActor*> ApplyMaterialToSelection(const FString& InstancePath );
	static UMaterialInstanceConstant* CreateMaterialOverride(FUAssetMeta AssetMetaData);
	static void ApplyMaterialInstance(FUAssetMeta AssetMetaData, UMaterialInstanceConstant* MaterialInstance);	

};