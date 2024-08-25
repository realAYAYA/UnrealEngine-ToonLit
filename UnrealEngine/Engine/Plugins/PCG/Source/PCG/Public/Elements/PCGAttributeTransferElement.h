// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGAttributeTransferElement.generated.h"

class UPCGPointData;
class UPCGSpatialData;

namespace PCGAttributeTransferConstants
{
	const FName NodeName = TEXT("TransferAttribute");
	const FText NodeTitle = NSLOCTEXT("PCGAttributeTransferElement", "NodeTitle", "Transfer Attribute");
	const FName SourceLabel = TEXT("Source");
	const FName TargetLabel = TEXT("Target");
}

/**
* Transfer an attribute from a source metadata to a target data.
* Only support Spatial to Spatial and Points to Points, and they need to match.
*  - For Spatial data, number of entries in the metadata should be the same between source and target.
*  - For Point data, number of points should be the same between source and target.
* 
* To do the same but with a Source param data, use CreateAttribute.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGAttributeTransferSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FText GetNodeTooltipText() const override;
	virtual bool HasDynamicPins() const override { return true; }
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;
#endif
	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;
	virtual FString GetAdditionalTitleInformation() const override;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override { return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector SourceAttributeProperty;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyOutputSelector TargetAttributeProperty;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName SourceAttributeName_DEPRECATED;

	UPROPERTY()
	FName TargetAttributeName_DEPRECATED;
#endif
};

class FPCGAttributeTransferElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
