// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"

#include "PCGDistance.generated.h"

UENUM()
enum class PCGDistanceShape
{
	SphereBounds,
	BoxBounds,
	Center,
};

namespace PCGDistance
{
	extern const FName SourceLabel;
	extern const FName TargetLabel;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGDistanceSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Distance")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGDistanceSettings", "NodeTitle", "Distance"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:

	/** The name of the attribute to store on the point.Use 'None' to disable */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName AttributeName = TEXT("Distance");

	/** Controls whether the attribute will be a scalar or a vector */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOutputDistanceVector = false;

	/** If true, will also set the density to be 0 - 1 based on MaximumDistance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bSetDensity = false;

	/** A maximum distance to search, which is used as an optimization */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "1", PCG_Overridable))
	double MaximumDistance = 20000.0;

	/** What shape is used on the 'source' points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	PCGDistanceShape SourceShape = PCGDistanceShape::SphereBounds;

	/** What shape is used on the 'target' points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	PCGDistanceShape TargetShape = PCGDistanceShape::SphereBounds;
};

class FPCGDistanceElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
