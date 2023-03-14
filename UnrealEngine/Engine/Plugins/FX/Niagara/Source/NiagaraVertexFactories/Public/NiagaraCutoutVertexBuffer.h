// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraCutoutVertexBuffer.h: Niagara cutout uv buffer.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"

/**
 * Vertex buffer to hold cutout UVs. 
 */
class NIAGARAVERTEXFACTORIES_API FNiagaraCutoutVertexBuffer : public FVertexBuffer
{
public:

	FNiagaraCutoutVertexBuffer(int32 ZeroInitCount = 0);

	/**
	 * Initialize the RHI for this rendering resource
	 */
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	FShaderResourceViewRHIRef VertexBufferSRV;

	/** Data to initialize the buffer. */
	TArray<FVector2f> Data;
};

extern NIAGARAVERTEXFACTORIES_API TGlobalResource<FNiagaraCutoutVertexBuffer> GFNiagaraNullCutoutVertexBuffer;
