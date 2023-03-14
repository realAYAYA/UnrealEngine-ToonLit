// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIDrawable_hpp
#define MTIDrawable_hpp

#include "imp_Drawable.hpp"
#include "MTIObject.hpp"
#import <QuartzCore/CAMetalLayer.h>

MTLPP_BEGIN

struct MTIDrawableTrace : public IMPTable<id<MTLDrawable>, MTIDrawableTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLDrawable>, MTIDrawableTrace> Super;
	
	MTIDrawableTrace()
	{
	}
	
	MTIDrawableTrace(id<MTLDrawable> C)
	: IMPTable<id<MTLDrawable>, MTIDrawableTrace>(object_getClass(C))
	{
	}
	
	static id<MTLDrawable> Register(id<MTLDrawable> Object);
	
	static id <MTLTexture> TextureImpl(id Obj, SEL Cmd, Super::TextureType::DefinedIMP Original);
	static CAMetalLayer* LayerImpl(id Obj, SEL Cmd, Super::LayerType::DefinedIMP Original);
	
	static void PresentImpl(id Obj, SEL Cmd, Super::PresentType::DefinedIMP Original);
	static void PresentAtTimeImpl(id Obj, SEL Cmd, Super::PresentAtTimeType::DefinedIMP Original, CFTimeInterval Time);
	
	INTERPOSE_DECLARATION(PresentAfterMinimumDuration, void, CFTimeInterval);
	INTERPOSE_DECLARATION(AddPresentedHandler, void, MTLDrawablePresentedHandler);
};

struct MTILayerTrace : public IMPTable<CAMetalLayer*, MTILayerTrace>, public MTIObjectTrace
{
	typedef IMPTable<CAMetalLayer*, MTIDrawableTrace> Super;
	
	MTILayerTrace()
	{
	}
	
	MTILayerTrace(Class C)
	: IMPTable<CAMetalLayer*, MTILayerTrace>(C)
	{
	}
	
	INTERPOSE_DECLARATION_VOID(init, id);
	INTERPOSE_DECLARATION(initWithLayer, id, id);
	INTERPOSE_DECLARATION(Setdevice, void, id <MTLDevice>);
	INTERPOSE_DECLARATION(SetpixelFormat, void, MTLPixelFormat);
	INTERPOSE_DECLARATION(SetframebufferOnly, void, BOOL);
	INTERPOSE_DECLARATION(SetddrawableSize, void, CGSize);
	INTERPOSE_DECLARATION_VOID(NextDrawable, id <CAMetalDrawable>);
	INTERPOSE_DECLARATION(SetmaximumDrawableCount, void, NSUInteger);
	INTERPOSE_DECLARATION(SetpresentsWithTransaction, void, BOOL);
	INTERPOSE_DECLARATION(Setcolorspace, void, CGColorSpaceRef);
	INTERPOSE_DECLARATION(SetwantsExtendedDynamicRangeContent, void, BOOL);
	INTERPOSE_DECLARATION(SetdisplaySyncEnabled, void, BOOL);
	INTERPOSE_DECLARATION(SetallowsNextDrawableTimeout, void, BOOL);
};

MTLPP_END

#endif /* MTIDrawable_hpp */
