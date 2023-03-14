// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIRenderPipeline_hpp
#define MTIRenderPipeline_hpp

#include "imp_RenderPipeline.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIRenderPipelineStateTrace : public IMPTable<id<MTLRenderPipelineState>, MTIRenderPipelineStateTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLRenderPipelineState>, MTIRenderPipelineStateTrace> Super;
	
	MTIRenderPipelineStateTrace()
	{
	}
	
	MTIRenderPipelineStateTrace(id<MTLRenderPipelineState> C)
	: IMPTable<id<MTLRenderPipelineState>, MTIRenderPipelineStateTrace>(object_getClass(C))
	{
	}
	
	static id<MTLRenderPipelineState> Register(id<MTLRenderPipelineState> RenderPipelineState);
};

MTLPP_END

#endif /* MTIRenderPipeline_hpp */
