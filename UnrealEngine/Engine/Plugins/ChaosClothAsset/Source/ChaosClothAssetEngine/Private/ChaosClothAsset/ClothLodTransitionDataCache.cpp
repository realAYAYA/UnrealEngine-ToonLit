// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothLodTransitionDataCache.h"

bool FChaosClothAssetLodTransitionDataCache::Serialize(FArchive& Ar)
{
	// Serialize normal tagged property data
	if (Ar.IsLoading() || Ar.IsSaving())
	{
		UScriptStruct* const Struct = FChaosClothAssetLodTransitionDataCache::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, (uint8*)this, Struct, nullptr);
	}

	Ar << ModelHash;

	Ar << LODTransitionUpData;
	Ar << LODTransitionDownData;

	// Return true to confirm that serialization has already been taken care of
	return true;
}