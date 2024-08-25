// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGDensityRemap.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Hidden, meta = (Deprecated = "5.1", DeprecationMessage = "Use DensityNoise instead."))
class UPCGLinearDensityRemapSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGLinearDensityRemapSettings();

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("LinearDensityRemap")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGLinearDensityRemapSettings", "NodeTitle", "Linear Density Remap"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Density; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", ClampMax = "1", PCG_Overridable))
	float RemapMin = 0.5f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", ClampMax = "1", PCG_Overridable))
	float RemapMax = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMultiplyDensity = true;
};

class FPCGLinearDensityRemapElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGNode.h"
#endif
