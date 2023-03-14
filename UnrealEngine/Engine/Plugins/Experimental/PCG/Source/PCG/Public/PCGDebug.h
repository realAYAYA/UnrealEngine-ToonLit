// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGDebug.generated.h"

class UStaticMesh;
class UMaterialInterface;

UENUM()
enum class EPCGDebugVisScaleMethod : uint8
{
	Relative,
	Absolute,
	Extents
};

USTRUCT(BlueprintType)
struct PCG_API FPCGDebugVisualizationSettings
{
	GENERATED_BODY()

public:
	FPCGDebugVisualizationSettings();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug, meta=(ClampMin="0", EditCondition = "ScaleMethod != EPCGDebugVisScaleMethod::Extents"))
	float PointScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug)
	EPCGDebugVisScaleMethod ScaleMethod = EPCGDebugVisScaleMethod::Extents;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug)
	TSoftObjectPtr<UStaticMesh> PointMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug)
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	/** Warning: enabling this flag will have severe performance impact */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug)
	bool bCheckForDuplicates = false;

	TSoftObjectPtr<UMaterialInterface> GetMaterial() const;
};
