// Copyright Epic Games, Inc. All Rights Reserved.

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#include "MTIDrawable.hpp"
#include "MTITexture.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTIDrawableTrace, id<MTLDrawable>);

struct MTITraceDrawableTextureHandler : public MTITraceCommandHandler
{
	MTITraceDrawableTextureHandler()
	: MTITraceCommandHandler("MTLDrawable", "texture")
	{
		
	}
	
	id <MTLTexture> Trace(id Object, id <MTLTexture> Tex)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Tex;
		
		MTITrace::Get().EndWrite();
		return Tex;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Tex;
		fs >> Tex;
		
		id<MTLTexture> Texture = [(id<CAMetalDrawable>)MTITrace::Get().FetchObject(Header.Receiver) texture];
		
		MTITrace::Get().RegisterObject(Tex, Texture);
	}
};
static MTITraceDrawableTextureHandler GMTITraceDrawableTextureHandler;
id <MTLTexture> MTIDrawableTrace::TextureImpl(id Obj, SEL Cmd, Super::TextureType::DefinedIMP Original)
{
	id <MTLTexture> Tex = GMTITraceDrawableTextureHandler.Trace(Obj, Original(Obj, Cmd));
	return MTITextureTrace::Register(Tex);
}

CAMetalLayer* MTIDrawableTrace::LayerImpl(id Obj, SEL Cmd, Super::LayerType::DefinedIMP Original)
{
	return Original(Obj, Cmd);
}

struct MTITraceDrawablePresentHandler : public MTITraceCommandHandler
{
	MTITraceDrawablePresentHandler()
	: MTITraceCommandHandler("MTLDrawable", "present")
	{
		
	}
	
	void Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		[(id<CAMetalDrawable>)MTITrace::Get().FetchObject(Header.Receiver) present];
	}
};
static MTITraceDrawablePresentHandler GMTITraceDrawablePresentHandler;
void MTIDrawableTrace::PresentImpl(id Obj, SEL Cmd, Super::PresentType::DefinedIMP Original)
{
	GMTITraceDrawablePresentHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct MTITraceDrawablePresentAtTimeHandler : public MTITraceCommandHandler
{
	MTITraceDrawablePresentAtTimeHandler()
	: MTITraceCommandHandler("MTLDrawable", "present")
	{
		
	}
	
	void Trace(id Object, CFTimeInterval Time)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Time;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		CFTimeInterval Time;
		fs >> Time;
		
		[(id<CAMetalDrawable>)MTITrace::Get().FetchObject(Header.Receiver) presentAtTime:Time];
	}
};
static MTITraceDrawablePresentAtTimeHandler GMTITraceDrawablePresentAtTimeHandler;
void MTIDrawableTrace::PresentAtTimeImpl(id Obj, SEL Cmd, Super::PresentAtTimeType::DefinedIMP Original, CFTimeInterval Time)
{
	GMTITraceDrawablePresentAtTimeHandler.Trace(Obj, Time);
	Original(Obj, Cmd, Time);
}

INTERPOSE_DEFINITION(MTIDrawableTrace, PresentAfterMinimumDuration, void, CFTimeInterval I)
{
	Original(Obj, Cmd, I);
}

INTERPOSE_DEFINITION(MTIDrawableTrace, AddPresentedHandler, void, MTLDrawablePresentedHandler B)
{
	Original(Obj, Cmd, B);
}

struct MTITraceLayerSetdeviceHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetdeviceHandler()
	: MTITraceCommandHandler("CAMetalLayer", "Setdevice")
	{
		
	}
	
	void Trace(id Object, id <MTLDevice> Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Val;
		fs >> Val;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setDevice:MTITrace::Get().FetchObject(Val)];
	}
};
static MTITraceLayerSetdeviceHandler GMTITraceLayerSetdeviceHandler;
INTERPOSE_DEFINITION(MTILayerTrace, Setdevice, void, id <MTLDevice> Val)
{
	GMTITraceLayerSetdeviceHandler.Trace(Obj, Val);
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerSetpixelFormatHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetpixelFormatHandler()
	: MTITraceCommandHandler("CAMetalLayer", "SetpixelFormat")
	{
		
	}
	
	void Trace(id Object, MTLPixelFormat Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger Val;
		fs >> Val;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setPixelFormat: (MTLPixelFormat)Val];
	}
};
static MTITraceLayerSetpixelFormatHandler GMTITraceLayerSetpixelFormatHandler;
INTERPOSE_DEFINITION(MTILayerTrace, SetpixelFormat, void, MTLPixelFormat Val)
{
	GMTITraceLayerSetpixelFormatHandler.Trace(Obj, Val);
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerSetframebufferOnlyHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetframebufferOnlyHandler()
	: MTITraceCommandHandler("CAMetalLayer", "SetframebufferOnly")
	{
		
	}
	
	void Trace(id Object, BOOL Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		BOOL Val;
		fs >> Val;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setFramebufferOnly:Val];
	}
};
static MTITraceLayerSetframebufferOnlyHandler GMTITraceLayerSetframebufferOnlyHandler;
INTERPOSE_DEFINITION(MTILayerTrace, SetframebufferOnly, void, BOOL Val)
{
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerSetddrawableSizeHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetddrawableSizeHandler()
	: MTITraceCommandHandler("CAMetalLayer", "SetddrawableSize")
	{
		
	}
	
	void Trace(id Object, CGSize Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Val.width;
		fs << Val.height;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		CGSize Val;
		fs >> Val.width;
		fs >> Val.height;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setDrawableSize:Val];
	}
};
static MTITraceLayerSetddrawableSizeHandler GMTITraceLayerSetddrawableSizeHandler;
INTERPOSE_DEFINITION(MTILayerTrace, SetddrawableSize, void, CGSize Val)
{
	GMTITraceLayerSetddrawableSizeHandler.Trace(Obj, Val);
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerNextDrawableHandler : public MTITraceCommandHandler
{
	MTITraceLayerNextDrawableHandler()
	: MTITraceCommandHandler("CAMetalLayer", "NextDrawable")
	{
		
	}
	
	id <CAMetalDrawable> Trace(id Object, id <CAMetalDrawable> Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Val;
		
		MTITrace::Get().EndWrite();
		return Val;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Val;
		fs >> Val;
		
		id <CAMetalDrawable> Drawable = [(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) nextDrawable];
		MTITrace::Get().RegisterObject(Val, Drawable);
	}
};
static MTITraceLayerNextDrawableHandler GMTITraceLayerNextDrawableHandler;
INTERPOSE_DEFINITION_VOID(MTILayerTrace, NextDrawable, id <CAMetalDrawable>)
{
	return GMTITraceLayerNextDrawableHandler.Trace(Obj, (id<CAMetalDrawable>)MTIDrawableTrace::Register(Original(Obj, Cmd)));
}

struct MTITraceLayerSetmaximumDrawableCountHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetmaximumDrawableCountHandler()
	: MTITraceCommandHandler("CAMetalLayer", "SetmaximumDrawableCount")
	{
		
	}
	
	void Trace(id Object, NSUInteger Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger Val;
		fs >> Val;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setMaximumDrawableCount:Val];
	}
};
static MTITraceLayerSetmaximumDrawableCountHandler GMTITraceLayerSetmaximumDrawableCountHandler;
INTERPOSE_DEFINITION(MTILayerTrace, SetmaximumDrawableCount, void, NSUInteger Val)
{
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerSetpresentsWithTransactionHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetpresentsWithTransactionHandler()
	: MTITraceCommandHandler("CAMetalLayer", "SetpresentsWithTransaction")
	{
		
	}
	
	void Trace(id Object, BOOL Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		BOOL Val;
		fs >> Val;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setPresentsWithTransaction:Val];
	}
};
static MTITraceLayerSetpresentsWithTransactionHandler GMTITraceLayerSetpresentsWithTransactionHandler;
INTERPOSE_DEFINITION(MTILayerTrace, SetpresentsWithTransaction, void, BOOL Val)
{
	GMTITraceLayerSetpresentsWithTransactionHandler.Trace(Obj, Val);
	Original(Obj, Cmd, Val);
}

INTERPOSE_DEFINITION(MTILayerTrace, Setcolorspace, void, CGColorSpaceRef Val)
{
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerSetwantsExtendedDynamicRangeContentHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetwantsExtendedDynamicRangeContentHandler()
	: MTITraceCommandHandler("CAMetalLayer", "SetwantsExtendedDynamicRangeContent")
	{
		
	}
	
	void Trace(id Object, BOOL Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		BOOL Val;
		fs >> Val;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setWantsExtendedDynamicRangeContent:Val];
	}
};
static MTITraceLayerSetwantsExtendedDynamicRangeContentHandler GMTITraceLayerSetwantsExtendedDynamicRangeContentHandler;
INTERPOSE_DEFINITION(MTILayerTrace, SetwantsExtendedDynamicRangeContent, void, BOOL Val)
{
	GMTITraceLayerSetwantsExtendedDynamicRangeContentHandler.Trace(Obj, Val);
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerSetdisplaySyncEnabledHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetdisplaySyncEnabledHandler()
	: MTITraceCommandHandler("CAMetalLayer", "SetdisplaySyncEnabled")
	{
		
	}
	
	void Trace(id Object, BOOL Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		BOOL Val;
		fs >> Val;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setDisplaySyncEnabled:Val];
	}
};
static MTITraceLayerSetdisplaySyncEnabledHandler GMTITraceLayerSetdisplaySyncEnabledHandler;
INTERPOSE_DEFINITION(MTILayerTrace, SetdisplaySyncEnabled, void, BOOL Val)
{
	GMTITraceLayerSetdisplaySyncEnabledHandler.Trace(Obj, Val);
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerSetallowsNextDrawableTimeoutHandler : public MTITraceCommandHandler
{
	MTITraceLayerSetallowsNextDrawableTimeoutHandler()
	: MTITraceCommandHandler("CAMetalLayer", "SetallowsNextDrawableTimeout")
	{
		
	}
	
	void Trace(id Object, BOOL Val)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Val;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		BOOL Val;
		fs >> Val;
		
		[(CAMetalLayer*)MTITrace::Get().FetchObject(Header.Receiver) setAllowsNextDrawableTimeout:Val];
	}
};
static MTITraceLayerSetallowsNextDrawableTimeoutHandler GMTITraceLayerSetallowsNextDrawableTimeoutHandler;
INTERPOSE_DEFINITION(MTILayerTrace, SetallowsNextDrawableTimeout, void, BOOL Val)
{
	GMTITraceLayerSetallowsNextDrawableTimeoutHandler.Trace(Obj, Val);
	Original(Obj, Cmd, Val);
}

struct MTITraceLayerInitHandler : public MTITraceCommandHandler
{
	MTITraceLayerInitHandler()
	: MTITraceCommandHandler("CAMetalLayer", "init")
	{
		
	}
	
	id Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
		return Object;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		CAMetalLayer* Layer = [CAMetalLayer new];
		
		const NSRect ContentRect = NSMakeRect(0, 0, 800, 600);
		NSWindow* Window = [[NSWindow alloc] initWithContentRect:ContentRect styleMask:(NSWindowStyleMask)NSWindowStyleMaskResizable|NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
		
		NSView* View = [[NSView alloc] initWithFrame:ContentRect];
		[View setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		[View setWantsLayer:YES];
		
		CGFloat bgColor[] = { 0.0, 0.0, 0.0, 0.0 };
		Layer.edgeAntialiasingMask = 0;
		Layer.masksToBounds = YES;
		Layer.backgroundColor = CGColorCreate(CGColorSpaceCreateDeviceRGB(), bgColor);
		Layer.presentsWithTransaction = NO;
		Layer.anchorPoint = CGPointMake(0.5, 0.5);
		Layer.frame = ContentRect;
		Layer.magnificationFilter = kCAFilterNearest;
		Layer.minificationFilter = kCAFilterNearest;
		[Layer removeAllAnimations];
		[View setLayer:Layer];
		[Window setContentView:View];
		
		MTITrace::Get().RegisterObject(Header.Receiver, Layer);
	}
};
static MTITraceLayerInitHandler GMTITraceLayerInitHandler;
INTERPOSE_DEFINITION_VOID(MTILayerTrace, init, id)
{
	return GMTITraceLayerInitHandler.Trace(Original(Obj, Cmd));
}

INTERPOSE_DEFINITION(MTILayerTrace, initWithLayer, id, id Val)
{
	return GMTITraceLayerInitHandler.Trace(Original(Obj, Cmd, Val));
}


INTERPOSE_CLASS_REGISTER(MTILayerTrace, CAMetalLayer);


MTLPP_END
