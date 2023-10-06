// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ShaderParameterMetadataAllocation.h"
#include "ShaderParameterMetadata.h"

FShaderParametersMetadataAllocations::~FShaderParametersMetadataAllocations()
{
	for (FShaderParametersMetadata* ShaderParameterMetadata : ShaderParameterMetadatas)
	{
		delete ShaderParameterMetadata;
	}
}
