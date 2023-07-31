// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGSurfaceSampler.generated.h"

class FPCGSurfaceSamplerElement;

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSurfaceSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGSurfaceSamplerSettings();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ClampMin="0"))
	float PointsPerSquaredMeter = 0.1f;

	UPROPERTY()
	float PointRadius_DEPRECATED = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FVector PointExtents = FVector(100.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ClampMin="0"))
	float Looseness = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Points")
	bool bApplyDensityToPoints = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Points", meta=(ClampMin="0", ClampMax="1"))
	float PointSteepness = 0.5f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, BlueprintReadWrite, EditAnywhere, Category = "Debug")
	bool bKeepZeroDensityPoints = false;
#endif

	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

#if WITH_EDITOR
	//~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SurfaceSamplerNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface
};

class FPCGSurfaceSamplerElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
