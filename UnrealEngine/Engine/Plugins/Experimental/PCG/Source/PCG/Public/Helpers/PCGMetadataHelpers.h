// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class UPCGData;
class UPCGMetadata;

namespace PCGMetadataHelpers
{
	PCG_API bool HasSameRoot(const UPCGMetadata* Metadata1, const UPCGMetadata* Metadata2);
	PCG_API const UPCGMetadata* GetParentMetadata(const UPCGMetadata* Metadata);

	// Helpers functions to cast in spatial/param and return metadata. Nullptr if data doesn't have metadata
	PCG_API const UPCGMetadata* GetConstMetadata(const UPCGData* InData);
	PCG_API UPCGMetadata* GetMutableMetadata(UPCGData* InData);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
