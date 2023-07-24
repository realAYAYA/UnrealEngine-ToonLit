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
* The output will be the target data with the updated metadata.
* If the TargetAttributeName is None, it will use SourceAttributeName instead.
* 
* To do the same but with a Source param data, use CreateAttribute.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGAttributeTransferSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
#endif
	virtual FName AdditionalTaskName() const override;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName SourceAttributeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName TargetAttributeName;

protected:
	virtual FPCGElementPtr CreateElement() const override;
};


class FPCGAttributeTransferElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	UPCGSpatialData* TransferSpatialToSpatial(FPCGContext* Context, const UPCGSpatialData* SourceData, const UPCGSpatialData* TargetData) const;
	UPCGPointData* TransferPointToPoint(FPCGContext* Context, const UPCGPointData* SourceData, const UPCGPointData* TargetData) const;
};
