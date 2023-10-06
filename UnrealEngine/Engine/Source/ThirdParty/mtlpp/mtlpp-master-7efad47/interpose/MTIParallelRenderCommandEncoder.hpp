// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIParallelRenderCommandEncoder_hpp
#define MTIParallelRenderCommandEncoder_hpp

#include "imp_ParallelRenderCommandEncoder.hpp"
#include "MTICommandEncoder.hpp"

MTLPP_BEGIN

struct MTIParallelRenderCommandEncoderTrace : public IMPTable<id<MTLParallelRenderCommandEncoder>, MTIParallelRenderCommandEncoderTrace>, public MTIObjectTrace, public MTICommandEncoderTrace
{
	typedef IMPTable<id<MTLParallelRenderCommandEncoder>, MTIParallelRenderCommandEncoderTrace> Super;
	
	MTIParallelRenderCommandEncoderTrace()
	{
	}
	
	MTIParallelRenderCommandEncoderTrace(id<MTLParallelRenderCommandEncoder> C)
	: IMPTable<id<MTLParallelRenderCommandEncoder>, MTIParallelRenderCommandEncoderTrace>(object_getClass(C))
	{
	}
	
	static id<MTLParallelRenderCommandEncoder> Register(id<MTLParallelRenderCommandEncoder> Object);
	
	INTERPOSE_DECLARATION_VOID(RenderCommandEncoder,  id <MTLRenderCommandEncoder>);
	INTERPOSE_DECLARATION(SetcolorstoreactionAtindex,  void, MTLStoreAction,NSUInteger);
	INTERPOSE_DECLARATION(Setdepthstoreaction,  void, MTLStoreAction);
	INTERPOSE_DECLARATION(Setstencilstoreaction,  void, MTLStoreAction);
	INTERPOSE_DECLARATION(SetcolorstoreactionoptionsAtindex,  void, MTLStoreActionOptions,NSUInteger);
	INTERPOSE_DECLARATION(Setdepthstoreactionoptions,  void, MTLStoreActionOptions);
	INTERPOSE_DECLARATION(Setstencilstoreactionoptions,  void, MTLStoreActionOptions);
};

MTLPP_END

#endif /* MTIParallelRenderCommandEncoder_hpp */
