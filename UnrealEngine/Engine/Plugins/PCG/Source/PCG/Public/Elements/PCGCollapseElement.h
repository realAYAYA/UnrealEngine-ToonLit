// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGCollapseElement.generated.h"

/** Convert input to point data, performing sampling with default settings if necessary */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCollapseSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ToPoint")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGCollapseElement", "NodeTitle", "To Point"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual bool ShouldDrawNodeCompact() const override { return true; }
	virtual bool GetCompactNodeIcon(FName& OutCompactNodeIcon) const override;
#endif
	

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

/** Converts attribute sets to point data */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGConvertToPointDataSettings : public UPCGCollapseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("AttributeSetToPoint")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGConvertToPointElement", "NodeTitle", "Attribute Set To Point"); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings interface
};

class FPCGCollapseElement : public IPCGElement
{
public:
	// Might be sampling spline/landscape or other external data, worth computing a full CRC in case we can halt change propagation/re-executions
	virtual bool ShouldComputeFullOutputDataCrc(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
