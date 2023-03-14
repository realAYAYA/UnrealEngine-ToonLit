// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIComputeCommandEncoder_hpp
#define MTIComputeCommandEncoder_hpp

#include "imp_ComputeCommandEncoder.hpp"
#include "MTICommandEncoder.hpp"

MTLPP_BEGIN

struct MTIComputeCommandEncoderTrace : public IMPTable<id<MTLComputeCommandEncoder>, MTIComputeCommandEncoderTrace>, public MTIObjectTrace, public MTICommandEncoderTrace
{
	typedef IMPTable<id<MTLComputeCommandEncoder>, MTIComputeCommandEncoderTrace> Super;
	
	MTIComputeCommandEncoderTrace()
	{
	}
	
	MTIComputeCommandEncoderTrace(id<MTLComputeCommandEncoder> C)
	: IMPTable<id<MTLComputeCommandEncoder>, MTIComputeCommandEncoderTrace>(object_getClass(C))
	{
	}
	
	static id<MTLComputeCommandEncoder> Register(id<MTLComputeCommandEncoder> Object);
	
	INTERPOSE_DECLARATION(Setcomputepipelinestate, void, id <MTLComputePipelineState>);
	INTERPOSE_DECLARATION(Setbyteslengthatindex, void, const void *, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(Setbufferoffsetatindex, void,  id <MTLBuffer>, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(SetBufferOffsetatindex, void, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(Setbuffersoffsetswithrange, void, const id <MTLBuffer>  *, const NSUInteger *, NSRange);
	INTERPOSE_DECLARATION(Settextureatindex, void,  id <MTLTexture>, NSUInteger);
	INTERPOSE_DECLARATION(Settextureswithrange, void, const id <MTLTexture>  *, NSRange);
	INTERPOSE_DECLARATION(Setsamplerstateatindex, void,  id <MTLSamplerState>, NSUInteger);
	INTERPOSE_DECLARATION(Setsamplerstateswithrange, void, const id <MTLSamplerState>  *, NSRange);
	INTERPOSE_DECLARATION(Setsamplerstatelodminclamplodmaxclampatindex, void,  id <MTLSamplerState>, float, float, NSUInteger);
	INTERPOSE_DECLARATION(Setsamplerstateslodminclampslodmaxclampswithrange, void, const id <MTLSamplerState>  *, const float *, const float *, NSRange);
	INTERPOSE_DECLARATION(Setthreadgroupmemorylengthatindex, void, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(Setstageinregion, void, MTLPPRegion);
	INTERPOSE_DECLARATION(Dispatchthreadgroupsthreadsperthreadgroup, void, MTLPPSize, MTLPPSize);
	INTERPOSE_DECLARATION(Dispatchthreadgroupswithindirectbufferindirectbufferoffsetthreadsperthreadgroup, void, id <MTLBuffer>, NSUInteger, MTLPPSize);
	INTERPOSE_DECLARATION(Dispatchthreadsthreadsperthreadgroup, void, MTLPPSize, MTLPPSize);
	INTERPOSE_DECLARATION(Updatefence, void, id <MTLFence>);
	INTERPOSE_DECLARATION(Waitforfence, void, id <MTLFence>);
	INTERPOSE_DECLARATION(Useresourceusage, void, id <MTLResource>, MTLResourceUsage);
	INTERPOSE_DECLARATION(Useresourcescountusage, void, const id <MTLResource> *, NSUInteger, MTLResourceUsage);
	INTERPOSE_DECLARATION(Useheap, void, id <MTLHeap>);
	INTERPOSE_DECLARATION(Useheapscount, void, const id <MTLHeap> *, NSUInteger);
	INTERPOSE_DECLARATION(SetImageblockWidthHeight, void, NSUInteger, NSUInteger);
};

MTLPP_END

#endif /* MTIComputeCommandEncoder_hpp */
