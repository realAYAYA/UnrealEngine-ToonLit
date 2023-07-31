// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGNode.h"
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
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("DensityNoiseNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Density; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Density = (OriginalDensity op DensityNoise), DensityNoise in [DensityNoiseMin, DensityNoiseMax] */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGDensityNoiseMode DensityMode = EPCGDensityNoiseMode::Set;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float DensityNoiseMin = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float DensityNoiseMax = 1.f;

	/** Density = 1 - Density before applying the DensityMode operation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bInvertSourceDensity = false;
};

class FPCGDensityNoiseElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;
};
