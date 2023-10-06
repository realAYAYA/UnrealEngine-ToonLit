// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "UObject/ObjectMacros.h"
#include "MaterialTypes.h"

#include "MaterialExpressionSparseVolumeTextureBase.generated.h"

class USparseVolumeTexture;
struct FMaterialParameterMetadata;

UCLASS(abstract, hidecategories = Object, MinimalAPI)
class UMaterialExpressionSparseVolumeTextureBase : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** The Sparse Virtual Texture to sample. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SparseVolumeTexture)
	TObjectPtr<USparseVolumeTexture> SparseVolumeTexture;

	//~ Begin UMaterialExpression Interface
	ENGINE_API virtual UObject* GetReferencedTexture() const override;
	virtual bool CanReferenceTexture() const { return true; }
	//~ End UMaterialExpression Interface
};
