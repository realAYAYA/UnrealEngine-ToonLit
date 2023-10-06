// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PCGSettings.h"

#include "UObject/NameTypes.h"
#include "PCGNumberOfPoints.generated.h"

/**
 * Used to wrangle multiple input wires into one output wire for organizational purposes.
 */

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGNumberOfPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Get Number of Points")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGNumberOfPointsSettings", "NodeTitle", "Get Number of Points"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGNumberOfPointsSettings", "NodeTooltip", "Return the number of points in a point data."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif


protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName OutputAttributeName = TEXT("NumPoints");
};

class FPCGNumberOfPointsElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};