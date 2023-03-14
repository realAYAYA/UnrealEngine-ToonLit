// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGMetadataCommon.generated.h"

typedef int64 PCGMetadataEntryKey;
typedef int32 PCGMetadataAttributeKey;
typedef int32 PCGMetadataValueKey;

const PCGMetadataEntryKey PCGInvalidEntryKey = -1;
const PCGMetadataValueKey PCGDefaultValueKey = -1;

UENUM()
enum class EPCGMetadataOp : uint8
{
	Min,
	Max,
	Sub,
	Add,
	Mul,
	Div
};