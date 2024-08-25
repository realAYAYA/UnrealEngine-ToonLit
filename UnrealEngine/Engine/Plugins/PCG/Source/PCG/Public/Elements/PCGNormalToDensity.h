// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"

#include "PCGNormalToDensity.generated.h"

UENUM()
enum class PCGNormalToDensityMode : int8
{
	Set,
	Minimum,
	Maximum,
	Add,
	Subtract,
	Multiply,
	Divide
};

/**
 * Finds the angle against the specified direction and applies that to the density
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGNormalToDensitySettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("NormalToDensity")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGNormalToDensitySettings", "NodeTitle", "Normal To Density"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
	

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:

	// The normal to compare against
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Normal = {0, 0.0, 1.0};

	// This is biases the value towards or against the normal (positive or negative)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Offset = 0.0;

	// This applies a curve to scale the result density with Result = Result^(1/Strength)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0001", ClampMax = "100.0"))
	double Strength = 1.0;

	// The operator to apply to the output density 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	PCGNormalToDensityMode DensityMode = PCGNormalToDensityMode::Set;
};

class FPCGNormalToDensityElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

