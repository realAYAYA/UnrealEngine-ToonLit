// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ChunkedArray.h"

class FShaderParametersMetadata;

/** 
 * When we build FShaderParametersMetadata at runtime it can require that we allocate
 * additional data referenced but not owned by the FShaderParametersMetadata.
 * This structure tracks the allocations, and should be released only after we are
 * done with the associated FShaderParametersMetadata.
 */
struct FShaderParametersMetadataAllocations
{
	/** Allocated metadata. Should include the parent metadata allocation. */
	TArray<FShaderParametersMetadata*> ShaderParameterMetadatas;
	/** Allocated name dictionary. */
	TChunkedArray<FString, 512> Names;

	FShaderParametersMetadataAllocations() = default;
	FShaderParametersMetadataAllocations(FShaderParametersMetadataAllocations& Other) = delete;
	~FShaderParametersMetadataAllocations();
};
