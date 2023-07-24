// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPin.h"
#include "PCGSettings.h"

#include "Elements/PCGPointProcessingElementBase.h"

#include "PCGDensityNoise.generated.h"

UENUM()
enum class EPCGDensityNoiseMode : uint8
{
	Set,
	Minimum,
	Maximum,
	Add,
	Multiply
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGDensityNoiseSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGDensityNoiseSettings();

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("DensityNoise")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGDensityNodeSettings", "NodeTitle", "Density Noise"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Density; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Density = (OriginalDensity op DensityNoise), DensityNoise in [DensityNoiseMin, DensityNoiseMax] */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGDensityNoiseMode DensityMode = EPCGDensityNoiseMode::Set;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float DensityNoiseMin = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float DensityNoiseMax = 1.f;

	/** Density = 1 - Density before applying the DensityMode operation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInvertSourceDensity = false;
};

class FPCGDensityNoiseElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGNode.h"
#endif
