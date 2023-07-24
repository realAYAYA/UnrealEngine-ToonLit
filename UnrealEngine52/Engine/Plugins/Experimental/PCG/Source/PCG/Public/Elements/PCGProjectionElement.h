// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/PCGProjectionParams.h"
#include "PCGSettings.h"


#include "PCGProjectionElement.generated.h"

namespace PCGProjectionConstants
{
	const FName ProjectionTargetLabel = TEXT("Projection Target");
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGProjectionSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Projection")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGProjectionSettings", "NodeTitle", "Projection"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties, PCG_Overridable))
	FPCGProjectionParams ProjectionParams;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug", meta = (PCG_Overridable))
	bool bKeepZeroDensityPoints = false;
#endif

#if WITH_EDITOR
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class FPCGProjectionElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Data/PCGProjectionData.h"
#endif
