// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIHeap.hpp"
#include "MTIBuffer.hpp"
#include "MTITexture.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTIHeapTrace, id<MTLHeap>);

struct MTITraceHeapSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceHeapSetLabelHandler()
	: MTITraceCommandHandler("MTLHeap", "setLabel")
	{
		
	}
	
	void Trace(id Object, NSString* Label)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString(Label ? [Label UTF8String] : "");

		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString label;
		fs >> label;
		
		[(id<MTLHeap>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceHeapSetLabelHandler GMTITraceHeapSetLabelHandler;
void MTIHeapTrace::SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Ptr)
{
	GMTITraceHeapSetLabelHandler.Trace(Obj, Ptr);
	Original(Obj, Cmd, Ptr);
}

struct MTITraceHeapSetPurgeableStateHandler : public MTITraceCommandHandler
{
	MTITraceHeapSetPurgeableStateHandler()
	: MTITraceCommandHandler("MTLHeap", "setPurgeableState")
	{
		
	}
	
	MTLPurgeableState Trace(id Object, MTLPurgeableState State, MTLPurgeableState Result)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << State;
		fs << Result;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger State, Result;
		fs >> State;
		fs >> Result;
		
		[(id<MTLHeap>)MTITrace::Get().FetchObject(Header.Receiver) setPurgeableState:(MTLPurgeableState)State];
	}
};
static MTITraceHeapSetPurgeableStateHandler GMTITraceHeapSetPurgeableStateHandler;
MTLPurgeableState MTIHeapTrace::SetPurgeableStateImpl(id Obj, SEL Cmd, Super::SetPurgeableStateType::DefinedIMP Original, MTLPurgeableState S)
{
	return GMTITraceHeapSetPurgeableStateHandler.Trace(Obj, S, Original(Obj, Cmd, S));
}

NSUInteger MTIHeapTrace::MaxAvailableSizeWithAlignmentImpl(id Obj, SEL Cmd, Super::MaxAvailableSizeWithAlignmentType::DefinedIMP Original, NSUInteger A)
{
	return Original(Obj, Cmd, A);
}

struct MTITraceHeapNewBufferHandler : public MTITraceCommandHandler
{
	MTITraceHeapNewBufferHandler()
	: MTITraceCommandHandler("MTLHeap", "newBufferWithLength")
	{
		
	}
	
	id<MTLBuffer> Trace(id Object, NSUInteger Len, MTLResourceOptions Opt, id<MTLBuffer> Buffer)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (NSUInteger)Len;
		fs << (NSUInteger)Opt;
		fs << (uintptr_t)Buffer;
		
		MTITrace::Get().EndWrite();
		return Buffer;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger Len;
		fs >> Len;
		
		NSUInteger Opt;
		fs >> Opt;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLBuffer> Buffer = [(id<MTLHeap>)MTITrace::Get().FetchObject(Header.Receiver) newBufferWithLength: Len options:(MTLResourceOptions)Opt];
		assert(Buffer);
		
		MTITrace::Get().RegisterObject(Result, Buffer);
	}
};
static MTITraceHeapNewBufferHandler GMTITraceHeapNewBufferHandler;
id<MTLBuffer> MTIHeapTrace::NewBufferWithLengthImpl(id Obj, SEL Cmd, Super::NewBufferWithLengthType::DefinedIMP Original, NSUInteger L, MTLResourceOptions O)
{
	return GMTITraceHeapNewBufferHandler.Trace(Obj, L, O, MTIBufferTrace::Register(Original(Obj, Cmd, L, O)));
}


struct MTITraceHeapNewTextureHandler : public MTITraceCommandHandler
{
	MTITraceHeapNewTextureHandler()
	: MTITraceCommandHandler("MTLHeap", "newTextureWithDescriptor")
	{
		
	}
	
	id<MTLTexture> Trace(id Object, MTLTextureDescriptor* Desc, id<MTLTexture> Texture)
	{
		GMTITraceNewTextureDescHandler.Trace(Desc);
		
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Desc;
		fs << (uintptr_t)Texture;
		
		MTITrace::Get().EndWrite();
		return Texture;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLTexture> Texture = [(id<MTLDevice>)MTITrace::Get().FetchObject(Header.Receiver) newTextureWithDescriptor:(MTLTextureDescriptor*)MTITrace::Get().FetchObject(Desc)];
		assert(Texture);
		
		MTITrace::Get().RegisterObject(Result, Texture);
	}
};
static MTITraceHeapNewTextureHandler GMTITraceHeapNewTextureHandler;
id<MTLTexture> MTIHeapTrace::NewTextureWithDescriptorImpl(id Obj, SEL Cmd, Super::NewTextureWithDescriptorType::DefinedIMP Original, MTLTextureDescriptor* D)
{
	return GMTITraceHeapNewTextureHandler.Trace(Obj, D, MTITextureTrace::Register(Original(Obj, Cmd, D)));
}

MTLPP_END
