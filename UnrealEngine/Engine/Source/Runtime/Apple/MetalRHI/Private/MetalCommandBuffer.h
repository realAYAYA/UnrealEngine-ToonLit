// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalResources.h"
#include "MetalShaderResources.h"

/**
 * EMetalDebugCommandType: Types of command recorded in our debug command-buffer wrapper.
 */
enum EMetalDebugCommandType
{
	EMetalDebugCommandTypeRenderEncoder,
	EMetalDebugCommandTypeComputeEncoder,
	EMetalDebugCommandTypeBlitEncoder,
	EMetalDebugCommandTypeEndEncoder,
    EMetalDebugCommandTypePipeline,
	EMetalDebugCommandTypeDraw,
	EMetalDebugCommandTypeDispatch,
	EMetalDebugCommandTypeBlit,
	EMetalDebugCommandTypeSignpost,
	EMetalDebugCommandTypePushGroup,
	EMetalDebugCommandTypePopGroup,
	EMetalDebugCommandTypeInvalid
};

/**
 * EMetalDebugLevel: Level of Metal debug features to be enabled.
 */
enum EMetalDebugLevel
{
	EMetalDebugLevelOff,
	EMetalDebugLevelFastValidation,
	EMetalDebugLevelResetOnBind,
	EMetalDebugLevelConditionalSubmit,
	EMetalDebugLevelValidation,
	EMetalDebugLevelLogOperations,
	EMetalDebugLevelWaitForComplete,
};

NS_ASSUME_NONNULL_BEGIN
/**
 * FMetalDebugCommand: The data recorded for each command in the debug command-buffer wrapper.
 */
struct FMetalDebugCommand
{
	NSString* Label;
	EMetalDebugCommandType Type;
	MTLRenderPassDescriptor* PassDesc;
};

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS

/**
 * FMetalDebugCommandBuffer: Wrapper around id<MTLCommandBuffer> that records information about commands.
 * This allows reporting of substantially more information in debug modes which can be especially helpful
 * when debugging GPU command-buffer failures.
 */
@interface FMetalDebugCommandBuffer : FApplePlatformObject
{
@public
	NSMutableArray<NSString*>* DebugGroup;
	NSString* ActiveEncoder;
	id<MTLCommandBuffer> InnerBuffer;
	TArray<FMetalDebugCommand*> DebugCommands;
	EMetalDebugLevel DebugLevel;
	id<MTLBuffer> DebugInfoBuffer;
};

/** Initialise the wrapper with the provided command-buffer. */
-(id)initWithCommandBuffer:(id<MTLCommandBuffer>)Buffer;

@end

class FMetalCommandBufferDebugging : public ns::Object<FMetalDebugCommandBuffer*>
{
public:
	FMetalCommandBufferDebugging();
	FMetalCommandBufferDebugging(mtlpp::CommandBuffer& Buffer);
	FMetalCommandBufferDebugging(FMetalDebugCommandBuffer* handle);
	
	static FMetalCommandBufferDebugging Get(mtlpp::CommandBuffer& Buffer);
	ns::AutoReleased<ns::String> GetDescription();
	ns::AutoReleased<ns::String> GetDebugDescription();
	
	void BeginRenderCommandEncoder(ns::String const& Label, mtlpp::RenderPassDescriptor const& Desc);
	void BeginComputeCommandEncoder(ns::String const& Label);
	void BeginBlitCommandEncoder(ns::String const& Label);
	void EndCommandEncoder();
	
	void SetPipeline(ns::String const& Desc);
	void Draw(ns::String const& Desc);
	void Dispatch(ns::String const& Desc);
	void Blit(ns::String const& Desc);
	
	void InsertDebugSignpost(ns::String const& Label);
	void PushDebugGroup(ns::String const& Group);
	void PopDebugGroup();
};

#endif

NS_ASSUME_NONNULL_END
