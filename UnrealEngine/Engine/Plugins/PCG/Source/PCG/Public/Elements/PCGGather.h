// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PCGSettings.h"

#include "PCGGather.generated.h"

/**
 * Used to wrangle multiple input wires into one output wire for organizational purposes.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGGatherSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Gather")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGatherSettings", "NodeTitle", "Gather"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGGatherSettings", "NodeTooltip", "Gathers multiple data in a single collection. Can also be used to order execution through the dependency-only pin."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
	virtual bool HasDynamicPins() const override { return true; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class FPCGGatherElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGGather
{
	/** Gathers the input data into a single data collection and updates the tags */
	FPCGDataCollection GatherDataForPin(const FPCGDataCollection& InputData, const FName InputLabel = PCGPinConstants::DefaultInputLabel, const FName OutputLabel = PCGPinConstants::DefaultOutputLabel);
}


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
