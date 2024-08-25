// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGMetadataPartition.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGMetadataPartitionSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("AttributePartition")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGMetadataPartitionSettings", "NodeTitle", "Attribute Partition"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual bool HasDynamicPins() const override { return true; }
#endif
	
	virtual FString GetAdditionalTitleInformation() const override;

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// TODO: Should be overridable once array override is supported
	/** The data will be partitioned on these selected attributes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGAttributePropertyInputSelector> PartitionAttributeSelectors = {FPCGAttributePropertyInputSelector()};

	// TODO: Should be deprecated once array override is supported
	UPROPERTY(meta = (PCG_Overridable))
	FString PartitionAttributeNames;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName PartitionAttribute_DEPRECATED = NAME_None;

	UPROPERTY()
	FPCGAttributePropertyInputSelector PartitionAttributeSource_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};

class FPCGMetadataPartitionElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
