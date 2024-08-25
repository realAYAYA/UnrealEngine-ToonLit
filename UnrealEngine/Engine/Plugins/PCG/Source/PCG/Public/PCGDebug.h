// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug, meta = (ClampMin="0", EditCondition = "ScaleMethod != EPCGDebugVisScaleMethod::Extents && bDisplayProperties", EditConditionHides, HideEditConditionToggle))
	float PointScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug, meta = (EditCondition = "bDisplayProperties", EditConditionHides, HideEditConditionToggle))
	EPCGDebugVisScaleMethod ScaleMethod = EPCGDebugVisScaleMethod::Extents;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug, meta = (EditCondition = "bDisplayProperties", EditConditionHides, HideEditConditionToggle))
	TSoftObjectPtr<UStaticMesh> PointMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug, meta = (EditCondition = "bDisplayProperties", EditConditionHides, HideEditConditionToggle))
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	TSoftObjectPtr<UMaterialInterface> GetMaterial() const;

#if WITH_EDITORONLY_DATA
	// This can be set false to hide the debugging properties.
	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bDisplayProperties = true;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
