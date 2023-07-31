// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"

#include "UVLayoutProperties.generated.h"

/**
 * UV Layout Strategies for the UV Layout Tool
 */
UENUM()
enum class EUVLayoutType
{
	/** Apply Scale and Translation properties to all UV values */
	Transform,
	/** Uniformly scale and translate each UV island individually to pack it into the unit square, i.e. fit between 0 and 1 with overlap */
	Stack,
	/** Uniformly scale and translate UV islands collectively to pack them into the unit square, i.e. fit between 0 and 1 with no overlap */
	Repack
};


/**
 * UV Layout Settings
 */
UCLASS()
class MODELINGOPERATORS_API UUVLayoutProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Type of layout applied to input UVs */
	UPROPERTY(EditAnywhere, Category = "UV Layout")
	EUVLayoutType LayoutType = EUVLayoutType::Repack;

	/** Expected resolution of the output textures; this controls spacing left between UV islands to avoid interpolation artifacts */
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (UIMin = "64", UIMax = "2048", ClampMin = "2", ClampMax = "4096"))
	int TextureResolution = 1024;

	/** Uniform scale applied to UVs after packing */
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (UIMin = "0.1", UIMax = "5.0", ClampMin = "0.0001", ClampMax = "10000") )
	float Scale = 1;

	/** Translation applied to UVs after packing, and after scaling */
	UPROPERTY(EditAnywhere, Category = "UV Layout")
	FVector2D Translation = FVector2D(0,0);

	/** Allow the Repack layout type to flip the orientation of UV islands to save space. Note that this may cause problems for downstream operations, and therefore is disabled by default. */
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (EditCondition = "LayoutType == EUVLayoutType::Repack"))
	bool bAllowFlips = false;

};
