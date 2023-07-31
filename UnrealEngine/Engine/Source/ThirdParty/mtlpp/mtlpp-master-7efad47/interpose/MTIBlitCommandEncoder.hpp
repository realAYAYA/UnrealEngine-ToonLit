// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIBlitCommandEncoder_hpp
#define MTIBlitCommandEncoder_hpp

#include "imp_BlitCommandEncoder.hpp"
#include "MTICommandEncoder.hpp"

MTLPP_BEGIN

struct MTIBlitCommandEncoderTrace : public IMPTable<id<MTLBlitCommandEncoder>, MTIBlitCommandEncoderTrace>, public MTIObjectTrace, public MTICommandEncoderTrace
{
	typedef IMPTable<id<MTLBlitCommandEncoder>, MTIBlitCommandEncoderTrace> Super;
	
	MTIBlitCommandEncoderTrace()
	{
	}
	
	MTIBlitCommandEncoderTrace(id<MTLBlitCommandEncoder> C)
	: IMPTable<id<MTLBlitCommandEncoder>, MTIBlitCommandEncoderTrace>(object_getClass(C))
	{
	}
	
	static id<MTLBlitCommandEncoder> Register(id<MTLBlitCommandEncoder> Object);
	
	INTERPOSE_DECLARATION(SynchronizeResource, void, id<MTLResource>);
	INTERPOSE_DECLARATION(SynchronizeTextureSliceLevel, void, id<MTLTexture>, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(CopyFromTexturesourceSlicesourceLevelsourceOriginsourceSizetoTexturedestinationSlicedestinationLeveldestinationOrigin, void, id<MTLTexture>, NSUInteger, NSUInteger, MTLPPOrigin, MTLPPSize, id<MTLTexture>, NSUInteger, NSUInteger, MTLPPOrigin);
	INTERPOSE_DECLARATION(CopyFromBuffersourceOffsetsourceBytesPerRowsourceBytesPerImagesourceSizetoTexturedestinationSlicedestinationLeveldestinationOrigin, void, id<MTLBuffer>, NSUInteger, NSUInteger, NSUInteger, MTLPPSize, id<MTLTexture>, NSUInteger, NSUInteger, MTLPPOrigin);
	INTERPOSE_DECLARATION(CopyFromBuffersourceOffsetsourceBytesPerRowsourceBytesPerImagesourceSizetoTexturedestinationSlicedestinationLeveldestinationOriginoptions, void, id<MTLBuffer>, NSUInteger, NSUInteger, NSUInteger, MTLPPSize, id<MTLTexture>, NSUInteger, NSUInteger, MTLPPOrigin, MTLBlitOption);
	INTERPOSE_DECLARATION(CopyFromTexturesourceSlicesourceLevelsourceOriginsourceSizetoBufferdestinationOffsetdestinationBytesPerRowdestinationBytesPerImage, void, id<MTLTexture>, NSUInteger, NSUInteger, MTLPPOrigin, MTLPPSize, id<MTLBuffer>, NSUInteger, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(CopyFromTexturesourceSlicesourceLevelsourceOriginsourceSizetoBufferdestinationOffsetdestinationBytesPerRowdestinationBytesPerImageoptions, void, id<MTLTexture>, NSUInteger, NSUInteger, MTLPPOrigin, MTLPPSize, id<MTLBuffer>, NSUInteger, NSUInteger, NSUInteger, MTLBlitOption);
	INTERPOSE_DECLARATION(GenerateMipmapsForTexture, void, id<MTLTexture>);
	INTERPOSE_DECLARATION(FillBufferRangeValue, void, id <MTLBuffer>, NSRange, uint8_t);
	INTERPOSE_DECLARATION(CopyFromBufferSourceOffsetToBufferDestinationOffsetSize, void, id <MTLBuffer>, NSUInteger, id <MTLBuffer>, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(UpdateFence, void, id <MTLFence>);
	INTERPOSE_DECLARATION(WaitForFence, void, id <MTLFence>);
};

MTLPP_END

#endif /* MTIBlitCommandEncoder_hpp */
