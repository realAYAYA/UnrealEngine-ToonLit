// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"

class AStaticMeshActor;
class UMaterialInstanceConstant;
struct FUAssetMeta;

class FMaterialUtils
{
public:
	static UMaterialInstanceConstant* CreateInstanceMaterial(const FString& MasterMaterialPath, const FString& InstanceDestination, const FString& AssetName);
	static bool ShouldOverrideMaterial(const FString& AssetType);
	static TArray<AStaticMeshActor*> ApplyMaterialToSelection(const FString& InstancePath );
	static UMaterialInstanceConstant* CreateMaterialOverride(FUAssetMeta AssetMetaData);
	static void ApplyMaterialInstance(FUAssetMeta AssetMetaData, UMaterialInstanceConstant* MaterialInstance);	

};
