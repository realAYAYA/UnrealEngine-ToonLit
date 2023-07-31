// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"

#include "PCGInputOutputSettings.generated.h"

namespace PCGInputOutputConstants
{
	const FName DefaultInputLabel = TEXT("Input");
	const FName DefaultActorLabel = TEXT("Actor");
	const FName DefaultOriginalActorLabel = TEXT("OriginalActor");
	const FName DefaultLandscapeLabel = TEXT("Landscape");
	const FName DefaultLandscapeHeightLabel = TEXT("Landscape Height");
	const FName DefaultExcludedActorsLabel = TEXT("ExcludedActors");
}

class PCG_API FPCGInputOutputElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

UCLASS(NotBlueprintable, Hidden, ClassGroup = (Procedural))
class PCG_API UPCGGraphInputOutputSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGGraphInputOutputSettings(const FObjectInitializer& ObjectInitializer);

	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return bIsInput ? FName(TEXT("InputNode")) : FName(TEXT("OutputNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::InputOutput; }
#endif

	TArray<FPCGPinProperties> InputPinProperties() const override;
	TArray<FPCGPinProperties> OutputPinProperties() const override;

	void SetInput(bool bInIsInput) { bIsInput = bInIsInput; }

	bool IsPinAdvanced(const UPCGPin* Pin) const;
	void SetShowAdvancedPins(bool bValue);

	void AddCustomPin(const FPCGPinProperties& NewCustomPinProperties);

protected:
	virtual FPCGElementPtr CreateElement() const override { return MakeShared<FPCGInputOutputElement>(); }
	// ~End UPCGSettings interface

	const TArray<FName>& StaticLabels() const { return bIsInput ? StaticInLabels : StaticOutLabels; }
	const TArray<FName>& StaticAdvancedLabels() const { return bIsInput ? StaticAdvancedInLabels : StaticAdvancedOutLabels; }

	TArray<FPCGPinProperties> GetPinProperties() const;

protected:
	UPROPERTY()
	TSet<FName> PinLabels_DEPRECATED;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Input")
	TArray<FPCGPinProperties> CustomPins;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Input")
	bool bShowAdvancedPins = false;

	TArray<FName> StaticInLabels;
	TArray<FName> StaticAdvancedInLabels;
	TArray<FName> StaticOutLabels;
	TArray<FName> StaticAdvancedOutLabels;

	bool bIsInput = false;
};