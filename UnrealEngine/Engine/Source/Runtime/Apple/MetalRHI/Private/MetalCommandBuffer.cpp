// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandBuffere.cpp: Metal command buffer wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandBuffer.h"
#include "MetalRenderCommandEncoder.h"
#include "MetalBlitCommandEncoder.h"
#include "MetalComputeCommandEncoder.h"
#include <objc/runtime.h>

NSString* GMetalDebugCommandTypeNames[EMetalDebugCommandTypeInvalid] = {
	@"RenderEncoder",
	@"ComputeEncoder",
	@"BlitEncoder",
    @"EndEncoder",
    @"Pipeline",
	@"Draw",
	@"Dispatch",
	@"Blit",
	@"Signpost",
	@"PushGroup",
	@"PopGroup"
};

extern int32 GMetalRuntimeDebugLevel;

uint32 SafeGetRuntimeDebuggingLevel()
{
	return GIsRHIInitialized ? GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() : GMetalRuntimeDebugLevel;
}

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS

@implementation FMetalDebugCommandBuffer

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugCommandBuffer)

-(id)initWithCommandBuffer:(id<MTLCommandBuffer>)Buffer
{
	id Self = [super init];
	if (Self)
	{
        DebugLevel = (EMetalDebugLevel)GMetalRuntimeDebugLevel;
		InnerBuffer = Buffer;
		DebugGroup = [NSMutableArray new];
		ActiveEncoder = nil;
		DebugInfoBuffer = nil;
		if (DebugLevel >= EMetalDebugLevelValidation)
		{
			DebugInfoBuffer = [Buffer.device newBufferWithLength:BufferOffsetAlignment options:0];
		}
	}
	return Self;
}

-(void)dealloc
{
	for (FMetalDebugCommand* Command : DebugCommands)
	{
		[Command->Label release];
		[Command->PassDesc release];
		delete Command;
	}
	
	[DebugGroup release];
	[DebugInfoBuffer release];
	DebugInfoBuffer = nil;
	
	[super dealloc];
}

@end

FMetalCommandBufferDebugging FMetalCommandBufferDebugging::Get(mtlpp::CommandBuffer& Buffer)
{
	return Buffer.GetAssociatedObject<FMetalCommandBufferDebugging>((void const*)&FMetalCommandBufferDebugging::Get);
}

FMetalCommandBufferDebugging::FMetalCommandBufferDebugging()
: ns::Object<FMetalDebugCommandBuffer*>(nullptr)
{
	
}
FMetalCommandBufferDebugging::FMetalCommandBufferDebugging(mtlpp::CommandBuffer& Buffer)
: ns::Object<FMetalDebugCommandBuffer*>([[FMetalDebugCommandBuffer alloc] initWithCommandBuffer:Buffer.GetPtr()], ns::Ownership::Assign)
{
	Buffer.SetAssociatedObject((void const*)&FMetalCommandBufferDebugging::Get, *this);
}
FMetalCommandBufferDebugging::FMetalCommandBufferDebugging(FMetalDebugCommandBuffer* handle)
: ns::Object<FMetalDebugCommandBuffer*>(handle)
{
	
}

ns::AutoReleased<ns::String> FMetalCommandBufferDebugging::GetDescription()
{
	NSMutableString* String = [[NSMutableString new] autorelease];
	NSString* Label = m_ptr->InnerBuffer.label ? m_ptr->InnerBuffer.label : @"Unknown";
	[String appendFormat:@"Command Buffer %p %@:", m_ptr->InnerBuffer, Label];
	return ns::AutoReleased<ns::String>(String);
}

ns::AutoReleased<ns::String> FMetalCommandBufferDebugging::GetDebugDescription()
{
	NSMutableString* String = [[NSMutableString new] autorelease];
	NSString* Label = m_ptr->InnerBuffer.label ? m_ptr->InnerBuffer.label : @"Unknown";
	[String appendFormat:@"Command Buffer %p %@:", m_ptr->InnerBuffer, Label];
	
	uint32 Index = 0;
	if (m_ptr->DebugInfoBuffer)
	{
		Index = *((uint32*)m_ptr->DebugInfoBuffer.contents);
	}
	
	uint32 Count = 1;
	for (FMetalDebugCommand* Command : m_ptr->DebugCommands)
	{
		if (Index == Count++)
		{
			[String appendFormat:@"\n\t--> %@: %@", GMetalDebugCommandTypeNames[Command->Type], Command->Label];
		}
		else
		{
			[String appendFormat:@"\n\t%@: %@", GMetalDebugCommandTypeNames[Command->Type], Command->Label];
		}
	}
	
	return ns::AutoReleased<ns::String>(String);
}

void FMetalCommandBufferDebugging::BeginRenderCommandEncoder(ns::String const& Label, mtlpp::RenderPassDescriptor const& Desc)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelValidation)
	{
		if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
		{
			check(!m_ptr->ActiveEncoder);
			m_ptr->ActiveEncoder = [Label.GetPtr() retain];
			FMetalDebugCommand* Command = new FMetalDebugCommand;
			Command->Type = EMetalDebugCommandTypeRenderEncoder;
			Command->Label = [Label.GetPtr() retain];
			Command->PassDesc = [Desc.GetPtr() retain];
			m_ptr->DebugCommands.Add(Command);
		}
	}
}
void FMetalCommandBufferDebugging::BeginComputeCommandEncoder(ns::String const& Label)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		check(!m_ptr->ActiveEncoder);
		m_ptr->ActiveEncoder = [Label.GetPtr() retain];
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypeComputeEncoder;
		Command->Label = [m_ptr->ActiveEncoder retain];
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
	}
}
void FMetalCommandBufferDebugging::BeginBlitCommandEncoder(ns::String const& Label)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		check(!m_ptr->ActiveEncoder);
		m_ptr->ActiveEncoder = [Label.GetPtr() retain];
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypeBlitEncoder;
		Command->Label = [m_ptr->ActiveEncoder retain];
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
	}
}
void FMetalCommandBufferDebugging::EndCommandEncoder()
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		check(m_ptr->ActiveEncoder);
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypeEndEncoder;
		Command->Label = m_ptr->ActiveEncoder;
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
		m_ptr->ActiveEncoder = nil;
	}
}

void FMetalCommandBufferDebugging::SetPipeline(ns::String const& Desc)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypePipeline;
		Command->Label = [Desc.GetPtr() retain];
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
	}
}
void FMetalCommandBufferDebugging::Draw(ns::String const& Desc)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypeDraw;
		Command->Label = [Desc.GetPtr() retain];
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
	}
}
void FMetalCommandBufferDebugging::Dispatch(ns::String const& Desc)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypeDispatch;
		Command->Label = [Desc.GetPtr() retain];
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
	}
}
void FMetalCommandBufferDebugging::Blit(ns::String const& Desc)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypeBlit;
		Command->Label = [Desc.GetPtr() retain];
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
	}
}

void FMetalCommandBufferDebugging::InsertDebugSignpost(ns::String const& Label)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypeSignpost;
		Command->Label = [Label.GetPtr() retain];
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
	}
}
void FMetalCommandBufferDebugging::PushDebugGroup(ns::String const& Group)
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		[m_ptr->DebugGroup addObject:Group.GetPtr()];
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypePushGroup;
		Command->Label = [Group.GetPtr() retain];
		Command->PassDesc = nil;
		m_ptr->DebugCommands.Add(Command);
	}
}
void FMetalCommandBufferDebugging::PopDebugGroup()
{
	if (m_ptr->DebugLevel >= EMetalDebugLevelLogOperations)
	{
		if (m_ptr->DebugGroup.lastObject)
		{
			FMetalDebugCommand* Command = new FMetalDebugCommand;
			Command->Type = EMetalDebugCommandTypePopGroup;
			Command->Label = [m_ptr->DebugGroup.lastObject retain];
			Command->PassDesc = nil;
			m_ptr->DebugCommands.Add(Command);
		}
	}
}

#endif
