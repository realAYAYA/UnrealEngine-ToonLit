// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "Data/PCGDifferenceData.h"

#include "PCGDifferenceElement.generated.h"

namespace PCGDifferenceConstants
{
	const FName SourceLabel = TEXT("Source");
	const FName DifferencesLabel = TEXT("Differences");
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGDifferenceSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("DifferenceNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGDifferenceDensityFunction DensityFunction = EPCGDifferenceDensityFunction::Minimum;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGDifferenceMode Mode = EPCGDifferenceMode::Inferred;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDiffMetadata = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, BlueprintReadWrite, EditAnywhere, Category = Debug)
	bool bKeepZeroDensityPoints = false;
#endif
};

class FPCGDifferenceElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;

	void LabellessProcessing(FPCGContext* Context) const;
};
