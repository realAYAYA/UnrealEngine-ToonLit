// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonRenderResources.h"
#include "RenderGraphResources.h"
#include "RHI.h"

namespace NiagaraClearCounts
{
	NIAGARASHADER_API void ClearCountsInt(FRDGBuilder& GraphBuilder, FRDGBufferUAVRef UAV, TConstArrayView<TPair<uint32, int32>> IndexAndValueArray);

	//-TODO:RDG: Deprecated
	NIAGARASHADER_API void ClearCountsInt(FRHICommandList& RHICmdList, FRHIUnorderedAccessView* UAV, TConstArrayView<TPair<uint32, int32>> IndexAndValueArray);
}
