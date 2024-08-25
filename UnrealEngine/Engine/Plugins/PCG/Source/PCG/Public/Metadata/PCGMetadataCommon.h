// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PCGMetadataCommon.generated.h"

typedef int64 PCGMetadataEntryKey;
typedef int32 PCGMetadataAttributeKey;
typedef int32 PCGMetadataValueKey;

const PCGMetadataEntryKey PCGInvalidEntryKey = -1;
const PCGMetadataValueKey PCGDefaultValueKey = -1;
const PCGMetadataValueKey PCGNotFoundValueKey = -2;

UENUM()
enum class EPCGMetadataOp : uint8
{
	/** Take the minimum value. */
	Min,
	/** Take the maximum value. */
	Max,
	/** Subtract the values. */
	Sub,
	/** Add the values. */
	Add,
	/** Multiply the values. */
	Mul,
	/** Divide the values. */
	Div,
	/** Pick the source (first) value. */
	SourceValue,
	/** Pick the target (second) value. */
	TargetValue
};

UENUM()
enum class EPCGMetadataFilterMode : uint8
{
	/** The listed attributes will be unchanged by the projection and will not be added from the target data. */
	ExcludeAttributes,
	/** Only the listed attributes will be changed by the projection or added from the target data. */
	IncludeAttributes,
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
