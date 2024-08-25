// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGCopyPoints.generated.h"

namespace PCGCopyPointsConstants
{
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
enum class EPCGCopyPointsTagInheritanceMode : uint8
{
	Both,
	Source,
	Target,
};

UENUM()
enum class EPCGCopyPointsMetadataInheritanceMode : uint8
{
	SourceFirst UMETA(Tooltip = "Points will inherit from source metadata and apply only unique attributes from target."),
	TargetFirst UMETA(Tooltip = "Points will inherit from target metadata and apply only unique attributes from source."),
	SourceOnly  UMETA(Tooltip = "Points will inherit metadata only from the source."),
	TargetOnly  UMETA(Tooltip = "Points will inherit metadata only from the target."),
	None        UMETA(Tooltip = "Points will have no metadata.")
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCopyPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CopyPoints")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGCopyPointSettings", "NodeTitle", "Copy Points"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif
	

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** The method used to determine output point rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGCopyPointsInheritanceMode RotationInheritance = EPCGCopyPointsInheritanceMode::Relative;

	/** The method used to determine output point scale */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGCopyPointsInheritanceMode ScaleInheritance = EPCGCopyPointsInheritanceMode::Relative;

	/** The method used to determine output point color */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGCopyPointsInheritanceMode ColorInheritance = EPCGCopyPointsInheritanceMode::Relative;

	/** The method used to determine output seed values. Relative recomputes the seed from the new location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGCopyPointsInheritanceMode SeedInheritance = EPCGCopyPointsInheritanceMode::Relative;

	/** The method used to determine output data attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGCopyPointsMetadataInheritanceMode AttributeInheritance = EPCGCopyPointsMetadataInheritanceMode::SourceFirst;

	/** The method used to determine the output data tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGCopyPointsTagInheritanceMode TagInheritance = EPCGCopyPointsTagInheritanceMode::Both;
};

class FPCGCopyPointsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
