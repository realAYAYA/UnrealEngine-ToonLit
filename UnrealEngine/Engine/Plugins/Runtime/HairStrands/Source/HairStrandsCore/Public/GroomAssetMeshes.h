// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "GroomAssetCards.h"

#include "GroomAssetMeshes.generated.h"


class UMaterialInterface;
class UStaticMesh;


USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairGroupsMeshesSourceDescription
{
	GENERATED_BODY()

	FHairGroupsMeshesSourceDescription();

	/* Deprecated */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY()
	FName MaterialSlotName;

	UPROPERTY(EditAnywhere, Category = "MeshSettings", meta = (ToolTip = "Mesh settings"))
	TObjectPtr<class UStaticMesh> ImportedMesh;

	UPROPERTY(EditAnywhere, Category = "MeshesSource")
	FHairGroupCardsTextures Textures;

	/* Group index on which this mesh geometry will be used (#hair_todo: change this to be a dropdown selection menu in FHairLODSettings instead) */
	UPROPERTY(EditAnywhere, Category = "MeshesSource")
	int32 GroupIndex = 0;

	/* LOD on which this mesh geometry will be used. -1 means not used  (#hair_todo: change this to be a dropdown selection menu in FHairLODSettings instead) */
	UPROPERTY(EditAnywhere, Category = "MeshesSource")
	int32 LODIndex = -1;

	UPROPERTY(Transient)
	FString ImportedMeshKey;

	bool operator==(const FHairGroupsMeshesSourceDescription& A) const;

	FString GetMeshKey() const;
	bool HasMeshChanged() const;
	void UpdateMeshKey();
};