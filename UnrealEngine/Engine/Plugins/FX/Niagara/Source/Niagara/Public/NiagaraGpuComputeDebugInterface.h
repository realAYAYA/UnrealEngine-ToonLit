// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
Class used help realtime debug Gpu Compute simulations
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraRenderGraphUtils.h"
#include "RenderGraphDefinitions.h"

#if NIAGARA_COMPUTEDEBUG_ENABLED

class FNiagaraGpuComputeDebug;

class NIAGARA_API FNiagaraGpuComputeDebugInterface
{
	friend class FNiagaraGpuComputeDispatchInterface;

private:
	explicit FNiagaraGpuComputeDebugInterface(FNiagaraGpuComputeDebug* InComputeDebug)
		: ComputeDebug(InComputeDebug)
	{
	}

public:
	// Add a texture to visualize
	void AddTexture(FRDGBuilder& GraphBuilder, FNiagaraSystemInstanceID SystemInstanceID, FName SourceName, FRDGTextureRef Texture, FVector2D PreviewDisplayRange = FVector2D::ZeroVector);

	// Add a texture to visualize that contains a number of attributes and select which attributes to push into RGBA where -1 means ignore that channel
	// The first -1 in the attribute indices list will also limit the number of attributes we attempt to read.
	// NumTextureAttributes in this version is meant for a 2D atlas
	void AddAttributeTexture(FRDGBuilder& GraphBuilder, FNiagaraSystemInstanceID SystemInstanceID, FName SourceName, FRDGTextureRef Texture, FIntPoint NumTextureAttributes, FIntVector4 AttributeIndices, FVector2D PreviewDisplayRange = FVector2D::ZeroVector);

	// Add a texture to visualize that contains a number of attributes and select which attributes to push into RGBA where -1 means ignore that channel
	// The first -1 in the attribute indices list will also limit the number of attributes we attempt to read
	// NumTextureAttributes in this version is meant for a 3D atlas
	void AddAttributeTexture(FRDGBuilder& GraphBuilder, FNiagaraSystemInstanceID SystemInstanceID, FName SourceName, FRDGTextureRef Texture, FIntVector4 NumTextureAttributes, FIntVector4 AttributeIndices, FVector2D PreviewDisplayRange = FVector2D::ZeroVector);

private:
	FNiagaraGpuComputeDebug* ComputeDebug = nullptr;
};

#endif //NIAGARA_COMPUTEDEBUG_ENABLED
