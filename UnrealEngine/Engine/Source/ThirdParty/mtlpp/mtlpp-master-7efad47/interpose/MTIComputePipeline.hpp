// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIComputePipeline_hpp
#define MTIComputePipeline_hpp

#include "imp_ComputePipeline.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIComputePipelineStateTrace : public IMPTable<id<MTLComputePipelineState>, MTIComputePipelineStateTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLComputePipelineState>, MTIComputePipelineStateTrace> Super;
	
	MTIComputePipelineStateTrace()
	{
	}
	
	MTIComputePipelineStateTrace(id<MTLComputePipelineState> C)
	: IMPTable<id<MTLComputePipelineState>, MTIComputePipelineStateTrace>(object_getClass(C))
	{
	}
	
	static id<MTLComputePipelineState> Register(id<MTLComputePipelineState> ComputePipelineState);
};

MTLPP_END

#endif /* MTIComputePipeline_hpp */
