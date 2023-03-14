// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "PCGNode.h"

#include "PCGDensityRemap.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Hidden, meta = (Deprecated = "5.1", DeprecationMessage = "Use DensityNoise instead."))
class PCG_API UPCGLinearDensityRemapSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGLinearDensityRemapSettings();

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("LinearDensityRemap")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Density; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", ClampMax = "1"))
	float RemapMin = 0.5f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", ClampMax = "1"))
	float RemapMax = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bMultiplyDensity = true;
};

class FPCGLinearDensityRemapElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;
};
