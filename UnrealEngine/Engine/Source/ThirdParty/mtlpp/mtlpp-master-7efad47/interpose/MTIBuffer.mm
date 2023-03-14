// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIBuffer.hpp"
#include "MTITexture.hpp"
#include "MTITrace.hpp"

#include <execinfo.h>
#include <sys/mman.h>
#include <objc/runtime.h>

MTLPP_BEGIN

void* MTIBufferTrace::ContentsImpl(id Obj, SEL Cmd, Super::ContentsType::DefinedIMP Original)
{
	return Original(Obj, Cmd);
}

struct MTITraceBufferDidModifyRangeHandler : public MTITraceCommandHandler
{
	MTITraceBufferDidModifyRangeHandler()
	: MTITraceCommandHandler("MTLBuffer", "didModifyRange")
	{
		
	}
	
	void Trace(id Object, NSRange Range)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << Range.location;
		fs << Range.length;
		
		MTITraceArray<uint8> Data;
		Data.Data = (uint8*)[(id<MTLBuffer>)Object contents] + Range.location;
		Data.Length = Range.length;
		
		fs << Data;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSRange Range;
		fs >> Range.location;
		fs >> Range.length;

		MTITraceArray<uint8> Data;
		fs >> Data;
		
		memcpy(((uint8*)[(id<MTLBuffer>)MTITrace::Get().FetchObject(Header.Receiver) contents] + Range.location), Data.Backing.data(), Data.Length);
		
		[(id<MTLBuffer>)MTITrace::Get().FetchObject(Header.Receiver) didModifyRange:Range];
	}
};
static MTITraceBufferDidModifyRangeHandler GMTITraceBufferDidModifyRangeHandler;
void MTIBufferTrace::DidModifyRangeImpl(id Obj, SEL Cmd, Super::DidModifyRangeType::DefinedIMP Original, NSRange Range)
{
	GMTITraceBufferDidModifyRangeHandler.Trace(Obj, Range);
	Original(Obj, Cmd, Range);
}

struct MTITraceBufferNewTextureHandler : public MTITraceCommandHandler
{
	MTITraceBufferNewTextureHandler()
	: MTITraceCommandHandler("MTLBuffer", "newTextureWithDescriptor")
	{
		
	}
	
	id<MTLTexture> Trace(id Object, MTLTextureDescriptor* Desc, NSUInteger Offset, NSUInteger BPR, id<MTLTexture> Texture)
	{
		GMTITraceNewTextureDescHandler.Trace(Desc);
		
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Desc;
		fs << Offset;
		fs << BPR;
		fs << (uintptr_t)Texture;
		
		MTITrace::Get().EndWrite();
		return Texture;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		NSUInteger Offset;
		NSUInteger BPR;
		fs >> Offset;
		fs >> BPR;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLTexture> Texture = [(id<MTLBuffer>)MTITrace::Get().FetchObject(Header.Receiver) newTextureWithDescriptor:(MTLTextureDescriptor*)MTITrace::Get().FetchObject(Desc) offset:Offset bytesPerRow:BPR];
		assert(Texture);
		
		MTITrace::Get().RegisterObject(Result, Texture);
	}
};
static MTITraceBufferNewTextureHandler GMTITraceBufferNewTextureHandler;
id<MTLTexture> MTIBufferTrace::NewTextureWithDescriptorOffsetBytesPerRowImpl(id Obj, SEL Cmd, Super::NewTextureWithDescriptorOffsetBytesPerRowType::DefinedIMP Original, MTLTextureDescriptor* Desc, NSUInteger Offset, NSUInteger BPR)
{
	return GMTITraceBufferNewTextureHandler.Trace(Obj, Desc, Offset, BPR, MTITextureTrace::Register(Original(Obj, Cmd, Desc, Offset, BPR)));
}

struct MTITraceBufferAddDebugMarkerRangeHandler : public MTITraceCommandHandler
{
	MTITraceBufferAddDebugMarkerRangeHandler()
	: MTITraceCommandHandler("MTLBuffer", "addDebugMarkerRange")
	{
		
	}
	
	void Trace(id Object, NSString* Str, NSRange Range)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString(Str ? [Str UTF8String] : "");
		fs << Range.location;
		fs << Range.length;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString Label;
		fs >> Label;
		
		NSRange Range;
		fs >> Range.location;
		fs >> Range.length;
		
		[(id<MTLBuffer>)MTITrace::Get().FetchObject(Header.Receiver) addDebugMarker:[NSString stringWithUTF8String:Label.c_str()] range:Range];
	}
};
static MTITraceBufferAddDebugMarkerRangeHandler GMTITraceBufferAddDebugMarkerRangeHandler;
INTERPOSE_DEFINITION(MTIBufferTrace, AddDebugMarkerRange, void, NSString* Str, NSRange R)
{
	GMTITraceBufferAddDebugMarkerRangeHandler.Trace(Obj, Str, R);
	Original(Obj, Cmd, Str, R);
}

struct MTITraceBufferRemoveAllDebugMarkersHandler : public MTITraceCommandHandler
{
	MTITraceBufferRemoveAllDebugMarkersHandler()
	: MTITraceCommandHandler("MTLBuffer", "addDebugMarkerRange")
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
		[(id<MTLBuffer>)MTITrace::Get().FetchObject(Header.Receiver) removeAllDebugMarkers];
	}
};
static MTITraceBufferRemoveAllDebugMarkersHandler GMTITraceBufferRemoveAllDebugMarkersHandler;
INTERPOSE_DEFINITION_VOID(MTIBufferTrace, RemoveAllDebugMarkers, void)
{
	GMTITraceBufferRemoveAllDebugMarkersHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct FMTLPPObjectCallStacks
{
	FMTLPPObjectCallStacks* Next;
	void* Callstack[16];
	int Num;
};

@interface FMTLPPObjectZombie : NSObject
{
@public
	Class OriginalClass;
	OSQueueHead* CallStacks;
}
@end

@implementation FMTLPPObjectZombie

-(id)init
{
	self = (FMTLPPObjectZombie*)[super init];
	if (self)
	{
		OriginalClass = nil;
		CallStacks = nullptr;
	}
	return self;
}

-(void)dealloc
{
	// Denied!
	return;
	
	[super dealloc];
}

- (nullable NSMethodSignature *)methodSignatureForSelector:(SEL)sel
{
	NSLog(@"Selector %@ sent to deallocated instance %p of class %@", NSStringFromSelector(sel), self, OriginalClass);
	
	if (CallStacks)
	{
		int j = 0;
		while (FMTLPPObjectCallStacks* Stack = (FMTLPPObjectCallStacks*)OSAtomicDequeue(CallStacks, offsetof(FMTLPPObjectCallStacks, Next)))
		{
			if (Stack->Num)
			{
				NSLog(@"Callstack: %d", j);
				char** Symbols = backtrace_symbols(Stack->Callstack, Stack->Num);
				for (int i = 0; i < Stack->Num && Symbols; i++)
				{
					NSLog(@"\t%d: %s", i, Symbols[i] ? Symbols[i] : "Unknown");
				}
			}
			j++;
		}
	}
	
	abort();
}
@end

static OSQueueHead* GetCallStackQueueHead(id Object)
{
	OSQueueHead* Head = (OSQueueHead*)objc_getAssociatedObject(Object, (void const*)&MTIBufferTrace::RetainImpl);
	if (!Head)
	{
		Head = new OSQueueHead;
		Head->opaque1 = nullptr;
		Head->opaque2 = 0;
		objc_setAssociatedObject(Object, (void const*)&MTIBufferTrace::RetainImpl, (id)Head, OBJC_ASSOCIATION_ASSIGN);
	}
	return Head;
}

#define MAKE_CALLSTACK(Name, Object) FMTLPPObjectCallStacks* Name = new FMTLPPObjectCallStacks;	\
	Stack->Next = nullptr;	\
	Stack->Num = backtrace(Stack->Callstack, 16);	\
	OSAtomicEnqueue(GetCallStackQueueHead(Object), Stack, offsetof(FMTLPPObjectCallStacks, Next))

void MTIBufferTrace::RetainImpl(id Object, SEL Selector, void(*RetainPtr)(id,SEL))
{
	MAKE_CALLSTACK(Stack, Object);
	
	RetainPtr(Object, Selector);
}

void MTIBufferTrace::ReleaseImpl(id Object, SEL Selector, void(*ReleasePtr)(id,SEL))
{
	MAKE_CALLSTACK(Stack, Object);
	
	ReleasePtr(Object, Selector);
}

void MTIBufferTrace::DeallocImpl(id Object, SEL Selector, void(*DeallocPtr)(id,SEL))
{
	MAKE_CALLSTACK(Stack, Object);
	OSQueueHead* Head = GetCallStackQueueHead(Object);
	
	// First call the destructor and then release the memory - like C++ placement new/delete
	objc_destructInstance(Object);
	
	Class CurrentClass = [Object class];
	object_setClass(Object, [FMTLPPObjectZombie class]);
	FMTLPPObjectZombie* ZombieSelf = (FMTLPPObjectZombie*)Object;
	ZombieSelf->OriginalClass = CurrentClass;
	ZombieSelf->CallStacks = Head;
//	DeallocPtr(Object, Selector);
}

INTERPOSE_PROTOCOL_REGISTER(MTIBufferTrace, id<MTLBuffer>);


MTLPP_END
