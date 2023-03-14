// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHIPrivate.h: Private Metal RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "PixelFormat.h"

// Dependencies
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

// Metal C++ wrapper
THIRD_PARTY_INCLUDES_START
#include "mtlpp.hpp"
THIRD_PARTY_INCLUDES_END

// Whether the Metal RHI is initialized sufficiently to handle resources
extern bool GIsMetalInitialized;

// Requirement for vertex buffer offset field
#if PLATFORM_MAC
const uint32 BufferOffsetAlignment = 256;
const uint32 BufferBackedLinearTextureOffsetAlignment = 1024;
#else
const uint32 BufferOffsetAlignment = 16;
const uint32 BufferBackedLinearTextureOffsetAlignment = 64;
#endif

// The maximum buffer page size that can be uploaded in a set*Bytes call
const uint32 MetalBufferPageSize = 4096;

// The buffer size that is more efficiently uploaded in a set*Bytes call - defined in terms of BufferOffsetAlignment
#if PLATFORM_MAC
const uint32 MetalBufferBytesSize = BufferOffsetAlignment * 2;
#else
const uint32 MetalBufferBytesSize = BufferOffsetAlignment * 32;
#endif

#include "MetalRHI.h"
#include "MetalDynamicRHI.h"
#include "RHI.h"

#define BUFFER_CACHE_MODE mtlpp::ResourceOptions::CpuCacheModeDefaultCache

#if PLATFORM_MAC
#define BUFFER_MANAGED_MEM mtlpp::ResourceOptions::StorageModeManaged
#define BUFFER_STORAGE_MODE mtlpp::StorageMode::Managed
#define BUFFER_RESOURCE_STORAGE_MANAGED mtlpp::ResourceOptions::StorageModeManaged
#define BUFFER_DYNAMIC_REALLOC BUF_AnyDynamic
// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 31;
#else
#define BUFFER_MANAGED_MEM 0
#define BUFFER_STORAGE_MODE mtlpp::StorageMode::Shared
#define BUFFER_RESOURCE_STORAGE_MANAGED mtlpp::ResourceOptions::StorageModeShared
#define BUFFER_DYNAMIC_REALLOC BUF_AnyDynamic
// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 30;
#endif

// Unavailable on iOS, but dealing with this clutters the code.
enum EMTLTextureType
{
	EMTLTextureTypeCubeArray = 6
};

// This is the right VERSION check, see Availability.h in the SDK
#define METAL_SUPPORTS_INDIRECT_ARGUMENT_BUFFERS 1
#define METAL_SUPPORTS_CAPTURE_MANAGER 1
#define METAL_SUPPORTS_TILE_SHADERS 1
// In addition to compile-time SDK checks we also need a way to check if these are available on runtime
extern bool GMetalSupportsCaptureManager;

struct FMetalBufferFormat
{
	// Valid linear texture pixel formats - potentially different than the actual texture formats
	mtlpp::PixelFormat LinearTextureFormat;
	// Metal buffer data types for manual ALU format conversions
	uint8 DataFormat;
};

extern FMetalBufferFormat GMetalBufferFormats[PF_MAX];

#define METAL_DEBUG_OPTIONS !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if METAL_DEBUG_OPTIONS
#define METAL_DEBUG_OPTION(Code) Code
#else
#define METAL_DEBUG_OPTION(Code)
#endif

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
#define METAL_DEBUG_ONLY(Code) Code
#define METAL_DEBUG_LAYER(Level, Code) if (SafeGetRuntimeDebuggingLevel() >= Level) Code
#else
#define METAL_DEBUG_ONLY(Code)
#define METAL_DEBUG_LAYER(Level, Code)
#endif

extern bool GMetalCommandBufferDebuggingEnabled;

/** Set to 1 to enable GPU events in Xcode frame debugger */
#ifndef ENABLE_METAL_GPUEVENTS_IN_TEST
	#define ENABLE_METAL_GPUEVENTS_IN_TEST 0
#endif
#define ENABLE_METAL_GPUEVENTS	(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT || (UE_BUILD_TEST && ENABLE_METAL_GPUEVENTS_IN_TEST))
#define ENABLE_METAL_GPUPROFILE	(ENABLE_METAL_GPUEVENTS && 1)

#if ENABLE_METAL_GPUPROFILE
#define METAL_GPUPROFILE(Code) Code
#else
#define METAL_GPUPROFILE(Code) 
#endif

#define UNREAL_TO_METAL_BUFFER_INDEX(Index) ((MaxMetalStreams - 1) - Index)
#define METAL_TO_UNREAL_BUFFER_INDEX(Index) ((MaxMetalStreams - 1) - Index)

#define METAL_NEW_NONNULL_DECL (__clang_major__ >= 9)

#if PLATFORM_IOS
#define METAL_FATAL_ERROR(Format, ...)  { UE_LOG(LogMetal, Warning, Format, __VA_ARGS__); FIOSPlatformMisc::MetalAssert(); }
#else
#define METAL_FATAL_ERROR(Format, ...)	UE_LOG(LogMetal, Fatal, Format, __VA_ARGS__)
#endif
#define METAL_FATAL_ASSERT(Condition, Format, ...) if (!(Condition)) { METAL_FATAL_ERROR(Format, __VA_ARGS__); }

#if !defined(METAL_IGNORED)
	#define METAL_IGNORED(Func)
#endif

struct FMetalDebugInfo
{
	uint32 CmdBuffIndex;
	uint32 EncoderIndex;
	uint32 ContextIndex;
	uint32 CommandIndex;
	uint64 CommandBuffer;
	uint32 PSOSignature[4];
};

// Get a compute pipeline state used to implement some debug features.
mtlpp::ComputePipelineState GetMetalDebugComputeState();

// Access the internal context for the device-owning DynamicRHI object
FMetalDeviceContext& GetMetalDeviceContext();

// Safely release a metal object, correctly handling the case where the RHI has been destructed first
void METALRHI_API SafeReleaseMetalObject(id Object);

// Safely release a metal texture, correctly handling the case where the RHI has been destructed first
void SafeReleaseMetalTexture(FMetalTexture& Object);

// Safely release a metal buffer, correctly handling the case where the RHI has been destructed first
void SafeReleaseMetalBuffer(FMetalBuffer& Buffer);

// Safely release a fence, correctly handling cases where fences aren't supported or the debug implementation is used.
void SafeReleaseMetalFence(class FMetalFence* Object);

// Safely release a render pass descriptor so that it may be reused.
void SafeReleaseMetalRenderPassDescriptor(mtlpp::RenderPassDescriptor& Desc);

// Access the underlying surface object from any kind of texture
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture);

#define NOT_SUPPORTED(Func) UE_LOG(LogMetal, Fatal, TEXT("'%s' is not supported"), TEXT(Func));

// Verifies we are on the correct thread to mutate internal MetalRHI resources.
FORCEINLINE void CheckMetalThread()
{
    check((IsInRenderingThread() && (!IsRunningRHIInSeparateThread() || !FRHICommandListExecutor::IsRHIThreadActive())) || IsInRHIThread());
}

FORCEINLINE bool MetalIsSafeToUseRHIThreadResources()
{
	// we can use RHI thread resources if we are on the RHIThread or on RenderingThread when there's no RHI thread, or the RHI thread is stalled or inactive
	return (GIsMetalInitialized && !GIsRHIInitialized) ||
			IsInRHIThread() ||
			(IsInRenderingThread() && (!IsRunningRHIInSeparateThread() || !FRHICommandListExecutor::IsRHIThreadActive() || FRHICommandListImmediate::IsStalled() || FRHICommandListExecutor::IsRHIThreadCompletelyFlushed()));
}

FORCEINLINE int32 GetMetalCubeFace(ECubeFace Face)
{
	// According to Metal docs these should match now: https://developer.apple.com/library/prerelease/ios/documentation/Metal/Reference/MTLTexture_Ref/index.html#//apple_ref/c/tdef/MTLTextureType
	switch (Face)
	{
		case CubeFace_PosX:;
		default:			return 0;
		case CubeFace_NegX:	return 1;
		case CubeFace_PosY:	return 2;
		case CubeFace_NegY:	return 3;
		case CubeFace_PosZ:	return 4;
		case CubeFace_NegZ:	return 5;
	}
}

FORCEINLINE mtlpp::LoadAction GetMetalRTLoadAction(ERenderTargetLoadAction LoadAction)
{
	switch(LoadAction)
	{
		case ERenderTargetLoadAction::ENoAction: return mtlpp::LoadAction::DontCare;
		case ERenderTargetLoadAction::ELoad: return mtlpp::LoadAction::Load;
		case ERenderTargetLoadAction::EClear: return mtlpp::LoadAction::Clear;
		default: return mtlpp::LoadAction::DontCare;
	}
}

mtlpp::PrimitiveType TranslatePrimitiveType(uint32 PrimitiveType);

#if PLATFORM_MAC
mtlpp::PrimitiveTopologyClass TranslatePrimitiveTopology(uint32 PrimitiveType);
#endif

mtlpp::PixelFormat ToSRGBFormat(mtlpp::PixelFormat LinMTLFormat);

uint8 GetMetalPixelFormatKey(mtlpp::PixelFormat Format);

template<typename TRHIType>
static FORCEINLINE typename TMetalResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
{
	return static_cast<typename TMetalResourceTraits<TRHIType>::TConcreteType*>(Resource);
}

static FORCEINLINE FMetalSurface* ResourceCast(FRHITexture* Texture)
{
	return GetMetalSurfaceFromRHITexture(Texture);
}

uint32 SafeGetRuntimeDebuggingLevel();

extern int32 GMetalBufferZeroFill;

mtlpp::LanguageVersion ValidateVersion(uint32 Version);

// Needs to be the same as EShaderFrequency when all stages are supported, but unlike EShaderFrequency you can compile out stages.
enum EMetalShaderStages
{
	Vertex,
	Pixel,
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	Geometry,
#endif
	Compute,
	
	Num,
};

FORCEINLINE EShaderFrequency GetRHIShaderFrequency(EMetalShaderStages Stage)
{
	switch (Stage)
	{
		case EMetalShaderStages::Vertex:
			return SF_Vertex;
		case EMetalShaderStages::Pixel:
			return SF_Pixel;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		case EMetalShaderStages::Geometry:
			return SF_Geometry;
#endif
		case EMetalShaderStages::Compute:
			return SF_Compute;
		default:
			return SF_NumFrequencies;
	}
}

FORCEINLINE EMetalShaderStages GetMetalShaderFrequency(EShaderFrequency Stage)
{
	switch (Stage)
	{
		case SF_Vertex:
			return EMetalShaderStages::Vertex;
		case SF_Pixel:
			return EMetalShaderStages::Pixel;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		case SF_Geometry:
			return EMetalShaderStages::Geometry;
#endif
		case SF_Compute:
			return EMetalShaderStages::Compute;
		default:
			return EMetalShaderStages::Num;
	}
}

#include "MetalStateCache.h"
#include "MetalContext.h"
