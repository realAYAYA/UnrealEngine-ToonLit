// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"
#include "RenderingThread.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "SlateElementVertexBuffer.h"

/**
 * Represents a per instance data buffer for a custom Slate mesh element
 */
class FSlateUpdatableInstanceBuffer final : public ISlateUpdatableInstanceBuffer
{
	// Owned by the render thread
	struct FRenderProxy final : public ISlateUpdatableInstanceBufferRenderProxy
	{
		TSlateElementVertexBuffer<FVector4> InstanceBufferResource;

		virtual ~FRenderProxy()
		{
			InstanceBufferResource.Destroy();
		}

		void Update(FRHICommandListImmediate& RHICmdList, FSlateInstanceBufferData& Data);

		virtual void BindStreamSource(FRHICommandList& RHICmdList, int32 StreamIndex, uint32 InstanceOffset) override final;
	} *Proxy;

public:
	FSlateUpdatableInstanceBuffer(int32 InstanceCount);
	~FSlateUpdatableInstanceBuffer();

private:
	// BEGIN ISlateUpdatableInstanceBuffer
	virtual uint32 GetNumInstances() const override final { return NumInstances; }
	virtual ISlateUpdatableInstanceBufferRenderProxy* GetRenderProxy() const override final { return Proxy; }
	virtual void Update(FSlateInstanceBufferData& Data) override final;
	// END ISlateUpdatableInstanceBuffer


private:
	uint32 NumInstances;
};
