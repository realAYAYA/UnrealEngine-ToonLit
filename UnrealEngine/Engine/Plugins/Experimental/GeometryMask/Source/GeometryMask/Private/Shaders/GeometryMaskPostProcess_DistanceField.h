// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "GeometryMaskPostProcess.h"
#include "RenderGraphResources.h"
#include "Templates/SharedPointer.h"

#if WITH_EDITOR
#include "GeometryMaskSettings.h"
#endif

class FRenderTarget;

struct FGeometryMaskPostProcessParameters_DistanceField
{
	FGeometryMaskPostProcessParameters_DistanceField();
	
	TBitArray<TInlineAllocator<4>> bPerChannelCalculateDF;
	TArray<double, TInlineAllocator<4>> PerChannelRadius;
};

/** */
class FGeometryMaskPostProcess_DistanceField
	: public TGeometryMaskPostProcess<FGeometryMaskPostProcessParameters_DistanceField>
	, public TSharedFromThis<FGeometryMaskPostProcess_DistanceField>
{
	using Super = TGeometryMaskPostProcess<FGeometryMaskPostProcessParameters_DistanceField>;
	
public:
	explicit FGeometryMaskPostProcess_DistanceField(const FGeometryMaskPostProcessParameters_DistanceField& InParameters);

	virtual void Execute(FRenderTarget* InTexture) override;

protected:
	virtual void Execute_RenderThread(FRHICommandListImmediate& InRHICmdList, FRenderTarget* InTexture) override;

private:
	/** Cached to selectively resize resources when needed. */
	FIntPoint LastInputSize{ForceInitToZero};

	int32 LastNumActiveChannels = 0;

	TRefCountPtr<FRDGPooledBuffer> StoredInitOutputBuffer;
	TRefCountPtr<FRDGPooledBuffer> StoredStepIntermediateBufferB;
};
