// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/PCGMetadataCommon.h"


#include "PCGProjectionParams.generated.h"

/** Parameters that control projection behaviour. */
USTRUCT(BlueprintType)
struct PCG_API FPCGProjectionParams
{
	GENERATED_BODY()

	/** Project positions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Apply Data")
	bool bProjectPositions = true;

	/** Project rotations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Apply Data")
	bool bProjectRotations = true;

	/** Project scales. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Apply Data")
	bool bProjectScales = false;

	/** Project colors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Apply Data")
	bool bProjectColors = false;

	/** Attributes to either explicitly exclude or include in the projection operation, depending on the Attribute Mode setting. Leave empty to gather all attributes and their values. Format is comma separated list like: Attribute1,Attribute2 .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Apply Data")
	FString AttributeList = TEXT("");

	/** How the attribute list is used. Exclude Attributes will ignore these attributes and their values on the projection target. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Apply Data")
	EPCGMetadataFilterMode AttributeMode = EPCGMetadataFilterMode::ExcludeAttributes;

	/** Operation to use to combine attributes that reside on both source and target data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Apply Data")
	EPCGMetadataOp AttributeMergeOperation = EPCGMetadataOp::TargetValue;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
