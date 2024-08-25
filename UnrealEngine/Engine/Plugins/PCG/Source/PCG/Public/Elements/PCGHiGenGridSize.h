// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

#include "PCGHiGenGridSize.generated.h"

/**
 * Set the execution grid size for downstream nodes. Enables executing a single graph across a hierarchy of grids.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGHiGenGridSizeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	EPCGHiGenGrid GetGrid() const { return HiGenGridSize; }
	uint32 GetGridSize() const { return (HiGenGridSize == EPCGHiGenGrid::Unbounded) ? PCGHiGenGrid::UnboundedGridSize() : PCGHiGenGrid::GridToGridSize(HiGenGridSize); }

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::HierarchicalGeneration; }
	virtual bool HasDynamicPins() const override { return true; }
#endif
	virtual FString GetAdditionalTitleInformation() const override;
	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName = "HiGen Grid Size"))
	EPCGHiGenGrid HiGenGridSize = EPCGHiGenGrid::Grid256;
protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class FPCGHiGenGridSizeElement : public IPCGElement
{
public:
	virtual void GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
