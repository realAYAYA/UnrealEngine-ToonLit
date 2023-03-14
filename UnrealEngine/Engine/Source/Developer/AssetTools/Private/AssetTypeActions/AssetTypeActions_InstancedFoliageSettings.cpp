// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_InstancedFoliageSettings.h"
#include "FoliageType_InstancedStaticMesh.h"

UClass* FAssetTypeActions_InstancedFoliageSettings::GetSupportedClass() const
{
	return UFoliageType_InstancedStaticMesh::StaticClass();
}
