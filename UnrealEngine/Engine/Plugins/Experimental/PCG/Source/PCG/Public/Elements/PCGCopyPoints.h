// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"

#include "PCGCopyPoints.generated.h"

namespace PCGCopyPointsConstants
{
	const FName ParamsLabel = TEXT("Params");
	const FName SourcePointsLabel = TEXT("Source");
	const FName TargetPointsLabel = TEXT("Target");
}

UENUM()
enum class EPCGCopyPointsInheritanceMode : uint8
{
	Relative,
	Source,
	Target
};

UENUM()
enum class EPCGCopyPointsMetadataInheritanceMode : uint8
{
	Source,
	Target
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGCopyPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CopyPointsNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** The method used to determine output point rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGCopyPointsInheritanceMode RotationInheritance = EPCGCopyPointsInheritanceMode::Relative;

	/** The method used to determine output point scale */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGCopyPointsInheritanceMode ScaleInheritance = EPCGCopyPointsInheritanceMode::Relative;

	/** The method used to determine output point color */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGCopyPointsInheritanceMode ColorInheritance = EPCGCopyPointsInheritanceMode::Relative;

	/** The method used to determine output seed values. Relative recomputes the seed from the new location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGCopyPointsInheritanceMode SeedInheritance = EPCGCopyPointsInheritanceMode::Relative;

	/** The method used to determine output data attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGCopyPointsMetadataInheritanceMode AttributeInheritance = EPCGCopyPointsMetadataInheritanceMode::Source;
};

class FPCGCopyPointsElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
