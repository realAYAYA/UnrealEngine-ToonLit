// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGFilterByType.generated.h"

/** Filters an input collection based on data type. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGFilterByTypeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("FilterByType"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
	virtual bool HasDynamicPins() const override { return true; }
	virtual bool ShouldDrawNodeCompact() const override { return true; }
#endif
	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGDataType TargetType = EPCGDataType::Any;
};

class FPCGFilterByTypeElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const;
};
