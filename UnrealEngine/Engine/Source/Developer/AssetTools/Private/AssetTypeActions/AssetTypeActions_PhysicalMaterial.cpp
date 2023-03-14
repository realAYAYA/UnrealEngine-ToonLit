// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_PhysicalMaterial.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UClass* FAssetTypeActions_PhysicalMaterial::GetSupportedClass() const
{
	return UPhysicalMaterial::StaticClass();
}
