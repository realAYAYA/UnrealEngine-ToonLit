// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIRenderCommandEncoder_hpp
#define MTIRenderCommandEncoder_hpp

#include "imp_RenderCommandEncoder.hpp"
#include "MTICommandEncoder.hpp"

MTLPP_BEGIN

struct MTIRenderCommandEncoderTrace : public IMPTable<id<MTLRenderCommandEncoder>, MTIRenderCommandEncoderTrace>, public MTIObjectTrace, public MTICommandEncoderTrace
{
	typedef IMPTable<id<MTLRenderCommandEncoder>, MTIRenderCommandEncoderTrace> Super;
	
	MTIRenderCommandEncoderTrace()
	{
	}
	
	MTIRenderCommandEncoderTrace(id<MTLRenderCommandEncoder> C)
	: IMPTable<id<MTLRenderCommandEncoder>, MTIRenderCommandEncoderTrace>(object_getClass(C))
	{
	}
	
	static id<MTLRenderCommandEncoder> Register(id<MTLRenderCommandEncoder> Object);
	
	INTERPOSE_DECLARATION(Setrenderpipelinestate,  void, id <MTLRenderPipelineState>);
	INTERPOSE_DECLARATION(SetvertexbytesLengthAtindex,  void, const void *,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetvertexbufferOffsetAtindex,  void,id <MTLBuffer>,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetvertexbufferoffsetAtindex,  void, NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetvertexbuffersOffsetsWithrange,  void, const id <MTLBuffer> *,const NSUInteger *,NSRange);
	INTERPOSE_DECLARATION(SetvertextextureAtindex,  void,id <MTLTexture>,NSUInteger);
	INTERPOSE_DECLARATION(SetvertextexturesWithrange,  void, const id <MTLTexture> *,NSRange);
	INTERPOSE_DECLARATION(SetvertexsamplerstateAtindex,  void,id <MTLSamplerState>,NSUInteger);
	INTERPOSE_DECLARATION(SetvertexsamplerstatesWithrange,  void, const id <MTLSamplerState> *,NSRange);
	INTERPOSE_DECLARATION(SetvertexsamplerstateLodminclampLodmaxclampAtindex,  void,id <MTLSamplerState>,float,float,NSUInteger);
	INTERPOSE_DECLARATION(SetvertexsamplerstatesLodminclampsLodmaxclampsWithrange,  void, const id <MTLSamplerState> *,const float *,const float *,NSRange);
	INTERPOSE_DECLARATION(Setviewport,  void, MTLPPViewport);
	INTERPOSE_DECLARATION(SetviewportsCount,  void, const MTLPPViewport *,NSUInteger);
	INTERPOSE_DECLARATION(Setfrontfacingwinding,  void, MTLWinding);
	INTERPOSE_DECLARATION(Setcullmode,  void, MTLCullMode);
	INTERPOSE_DECLARATION(Setdepthclipmode,  void, MTLDepthClipMode);
	INTERPOSE_DECLARATION(SetdepthbiasSlopescaleClamp,  void, float,float,float);
	INTERPOSE_DECLARATION(Setscissorrect,  void, MTLPPScissorRect);
	INTERPOSE_DECLARATION(SetscissorrectsCount,  void, const MTLPPScissorRect *,NSUInteger);
	INTERPOSE_DECLARATION(Settrianglefillmode,  void, MTLTriangleFillMode);
	INTERPOSE_DECLARATION(SetfragmentbytesLengthAtindex,  void, const void *,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetfragmentbufferOffsetAtindex,  void,id <MTLBuffer>,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetfragmentbufferoffsetAtindex,  void, NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetfragmentbuffersOffsetsWithrange,  void, const id <MTLBuffer> *,const NSUInteger *,NSRange);
	INTERPOSE_DECLARATION(SetfragmenttextureAtindex,  void,id <MTLTexture>,NSUInteger);
	INTERPOSE_DECLARATION(SetfragmenttexturesWithrange,  void, const id <MTLTexture> *,NSRange);
	INTERPOSE_DECLARATION(SetfragmentsamplerstateAtindex,  void,id <MTLSamplerState>,NSUInteger);
	INTERPOSE_DECLARATION(SetfragmentsamplerstatesWithrange,  void, const id <MTLSamplerState> *,NSRange);
	INTERPOSE_DECLARATION(SetfragmentsamplerstateLodminclampLodmaxclampAtindex,  void,id <MTLSamplerState>,float,float,NSUInteger);
	INTERPOSE_DECLARATION(SetfragmentsamplerstatesLodminclampsLodmaxclampsWithrange,  void, const id <MTLSamplerState> *,const float *,const float *,NSRange);
	INTERPOSE_DECLARATION(SetblendcolorredGreenBlueAlpha,  void, float,float,float,float);
	INTERPOSE_DECLARATION(Setdepthstencilstate,  void,id <MTLDepthStencilState>);
	INTERPOSE_DECLARATION(Setstencilreferencevalue,  void, uint32_t);
	INTERPOSE_DECLARATION(SetstencilfrontreferencevalueBackreferencevalue,  void, uint32_t,uint32_t);
	INTERPOSE_DECLARATION(SetvisibilityresultmodeOffset,  void, MTLVisibilityResultMode,NSUInteger);
	INTERPOSE_DECLARATION(SetcolorstoreactionAtindex,  void, MTLStoreAction,NSUInteger);
	INTERPOSE_DECLARATION(Setdepthstoreaction,  void, MTLStoreAction);
	INTERPOSE_DECLARATION(Setstencilstoreaction,  void, MTLStoreAction);
	INTERPOSE_DECLARATION(SetcolorstoreactionoptionsAtindex,  void, MTLStoreActionOptions,NSUInteger);
	INTERPOSE_DECLARATION(Setdepthstoreactionoptions,  void, MTLStoreActionOptions);
	INTERPOSE_DECLARATION(Setstencilstoreactionoptions,  void, MTLStoreActionOptions);
	INTERPOSE_DECLARATION(DrawprimitivesVertexstartVertexcountInstancecount,  void, MTLPrimitiveType,NSUInteger,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(DrawprimitivesVertexstartVertexcount,  void, MTLPrimitiveType,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecount,  void, MTLPrimitiveType,NSUInteger,MTLIndexType,id <MTLBuffer>,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffset,  void, MTLPrimitiveType,NSUInteger,MTLIndexType,id <MTLBuffer>,NSUInteger);
	INTERPOSE_DECLARATION(DrawprimitivesVertexstartVertexcountInstancecountBaseinstance,  void, MTLPrimitiveType,NSUInteger,NSUInteger,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(DrawindexedprimitivesIndexcountIndextypeIndexbufferIndexbufferoffsetInstancecountBasevertexBaseinstance,  void, MTLPrimitiveType,NSUInteger,MTLIndexType,id <MTLBuffer>,NSUInteger,NSUInteger,NSInteger,NSUInteger);
	INTERPOSE_DECLARATION(DrawprimitivesIndirectbufferIndirectbufferoffset,  void, MTLPrimitiveType,id <MTLBuffer>,NSUInteger);
	INTERPOSE_DECLARATION(DrawindexedprimitivesIndextypeIndexbufferIndexbufferoffsetIndirectbufferIndirectbufferoffset,  void, MTLPrimitiveType,MTLIndexType,id <MTLBuffer>,NSUInteger,id <MTLBuffer>,NSUInteger);
	INTERPOSE_DECLARATION_VOID(Texturebarrier,  void);
	INTERPOSE_DECLARATION(UpdatefenceAfterstages,  void, id <MTLFence>,MTLRenderStages);
	INTERPOSE_DECLARATION(WaitforfenceBeforestages,  void, id <MTLFence>,MTLRenderStages);
	INTERPOSE_DECLARATION(SettessellationfactorbufferOffsetInstancestride,  void,id <MTLBuffer>,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(Settessellationfactorscale,  void, float);
	INTERPOSE_DECLARATION(DrawpatchesPatchstartPatchcountPatchindexbufferPatchindexbufferoffsetInstancecountBaseinstance,  void, NSUInteger,NSUInteger,NSUInteger, id <MTLBuffer>,NSUInteger,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(DrawpatchesPatchindexbufferPatchindexbufferoffsetIndirectbufferIndirectbufferoffset,  void, NSUInteger, id <MTLBuffer>,NSUInteger,id <MTLBuffer>,NSUInteger);
	INTERPOSE_DECLARATION(DrawindexedpatchesPatchstartPatchcountPatchindexbufferPatchindexbufferoffsetControlpointindexbufferControlpointindexbufferoffsetInstancecountBaseinstance,  void, NSUInteger,NSUInteger,NSUInteger, id <MTLBuffer>,NSUInteger,id <MTLBuffer>,NSUInteger,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(DrawindexedpatchesPatchindexbufferPatchindexbufferoffsetControlpointindexbufferControlpointindexbufferoffsetIndirectbufferIndirectbufferoffset,  void, NSUInteger, id <MTLBuffer>,NSUInteger,id <MTLBuffer>,NSUInteger,id <MTLBuffer>,NSUInteger);
	INTERPOSE_DECLARATION(UseresourceUsage,  void, id <MTLResource>,MTLResourceUsage);
	INTERPOSE_DECLARATION(UseresourcesCountUsage,  void, const id <MTLResource> *,NSUInteger,MTLResourceUsage);
	INTERPOSE_DECLARATION(Useheap,  void, id <MTLHeap>);
	INTERPOSE_DECLARATION(UseheapsCount,  void, const id <MTLHeap> *, NSUInteger);
	
	INTERPOSE_DECLARATION(SetTilebytesLengthAtindex,	void, const void *,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetTilebufferOffsetAtindex,	void,id <MTLBuffer>,NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetTilebufferoffsetAtindex,	void, NSUInteger,NSUInteger);
	INTERPOSE_DECLARATION(SetTilebuffersOffsetsWithrange,	void, const id <MTLBuffer> *,const NSUInteger *,NSRange);
	INTERPOSE_DECLARATION(SetTiletextureAtindex,	void,id <MTLTexture>,NSUInteger);
	INTERPOSE_DECLARATION(SetTiletexturesWithrange,	void, const id <MTLTexture> *,NSRange);
	INTERPOSE_DECLARATION(SetTilesamplerstateAtindex,	void,id <MTLSamplerState>,NSUInteger);
	INTERPOSE_DECLARATION(SetTilesamplerstatesWithrange,	void, const id <MTLSamplerState> *,NSRange);
	INTERPOSE_DECLARATION(SetTilesamplerstateLodminclampLodmaxclampAtindex,	void,id <MTLSamplerState>,float,float,NSUInteger);
	INTERPOSE_DECLARATION(SetTilesamplerstatesLodminclampsLodmaxclampsWithrange,	void, const id <MTLSamplerState> *,const float *,const float *,NSRange);
	INTERPOSE_DECLARATION(dispatchThreadsPerTile,	void, MTLPPSize);
	INTERPOSE_DECLARATION(setThreadgroupMemoryLength,	void, NSUInteger, NSUInteger, NSUInteger);
};

MTLPP_END

#endif /* MTIRenderCommandEncoder_hpp */
