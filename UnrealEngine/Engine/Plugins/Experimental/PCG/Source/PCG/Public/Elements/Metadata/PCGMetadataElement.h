// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPin.h"
#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGMetadataElement.generated.h"

UENUM()
enum class UE_DEPRECATED(5.2, "Not used anymore") EPCGMetadataOperationTarget : uint8
{
	PropertyToAttribute,
	AttributeToProperty,
	AttributeToAttribute,
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataOperationSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGMetadataOperationSettings();

	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CopyAttribute")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGMetadataOperationSettings", "NodeTitle", "Copy Attribute"); }
	virtual FText GetNodeTooltipText() const;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGAttributePropertyInputSelector InputSource;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGAttributePropertyOutputSelector OutputTarget;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName SourceAttribute_DEPRECATED = NAME_None;

	UPROPERTY()
	EPCGPointProperties PointProperty_DEPRECATED = EPCGPointProperties::Density;

	UPROPERTY()
	FName DestinationAttribute_DEPRECATED = NAME_None;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	EPCGMetadataOperationTarget Target_DEPRECATED = EPCGMetadataOperationTarget::PropertyToAttribute;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
};

class FPCGMetadataOperationElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
