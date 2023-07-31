// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIArgumentEncoder_hpp
#define MTIArgumentEncoder_hpp

#include "imp_ArgumentEncoder.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIArgumentEncoderTrace : public IMPTable<id<MTLArgumentEncoder>, MTIArgumentEncoderTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLArgumentEncoder>, MTIArgumentEncoderTrace> Super;
	
	MTIArgumentEncoderTrace()
	{
	}
	
	MTIArgumentEncoderTrace(id<MTLArgumentEncoder> C)
	: IMPTable<id<MTLArgumentEncoder>, MTIArgumentEncoderTrace>(object_getClass(C))
	{
	}
	
	static id<MTLArgumentEncoder> Register(id<MTLArgumentEncoder> Object);
	
	INTERPOSE_DECLARATION_VOID(Device, id<MTLDevice>);
	INTERPOSE_DECLARATION_VOID(Label, NSString*);
	INTERPOSE_DECLARATION(SetLabel, void, NSString*);
	INTERPOSE_DECLARATION_VOID(EncodedLength, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Alignment, NSUInteger);
	INTERPOSE_DECLARATION(Setargumentbufferoffset, void, id <MTLBuffer>, NSUInteger);
	INTERPOSE_DECLARATION(Setargumentbufferstartoffsetarrayelement, void, id<MTLBuffer>, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(Setbufferoffsetatindex, void,  id <MTLBuffer>, NSUInteger, NSUInteger);
	INTERPOSE_DECLARATION(Setbuffersoffsetswithrange, void, const id <MTLBuffer>  *, const NSUInteger *, NSRange);
	INTERPOSE_DECLARATION(Settextureatindex, void,  id <MTLTexture>, NSUInteger);
	INTERPOSE_DECLARATION(Settextureswithrange, void, const id <MTLTexture>  *, NSRange);
	INTERPOSE_DECLARATION(Setsamplerstateatindex, void,  id <MTLSamplerState>, NSUInteger);
	INTERPOSE_DECLARATION(Setsamplerstateswithrange, void, const id <MTLSamplerState>  *, NSRange);
	INTERPOSE_DECLARATION(ConstantDataAtIndex, void*, NSUInteger);
	INTERPOSE_DECLARATION(NewArgumentEncoderForBufferAtIndex, id<MTLArgumentEncoder>, NSUInteger);
};

MTLPP_END

#endif /* MTIArgumentEncoder_hpp */
