// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGSelfPruning.generated.h"

UENUM()
enum class EPCGSelfPruningType : uint8
{
	LargeToSmall,
	SmallToLarge,
	AllEqual,
	None,
	RemoveDuplicates
};

namespace PCGSelfPruningElement
{
	void Execute(FPCGContext* Context, EPCGSelfPruningType PruningType, float RadiusSimilarityFactor, bool bRandomizedPruning);
}

UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGSelfPruningSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGSelfPruningType PruningType = EPCGSelfPruningType::LargeToSmall;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ClampMin=0.0f, EditCondition="PruningType == EPCGSelfPruningType::LargeToSmall || PruningType == EPCGSelfPruningType::SmallToLarge"))
	float RadiusSimilarityFactor = 0.25f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bRandomizedPruning = true;

#if WITH_EDITOR
	//~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SelfPruningNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface
};

class FPCGSelfPruningElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
