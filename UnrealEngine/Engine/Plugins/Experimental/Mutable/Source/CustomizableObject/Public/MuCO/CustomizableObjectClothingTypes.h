// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothLODData.h"

#include "CustomizableObjectClothingTypes.generated.h"


USTRUCT()
struct FCustomizableObjectClothConfigData
{
	GENERATED_BODY()
	
	UPROPERTY()
	FString ClassPath;
	
	UPROPERTY()
	FName ConfigName;

	UPROPERTY()
	TArray<uint8> ConfigBytes;

	friend FArchive& operator<<(FArchive& Ar, FCustomizableObjectClothConfigData& ClothConfig)
	{
		Ar << ClothConfig.ClassPath;
		Ar << ClothConfig.ConfigName;
		Ar << ClothConfig.ConfigBytes;

		return Ar;
	}
};

USTRUCT()
struct FCustomizableObjectClothingAssetData
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FClothLODDataCommon> LodData;

	UPROPERTY()
	TArray<int32> LodMap;

	UPROPERTY()
	TArray<FName> UsedBoneNames;

	UPROPERTY()
	TArray<int32> UsedBoneIndices;

	UPROPERTY()
	int32 ReferenceBoneIndex;

	UPROPERTY()
	TArray<FCustomizableObjectClothConfigData> ConfigsData;
	
	UPROPERTY()
	FString PhysicsAssetPath;

	UPROPERTY()
	FName Name;
	
	UPROPERTY()
	FGuid OriginalAssetGuid = FGuid{};
	
	friend FArchive& operator<<(FArchive& Ar, FCustomizableObjectClothingAssetData& ClothData)
	{	
		int32 LodDataCount = ClothData.LodData.Num();
		Ar << LodDataCount;

		if ( Ar.IsLoading() )
		{
			ClothData.LodData.SetNum(LodDataCount);
		}

		for ( FClothLODDataCommon& ClothLodData :  ClothData.LodData )
		{
			ClothLodData.Serialize( Ar );
		}

		Ar << ClothData.LodMap;
		Ar << ClothData.UsedBoneNames;
		Ar << ClothData.UsedBoneIndices;
		Ar << ClothData.ReferenceBoneIndex;
		Ar << ClothData.ConfigsData;
		Ar << ClothData.PhysicsAssetPath;
		Ar << ClothData.Name;
		Ar << ClothData.OriginalAssetGuid;
		
		return Ar;
	}
};
