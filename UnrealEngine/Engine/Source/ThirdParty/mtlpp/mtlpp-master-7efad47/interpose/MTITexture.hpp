// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTITexture_hpp
#define MTITexture_hpp

#include "imp_Texture.hpp"
#include "MTIObject.hpp"
#include "MTIResource.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

struct MTITextureTrace : public IMPTable<id<MTLTexture>, MTITextureTrace>, public MTIObjectTrace, public MTIResourceTrace
{
	typedef IMPTable<id<MTLTexture>, MTITextureTrace> Super;
	
	MTITextureTrace()
	{
	}
	
	MTITextureTrace(id<MTLTexture> C)
	: IMPTable<id<MTLTexture>, MTITextureTrace>(object_getClass(C))
	{
	}
	
	static id<MTLTexture> Register(id<MTLTexture> Texture);
	
	INTERPOSE_DECLARATION_VOID(Rootresource, id <MTLResource>);
	INTERPOSE_DECLARATION_VOID(Parenttexture, id <MTLTexture>);
	INTERPOSE_DECLARATION_VOID(Parentrelativelevel, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Parentrelativeslice, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Buffer, id <MTLBuffer>);
	INTERPOSE_DECLARATION_VOID(Bufferoffset, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Bufferbytesperrow, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Iosurface, IOSurfaceRef);
	INTERPOSE_DECLARATION_VOID(Iosurfaceplane, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Texturetype, MTLTextureType);
	INTERPOSE_DECLARATION_VOID(Pixelformat, MTLPixelFormat);
	INTERPOSE_DECLARATION_VOID(Width, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Height, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Depth, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Mipmaplevelcount, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Samplecount, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Arraylength, NSUInteger);
	INTERPOSE_DECLARATION_VOID(Usage, MTLTextureUsage);
	INTERPOSE_DECLARATION_VOID(Isframebufferonly, BOOL);
	INTERPOSE_DECLARATION(Getbytesbytesperrowbytesperimagefromregionmipmaplevelslice, void, void * , NSUInteger , NSUInteger , MTLPPRegion , NSUInteger , NSUInteger );
	INTERPOSE_DECLARATION(Replaceregionmipmaplevelslicewithbytesbytesperrowbytesperimage, void, MTLPPRegion , NSUInteger , NSUInteger , const void * , NSUInteger , NSUInteger );
	INTERPOSE_DECLARATION(Getbytesbytesperrowfromregionmipmaplevel, void, void * , NSUInteger , MTLPPRegion , NSUInteger );
	INTERPOSE_DECLARATION(Replaceregionmipmaplevelwithbytesbytesperrow, void, MTLPPRegion , NSUInteger , const void * , NSUInteger );
	INTERPOSE_DECLARATION(Newtextureviewwithpixelformat, id<MTLTexture>, MTLPixelFormat );
	INTERPOSE_DECLARATION(Newtextureviewwithpixelformattexturetypelevelsslices, id<MTLTexture>, MTLPixelFormat , MTLTextureType , NSRange , NSRange );
};

struct MTITraceNewTextureDescHandler : public MTITraceCommandHandler
{
	MTITraceNewTextureDescHandler();
	
	MTLTextureDescriptor* Trace(MTLTextureDescriptor* Desc);
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs);
};
extern MTITraceNewTextureDescHandler GMTITraceNewTextureDescHandler;

MTLPP_END

#endif /* MTITexture_hpp */
