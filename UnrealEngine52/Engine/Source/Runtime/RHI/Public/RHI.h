// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHI.h: Render Hardware Interface definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/ContainersFwd.h"
#include "Stats/Stats.h"
#include "RHIDefinitions.h"
#include "Containers/StaticArray.h"
#include "Containers/StringFwd.h"
#include "Math/IntRect.h"
#include "Math/PerspectiveMatrix.h"
#include "Math/ScaleMatrix.h"
#include "Math/TranslationMatrix.h"
#include "PixelFormat.h"
#include "ProfilingDebugging/CsvProfilerConfig.h"
#include "GpuProfilerTrace.h"
#include "RHIShaderPlatform.h"
#include "RHIFeatureLevel.h"
#include "RHIAccess.h"

class FResourceArrayInterface;
class FResourceBulkDataInterface;

/** RHI Logging. */
RHI_API DECLARE_LOG_CATEGORY_EXTERN(LogRHI,Log,VeryVerbose);

/**
 * RHI configuration settings.
 */

namespace RHIConfig
{
	RHI_API bool ShouldSaveScreenshotAfterProfilingGPU();
	RHI_API bool ShouldShowProfilerAfterProfilingGPU();
	RHI_API float GetGPUHitchThreshold();
}

/**
 * RHI globals.
 */

/** True if the render hardware has been initialized. */
extern RHI_API bool GIsRHIInitialized;

class RHI_API FRHICommandList;

/**
 * RHI capabilities.
 */

/** Optimal number of persistent thread groups to fill the GPU. */
extern RHI_API int32 GRHIPersistentThreadGroupCount;

 /** The maximum number of mip-maps that a texture can contain. */
extern RHI_API int32 GMaxTextureMipCount;

/** Does the RHI implements CopyToTexture() with FRHICopyTextureInfo::NumMips > 1 */
UE_DEPRECATED(5.1, "All RHIs now support copying to multiple mips.")
extern RHI_API bool GRHISupportsCopyToTextureMultipleMips;

/** true if this platform has quad buffer stereo support. */
extern RHI_API bool GSupportsQuadBufferStereo;

/** true if the RHI supports textures that may be bound as both a render target and a shader resource. */
extern RHI_API bool GSupportsRenderDepthTargetableShaderResources;

/** true if the RHI supports Draw Indirect */
extern RHI_API bool GRHISupportsDrawIndirect;

/** Whether the RHI can send commands to the device context from multiple threads. Used in the GPU readback to avoid stalling the RHI threads. */
extern RHI_API bool GRHISupportsMultithreading;

/** Whether RHIGetRenderQueryResult can be safely called off the render thread. */
extern RHI_API bool GRHISupportsAsyncGetRenderQueryResult;

/** 
 * only set if RHI has the information (after init of the RHI and only if RHI has that information, never changes after that)
 * e.g. "NVIDIA GeForce GTX 670"
 */
extern RHI_API FString GRHIAdapterName;
extern RHI_API FString GRHIAdapterInternalDriverVersion;
extern RHI_API FString GRHIAdapterUserDriverVersion;
extern RHI_API FString GRHIAdapterDriverDate;
extern RHI_API bool GRHIAdapterDriverOnDenyList;
extern RHI_API uint32 GRHIDeviceId;
extern RHI_API uint32 GRHIDeviceRevision;

// 0 means not defined yet, use functions like IsRHIDeviceAMD() to access
extern RHI_API uint32 GRHIVendorId;

// true if the RHI supports Pixel Shader UAV
extern RHI_API bool GRHISupportsPixelShaderUAVs;

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceAMD();

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceIntel();

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceNVIDIA();

// helper to return the shader language version for Metal shader.
RHI_API uint32 RHIGetMetalShaderLanguageVersion(const FStaticShaderPlatform Platform);

// helper to check if a preview feature level has been requested.
RHI_API bool RHIGetPreviewFeatureLevel(ERHIFeatureLevel::Type& PreviewFeatureLevelOUT);

// helper to check if preferred EPixelFormat is supported, return one if it is not
RHI_API EPixelFormat RHIPreferredPixelFormatHint(EPixelFormat PreferredPixelFormat);

// helper to check which resource type should be used for clear (UAV) replacement shaders.
RHI_API int32 RHIGetPreferredClearUAVRectPSResourceType(const FStaticShaderPlatform Platform);

// helper to force dump all RHI resource to CSV file
RHI_API void RHIDumpResourceMemoryToCSV();

struct FRHIResourceStats
{
	FName Name;
	FName OwnerName;
	FString Type;
	FString Flags;
	uint64	SizeInBytes = 0;
	bool	bMarkedForDelete = false;
	bool	bTransient = false;
	bool	bStreaming = false;
	bool	bRenderTarget = false;
	bool	bDepthStencil = false;
	bool	bUnorderedAccessView = false;
	bool	bRayTracingAccelerationStructure = false;
	bool	bHasFlags = false;

	FRHIResourceStats(const FName& InName, const FName& InOwnerName, const FString& InType, const FString& InFlags, const uint64& InSizeInBytes,
						bool bInMarkedForDelete, bool bInTransient, bool bInStreaming, bool bInRT, bool bInDS, bool bInUAV, bool bInRTAS, bool bInHasFlags)
		: Name(InName)
		, OwnerName(InOwnerName)
		, Type(InType)
		, Flags(InFlags)
		, SizeInBytes(InSizeInBytes)
		, bMarkedForDelete(bInMarkedForDelete)
		, bTransient(bInTransient)
		, bStreaming(bInStreaming)
		, bRenderTarget(bInRT)
		, bDepthStencil(bInDS)
		, bUnorderedAccessView(bInUAV)
		, bRayTracingAccelerationStructure(bInRTAS)
		, bHasFlags(bInHasFlags)
	{ }
};

RHI_API void RHIGetTrackedResourceStats(TArray<TSharedPtr<FRHIResourceStats>>& OutResourceStats);

// Wrapper for GRHI## global variables, allows values to be overridden for mobile preview modes.
template <typename TValueType>
class TRHIGlobal
{
public:
	explicit TRHIGlobal(const TValueType& InValue) : Value(InValue) {}

	TRHIGlobal& operator=(const TValueType& InValue) 
	{
		Value = InValue; 
		return *this;
	}

#if WITH_EDITOR
	inline void SetPreviewOverride(const TValueType& InValue)
	{
		PreviewValue = InValue;
	}

	inline operator TValueType() const
	{ 
		return PreviewValue.IsSet() ? GetPreviewValue() : Value;
	}
#else
	inline operator TValueType() const { return Value; }
#endif

private:
	TValueType Value;
#if WITH_EDITOR
	TOptional<TValueType> PreviewValue;
	TValueType GetPreviewValue() const { return PreviewValue.GetValue(); }
#endif
};

#if WITH_EDITOR
template<>
inline int32 TRHIGlobal<int32>::GetPreviewValue() const 
{
	// ensure the preview values are subsets of RHI functionality.
	return FMath::Min(PreviewValue.GetValue(), Value);
}
template<>
inline bool TRHIGlobal<bool>::GetPreviewValue() const
{
	// ensure the preview values are subsets of RHI functionality.
	return PreviewValue.GetValue() && Value;
}
#endif

/** true if the GPU is AMD's Pre-GCN architecture */
extern RHI_API bool GRHIDeviceIsAMDPreGCNArchitecture;

/** true if PF_G8 render targets are supported */
extern RHI_API TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_G8;

/** true if PF_FloatRGBA render targets are supported */
extern RHI_API TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_FloatRGBA;

/** true if mobile framebuffer fetch is supported */
extern RHI_API bool GSupportsShaderFramebufferFetch;

/** true if mobile framebuffer fetch is supported from MRT's*/
extern RHI_API bool GSupportsShaderMRTFramebufferFetch;

/** true if mobile pixel local storage is supported */
extern RHI_API bool GSupportsPixelLocalStorage;

/** true if mobile depth & stencil fetch is supported */
extern RHI_API bool GSupportsShaderDepthStencilFetch;

/** true if RQT_AbsoluteTime is supported by RHICreateRenderQuery */
extern RHI_API bool GSupportsTimestampRenderQueries;

/** true if RQT_AbsoluteTime is supported by RHICreateRenderQuery */
extern RHI_API bool GRHISupportsGPUTimestampBubblesRemoval;

/** true if RHIGetGPUFrameCycles removes CPu generated bubbles. */
extern RHI_API bool GRHISupportsFrameCyclesBubblesRemoval;

/** true if RHIGetGPUUsage() is supported. */
extern RHI_API bool GRHISupportsGPUUsage;

/** true if the GPU supports hidden surface removal in hardware. */
extern RHI_API bool GHardwareHiddenSurfaceRemoval;

/** true if the RHI supports asynchronous creation of texture resources */
extern RHI_API bool GRHISupportsAsyncTextureCreation;

/** true if the RHI supports quad topology (PT_QuadList). */
extern RHI_API bool GRHISupportsQuadTopology;

/** true if the RHI supports rectangular topology (PT_RectList). */
extern RHI_API bool GRHISupportsRectTopology;

/** true if the RHI supports primitive shaders. */
extern RHI_API bool GRHISupportsPrimitiveShaders;

/** true if the RHI supports 64 bit uint atomics. */
extern RHI_API bool GRHISupportsAtomicUInt64;

/** true if the RHI supports 64 bit uint atomics using DX12 SM6.6 - TEMP / DEPRECATED - DO NOT USE */
extern RHI_API bool GRHISupportsDX12AtomicUInt64;

/** true if the RHI supports optimal low level pipeline state sort keys. */
extern RHI_API bool GRHISupportsPipelineStateSortKey;

/** Temporary. When OpenGL is running in a separate thread, it cannot yet do things like initialize shaders that are first discovered in a rendering task. It is doable, it just isn't done. */
extern RHI_API bool GSupportsParallelRenderingTasksWithSeparateRHIThread;

/** If an RHI is so slow, that it is the limiting factor for the entire frame, we can kick early to try to give it as much as possible. */
extern RHI_API bool GRHIThreadNeedsKicking;

/** If an RHI cannot do an unlimited number of occlusion queries without stalling and waiting for the GPU, this can be used to tune hte occlusion culler to try not to do that. */
extern RHI_API int32 GRHIMaximumReccommendedOustandingOcclusionQueries;

/** Some RHIs can only do visible or not occlusion queries. */
extern RHI_API bool GRHISupportsExactOcclusionQueries;

/** True if and only if the GPU support rendering to volume textures (2D Array, 3D). Some OpenGL 3.3 cards support SM4, but can't render to volume textures. */
extern RHI_API bool GSupportsVolumeTextureRendering;

/** True if the RHI supports separate blend states per render target. */
extern RHI_API bool GSupportsSeparateRenderTargetBlendState;

/** True if the RHI has artifacts with atlased CSM depths. */
extern RHI_API bool GRHINeedsUnatlasedCSMDepthsWorkaround;

/** true if the RHI supports 3D textures */
extern RHI_API bool GSupportsTexture3D;

/** true if the RHI supports mobile multi-view */
extern RHI_API bool GSupportsMobileMultiView;

/** true if the RHI supports image external */
extern RHI_API bool GSupportsImageExternal;

/** true if the RHI supports 256bit MRT */
extern RHI_API bool GSupportsWideMRT;

/** True if the RHI and current hardware supports supports depth bounds testing */
extern RHI_API bool GSupportsDepthBoundsTest;

/** True if the RHI supports explicit access to depth target HTile meta data. */
extern RHI_API bool GRHISupportsExplicitHTile;

/** True if the RHI supports explicit access to MSAA target FMask meta data. */
extern RHI_API bool GRHISupportsExplicitFMask;

/** True if the RHI supports resummarizing depth target HTile meta data. */
extern RHI_API bool GRHISupportsResummarizeHTile;

/** True if the RHI supports depth target unordered access views. */
extern RHI_API bool GRHISupportsDepthUAV;

/** True if the RHI and current hardware supports efficient AsyncCompute (by default we assume false and later we can enable this for more hardware) */
extern RHI_API bool GSupportsEfficientAsyncCompute;

/** True if the RHI supports getting the result of occlusion queries when on a thread other than the render thread */
extern RHI_API bool GSupportsParallelOcclusionQueries;

/** true if the RHI requires a valid RT bound during UAV scatter operation inside the pixel shader */
extern RHI_API bool GRHIRequiresRenderTargetForPixelShaderUAVs;

/** true if the RHI supports unordered access view format aliasing */
extern RHI_API bool GRHISupportsUAVFormatAliasing;

/** true if the RHI supports texture views (data aliasing) */
extern RHI_API bool GRHISupportsTextureViews;

/** true if the pointer returned by Lock is a persistent direct pointer to gpu memory */
extern RHI_API bool GRHISupportsDirectGPUMemoryLock;

/** true if the multi-threaded shader creation is supported by (or desirable for) the RHI. */
extern RHI_API bool GRHISupportsMultithreadedShaderCreation;

/** Does the RHI support parallel resource commands (i.e. create / lock / unlock) on non-immediate command list APIs, recorded off the render thread. */
extern RHI_API bool GRHISupportsMultithreadedResources;

/** The minimum Z value in clip space for the RHI. This is a constant value to always match D3D clip-space. */
inline constexpr float GMinClipZ = 0.0f;

/** The sign to apply to the Y axis of projection matrices. This is a constant value to always match D3D clip-space. */
inline constexpr float GProjectionSignY = 1.0f;

/** Does this RHI need to wait for deletion of resources due to ref counting. */
extern RHI_API bool GRHINeedsExtraDeletionLatency;

/** Allow opt-out default RHI resource deletion latency for streaming textures */
extern RHI_API bool GRHIForceNoDeletionLatencyForStreamingTextures;

/** The maximum size allowed for a computeshader dispatch. */
extern RHI_API TRHIGlobal<int32> GMaxComputeDispatchDimension;

/** If true, then avoiding loading shader code and instead force the "native" path, which sends a library and a hash instead. */
extern RHI_API bool GRHILazyShaderCodeLoading;

/** If true, then it is possible to turn on GRHILazyShaderCodeLoading. */
extern RHI_API bool GRHISupportsLazyShaderCodeLoading;

/** true if the RHI supports UpdateFromBufferTexture method */
extern RHI_API bool GRHISupportsUpdateFromBufferTexture;

/** The maximum size to allow for the shadow depth buffer in the X dimension.  This must be larger or equal to GMaxShadowDepthBufferSizeY. */
extern RHI_API TRHIGlobal<int32> GMaxShadowDepthBufferSizeX;
/** The maximum size to allow for the shadow depth buffer in the Y dimension. */
extern RHI_API TRHIGlobal<int32> GMaxShadowDepthBufferSizeY;

/** The maximum size allowed for 2D textures in both dimensions. */
extern RHI_API TRHIGlobal<int32> GMaxTextureDimensions;

/** The maximum size allowed for 2D textures in both dimensions. */
extern RHI_API TRHIGlobal<int64> GMaxBufferDimensions;

/** The maximum size allowed for a contant buffer. */
extern RHI_API TRHIGlobal<int64> GRHIMaxConstantBufferByteSize;

/** The maximum size allowed for Shared Compute Memory. */
extern RHI_API TRHIGlobal<int64> GMaxComputeSharedMemory;

/** The maximum size allowed for 3D textures in all three dimensions. */
extern RHI_API TRHIGlobal<int32> GMaxVolumeTextureDimensions;

/** Whether RW texture buffers are supported */
extern RHI_API bool GRHISupportsRWTextureBuffers;

/** Whether a raw (ByteAddress) buffer view can be created for any buffer, regardless of its EBufferUsageFlags::ByteAddressBuffer flag. */
extern RHI_API bool GRHISupportsRawViewsForAnyBuffer;

/** Whether depth or stencil can individually be set to CopySrc/Dest access. */
extern RHI_API bool GRHISupportsSeparateDepthStencilCopyAccess;

/** Support using async thread for texture stream out operations */
extern RHI_API bool GRHISupportAsyncTextureStreamOut;

FORCEINLINE uint64 GetMaxBufferDimension()
{
	return GMaxBufferDimensions;
}

FORCEINLINE uint64 GetMaxConstantBufferByteSize()
{
	return GRHIMaxConstantBufferByteSize;
}

FORCEINLINE uint64 GetMaxComputeSharedMemory()
{
	return GMaxComputeSharedMemory;
}

FORCEINLINE uint32 GetMax2DTextureDimension()
{
	return GMaxTextureDimensions;
}

/** The maximum size allowed for cube textures. */
extern RHI_API TRHIGlobal<int32> GMaxCubeTextureDimensions;
FORCEINLINE uint32 GetMaxCubeTextureDimension()
{
	return GMaxCubeTextureDimensions;
}

/** The Maximum number of layers in a 1D or 2D texture array. */
extern RHI_API int32 GMaxTextureArrayLayers;
FORCEINLINE uint32 GetMaxTextureArrayLayers()
{
	return GMaxTextureArrayLayers;
}

extern RHI_API int32 GMaxTextureSamplers;
FORCEINLINE uint32 GetMaxTextureSamplers()
{
	return GMaxTextureSamplers;
}

/** The maximum work group invocations allowed for compute shader. */
extern RHI_API TRHIGlobal<int32> GMaxWorkGroupInvocations;
FORCEINLINE uint32 GetMaxWorkGroupInvocations()
{
	return GMaxWorkGroupInvocations;
}

/** true if we are running with the NULL RHI */
extern RHI_API bool GUsingNullRHI;

/**
 *	The size to check against for Draw*UP call vertex counts.
 *	If greater than this value, the draw call will not occur.
 */
extern RHI_API int32 GDrawUPVertexCheckCount;
/**
 *	The size to check against for Draw*UP call index counts.
 *	If greater than this value, the draw call will not occur.
 */
extern RHI_API int32 GDrawUPIndexCheckCount;

#include "MultiGPU.h" // IWYU pragma: export

/** Whether the next frame should profile the GPU. */
extern RHI_API bool GTriggerGPUProfile;

/** Whether we are profiling GPU hitches. */
extern RHI_API bool GTriggerGPUHitchProfile;

/** Non-empty if we are performing a gpu trace. Also says where to place trace file. */
extern RHI_API FString GGPUTraceFileName;

/** True if the RHI supports texture streaming */
extern RHI_API bool GRHISupportsTextureStreaming;
/** Amount of memory allocated by textures. In kilobytes. */
extern RHI_API volatile int32 GCurrentTextureMemorySize;
/** Amount of memory allocated by rendertargets. In kilobytes. */
extern RHI_API volatile int32 GCurrentRendertargetMemorySize;
/** Current texture streaming pool size, in bytes. 0 means unlimited. */
extern RHI_API int64 GTexturePoolSize;

/** In percent. If non-zero, the texture pool size is a percentage of GTotalGraphicsMemory. */
extern RHI_API int32 GPoolSizeVRAMPercentage;

/** Amount of local video memory demoted to system memory. In bytes. */
extern RHI_API uint64 GDemotedLocalMemorySize;

/** Whether or not the RHI can handle a non-zero BaseVertexIndex - extra SetStreamSource calls will be needed if this is false */
extern RHI_API bool GRHISupportsBaseVertexIndex;

/** True if the RHI supports copying cubemap faces using CopyToResolveTarget */
UE_DEPRECATED(5.1, "CopyToResoveTarget is deprecated.")
extern RHI_API bool GRHISupportsResolveCubemapFaces;

/** Whether or not the RHI can handle a non-zero FirstInstance to DrawIndexedPrimitive and friends - extra SetStreamSource calls will be needed if this is false */
extern RHI_API bool GRHISupportsFirstInstance;

/** Whether or not the RHI can handle dynamic resolution or not. */
extern RHI_API bool GRHISupportsDynamicResolution;

/**
* Whether or not the RHI supports ray tracing on current hardware (acceleration structure building and new ray tracing-specific shader types). 
* GRHISupportsRayTracingShaders and GRHISupportsInlineRayTracing must also be checked before dispatching ray tracing workloads.
*/
extern RHI_API bool GRHISupportsRayTracing;

/**
* Whether or not the RHI supports ray tracing raygen, miss and hit shaders (i.e. full ray tracing pipeline). 
* The RHI may support inline ray tracing from compute shaders, but not the full pipeline.
*/
extern RHI_API bool GRHISupportsRayTracingShaders;

/** Whether or not the RHI supports adding new shaders to an existing RT PSO. */
extern RHI_API bool GRHISupportsRayTracingPSOAdditions;

/** Whether or not the RHI supports indirect ray tracing dispatch commands. */
extern RHI_API bool GRHISupportsRayTracingDispatchIndirect;

/** Whether or not the RHI supports async building ray tracing acceleration structures. */
extern RHI_API bool GRHISupportsRayTracingAsyncBuildAccelerationStructure;

/** Whether or not the RHI supports the AMD Hit Token extension. */
extern RHI_API bool GRHISupportsRayTracingAMDHitToken;

/** Whether or not the RHI supports inline ray tracing in compute shaders, without a full ray tracing pipeline. */
extern RHI_API bool GRHISupportsInlineRayTracing;

/** Required alignment for ray tracing acceleration structures. */
extern RHI_API uint32 GRHIRayTracingAccelerationStructureAlignment;

/** Required alignment for ray tracing scratch buffers. */
extern RHI_API uint32 GRHIRayTracingScratchBufferAlignment;

/** Required alignment for ray tracing shader binding table buffer. */
extern RHI_API uint32 GRHIRayTracingShaderTableAlignment;

/** Size of an individual element in the ray tracing instance buffer. This defines the required stride and alignment of structured buffers of instances. */
extern RHI_API uint32 GRHIRayTracingInstanceDescriptorSize;

/** Whether or not the RHI supports shader wave operations (shader model 6.0). */
extern RHI_API bool GRHISupportsWaveOperations;

/** Whether or not the current GPU is integrated into to CPU */
extern RHI_API bool GRHIDeviceIsIntegrated;

/** 
* Specifies the minimum and maximum number of lanes in the SIMD wave that this GPU can support. I.e. 32 on NVIDIA, 64 on AMD.
* Valid values are in range [4..128] (as per SM 6.0 specification) or 0 if unknown.
* Rendering code must always check GRHISupportsWaveOperations in addition to wave min/max size.
*/
extern RHI_API int32 GRHIMinimumWaveSize;
extern RHI_API int32 GRHIMaximumWaveSize;

/** Whether or not the RHI supports an RHI thread.
Requirements for RHI thread
* Microresources (those in RHIStaticStates.h) need to be able to be created by any thread at any time and be able to work with a radically simplified rhi resource lifecycle. CreateSamplerState, CreateRasterizerState, CreateDepthStencilState, CreateBlendState
* CreateUniformBuffer needs to be threadsafe
* GetRenderQueryResult should be threadsafe, but this isn't required. If it isn't threadsafe, then you need to flush yourself in the RHI
* GetViewportBackBuffer and AdvanceFrameForGetViewportBackBuffer need to be threadsafe and need to support the fact that the render thread has a different concept of "current backbuffer" than the RHI thread. Without an RHIThread this is moot due to the next two items.
* AdvanceFrameForGetViewportBackBuffer needs be added as an RHI method and this needs to work with GetViewportBackBuffer to give the render thread the right back buffer even though many commands relating to the beginning and end of the frame are queued.
* BeginDrawingViewport, and 5 or so other frame advance methods are queued with an RHIThread. Without an RHIThread, these just flush internally.
***/
extern RHI_API bool GRHISupportsRHIThread;
/* as above, but we run the commands on arbitrary task threads */
extern RHI_API bool GRHISupportsRHIOnTaskThread;

/** Whether or not the RHI supports parallel RHIThread executes / translates
Requirements:
* RHICreateBoundShaderState & RHICreateGraphicsPipelineState is threadsafe and GetCachedBoundShaderState must not be used. GetCachedBoundShaderState_Threadsafe has a slightly different protocol.
***/
extern RHI_API bool GRHISupportsParallelRHIExecute;

/** Whether or not the RHI can perform MSAA sample load. */
extern RHI_API bool GRHISupportsMSAADepthSampleAccess;

/** Whether or not the RHI can render to the backbuffer with a custom depth/stencil surface bound. */
extern RHI_API bool GRHISupportsBackBufferWithCustomDepthStencil;

/** Whether or not HDR is currently enabled */
extern RHI_API bool GRHIIsHDREnabled;

/** Whether the present adapter/display offers HDR output capabilities. */
extern RHI_API bool GRHISupportsHDROutput;

/** Whether VRS (in all flavors) is currently enabled (separate from whether it's supported/available). */
extern RHI_API bool GRHIVariableRateShadingEnabled;

/** Whether attachment (image-based) VRS is currently enabled (separate from whether it's supported/available). */
extern RHI_API bool GRHIAttachmentVariableRateShadingEnabled;

/** The maximum number of groups that can be dispatched in each dimensions. */
extern RHI_API FIntVector GRHIMaxDispatchThreadGroupsPerDimension;

/** Whether or not the RHI can support per-draw Variable Rate Shading. */
extern RHI_API bool GRHISupportsPipelineVariableRateShading;

/** Whether or not the Variable Rate Shading can be done at larger (2x4 or 4x2 or 4x4) sizes. */
extern RHI_API bool GRHISupportsLargerVariableRateShadingSizes;

/** Whether or not the RHI can support image-based Variable Rate Shading. */
extern RHI_API bool GRHISupportsAttachmentVariableRateShading;

/** Whether or not the RHI can support complex combiner operatations between per-draw (pipeline) VRS and image VRS. */
extern RHI_API bool GRHISupportsComplexVariableRateShadingCombinerOps;

/** Whether or not the RHI can support shading rate attachments as array textures. */
extern RHI_API bool GRHISupportsVariableRateShadingAttachmentArrayTextures;

/** Maximum tile width in a screen space texture that can be used to drive Variable Rate Shading. */
extern RHI_API int32 GRHIVariableRateShadingImageTileMaxWidth;

/** Maximum tile height in a screen space texture that can be used to drive Variable Rate Shading. */
extern RHI_API int32 GRHIVariableRateShadingImageTileMaxHeight;

/** Minimum tile width in a screen space texture that can be used to drive Variable Rate Shading. */
extern RHI_API int32 GRHIVariableRateShadingImageTileMinWidth;

/** Minimum tile height in a screen space texture that can be used to drive Variable Rate Shading. */
extern RHI_API int32 GRHIVariableRateShadingImageTileMinHeight;

/** Data type contained in a shading-rate image for image-based Variable Rate Shading. */
extern RHI_API EVRSImageDataType GRHIVariableRateShadingImageDataType;

/** Image format for the shading rate image for image-based Variable Rate Shading. */
extern RHI_API EPixelFormat GRHIVariableRateShadingImageFormat;

/** Whether Variable Rate Shading deferred shading rate texture update is supported. */
extern RHI_API bool GRHISupportsLateVariableRateShadingUpdate;

/** Format used for the backbuffer when outputting to a HDR display. */
extern RHI_API EPixelFormat GRHIHDRDisplayOutputFormat;

/** Counter incremented once on each frame present. Used to support game thread synchronization with swap chain frame flips. */
extern RHI_API uint64 GRHIPresentCounter;

/** True if the RHI supports setting the render target array index from any shader stage */
extern RHI_API bool GRHISupportsArrayIndexFromAnyShader;

/** True if the pipeline file cache can be used with this RHI */
extern RHI_API bool GRHISupportsPipelineFileCache;

/** True if the RHI supports setting the stencil ref at pixel granularity from the pixel shader */
extern RHI_API bool GRHISupportsStencilRefFromPixelShader;

/** Whether current RHI supports overestimated conservative rasterization. */
extern RHI_API bool GRHISupportsConservativeRasterization;

/** true if the RHI supports Mesh and Amplification shaders with tier0 capability */
extern RHI_API bool GRHISupportsMeshShadersTier0;

/** true if the RHI supports Mesh and Amplification shaders with tier1 capability */
extern RHI_API bool GRHISupportsMeshShadersTier1;

/**
* True if the RHI supports reading system timer in shaders via GetShaderTimestamp().
* Individual shaders must be compiled with an appropriate vendor extension and check PLATFORM_SUPPORTS_SHADER_TIMESTAMP.
*/
extern RHI_API bool GRHISupportsShaderTimestamp;

extern RHI_API bool GRHISupportsEfficientUploadOnResourceCreation;

/** true if the RHI supports RLM_WriteOnly_NoOverwrite */
extern RHI_API bool GRHISupportsMapWriteNoOverwrite;

/** Tables of all MSAA sample offset for all MSAA supported. Use GetMSAASampleOffsets() to read it. */
extern RHI_API FVector2f GRHIDefaultMSAASampleOffsets[1 + 2 + 4 + 8 + 16];

/** True if the RHI supports pipeline precompiling from any thread. */
extern RHI_API bool GRHISupportsAsyncPipelinePrecompile;

/** Whether dynamic (bindless) resources are supported */
extern RHI_API ERHIBindlessSupport GRHIBindlessSupport;

UE_DEPRECATED(5.2, "You must use GRHIBindlessSupport instead.")
extern RHI_API bool GRHISupportsBindless;

// Calculate the index of the sample in GRHIDefaultMSAASampleOffsets
extern RHI_API int32 CalculateMSAASampleArrayIndex(int32 NumSamples, int32 SampleIndex);

// Gets the MSAA sample's offset from the center of the pixel coordinate.
inline FVector2f GetMSAASampleOffsets(int32 NumSamples, int32 SampleIndex)
{
	return GRHIDefaultMSAASampleOffsets[CalculateMSAASampleArrayIndex(NumSamples, SampleIndex)];
}

/** Initialize the 'best guess' pixel format capabilities. Platform formats and support must be filled out before calling this. */
extern RHI_API void RHIInitDefaultPixelFormatCapabilities();

inline bool RHIPixelFormatHasCapabilities(EPixelFormat InFormat, EPixelFormatCapabilities InCapabilities)
{
	return UE::PixelFormat::HasCapabilities(InFormat, InCapabilities);
}

inline bool RHIIsTypedUAVLoadSupported(EPixelFormat InFormat)
{
	return UE::PixelFormat::HasCapabilities(InFormat, EPixelFormatCapabilities::TypedUAVLoad);
}

inline bool RHIIsTypedUAVStoreSupported(EPixelFormat InFormat)
{
	return UE::PixelFormat::HasCapabilities(InFormat, EPixelFormatCapabilities::TypedUAVStore);
}

/**
* Returns the memory required to store an image in the given pixel format (EPixelFormat). Use
* GPixelFormats[Format].Get2D/3DImageSizeInBytes instead, unless you need PF_A1.
*/
extern RHI_API SIZE_T CalculateImageBytes(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format);


/**
 * Adjusts a projection matrix to output in the correct clip space for the
 * current RHI. Unreal projection matrices follow certain conventions and
 * need to be patched for some RHIs. All projection matrices should be adjusted
 * before being used for rendering!
 */
inline FMatrix AdjustProjectionMatrixForRHI(const FMatrix& InProjectionMatrix)
{
	FScaleMatrix ClipSpaceFixScale(FVector(1.0f, GProjectionSignY, 1.0f - GMinClipZ));
	FTranslationMatrix ClipSpaceFixTranslate(FVector(0.0f, 0.0f, GMinClipZ));	
	return InProjectionMatrix * ClipSpaceFixScale * ClipSpaceFixTranslate;
}

/** Set runtime selection of mobile feature level preview. */
RHI_API void RHISetMobilePreviewFeatureLevel(ERHIFeatureLevel::Type MobilePreviewFeatureLevel);

/** Current shader platform. */


/** Table for finding out which shader platform corresponds to a given feature level for this RHI. */
extern RHI_API EShaderPlatform GShaderPlatformForFeatureLevel[ERHIFeatureLevel::Num];

/** Get the shader platform associated with the supplied feature level on this machine */
inline EShaderPlatform GetFeatureLevelShaderPlatform(const FStaticFeatureLevel InFeatureLevel)
{
	return GShaderPlatformForFeatureLevel[InFeatureLevel];
}

struct FVertexElement
{
	uint8 StreamIndex;
	uint8 Offset;
	TEnumAsByte<EVertexElementType> Type;
	uint8 AttributeIndex;
	uint16 Stride;
	/**
	 * Whether to use instance index or vertex index to consume the element.  
	 * eg if bUseInstanceIndex is 0, the element will be repeated for every instance.
	 */
	uint16 bUseInstanceIndex;

	FVertexElement() {}
	FVertexElement(uint8 InStreamIndex,uint8 InOffset,EVertexElementType InType,uint8 InAttributeIndex,uint16 InStride,bool bInUseInstanceIndex = false):
		StreamIndex(InStreamIndex),
		Offset(InOffset),
		Type(InType),
		AttributeIndex(InAttributeIndex),
		Stride(InStride),
		bUseInstanceIndex(bInUseInstanceIndex)
	{}
	/**
	* Suppress the compiler generated assignment operator so that padding won't be copied.
	* This is necessary to get expected results for code that zeros, assigns and then CRC's the whole struct.
	*/
	void operator=(const FVertexElement& Other)
	{
		StreamIndex = Other.StreamIndex;
		Offset = Other.Offset;
		Type = Other.Type;
		AttributeIndex = Other.AttributeIndex;
		Stride = Other.Stride;
		bUseInstanceIndex = Other.bUseInstanceIndex;
	}

	bool operator==(const FVertexElement& Other) const
	{
		return (StreamIndex		== Other.StreamIndex &&
				Offset			== Other.Offset &&
				Type			== Other.Type &&
				AttributeIndex	== Other.AttributeIndex &&
				Stride			== Other.Stride &&
				bUseInstanceIndex == Other.bUseInstanceIndex);
	}

	friend FArchive& operator<<(FArchive& Ar,FVertexElement& Element)
	{
		Ar << Element.StreamIndex;
		Ar << Element.Offset;
		Ar << Element.Type;
		Ar << Element.AttributeIndex;
		Ar << Element.Stride;
		Ar << Element.bUseInstanceIndex;
		return Ar;
	}
	RHI_API FString ToString() const;
	RHI_API void FromString(const FString& Src);
	RHI_API void FromString(const FStringView& Src);
};

typedef TArray<FVertexElement,TFixedAllocator<MaxVertexElementCount> > FVertexDeclarationElementList;

/** RHI representation of a single stream out element. */
//#todo-RemoveStreamOut
struct FStreamOutElement
{
	/** Index of the output stream from the geometry shader. */
	uint32 Stream;

	/** Semantic name of the output element as defined in the geometry shader.  This should not contain the semantic number. */
	const ANSICHAR* SemanticName;

	/** Semantic index of the output element as defined in the geometry shader.  For example "TEXCOORD5" in the shader would give a SemanticIndex of 5. */
	uint32 SemanticIndex;

	/** Start component index of the shader output element to stream out. */
	uint8 StartComponent;

	/** Number of components of the shader output element to stream out. */
	uint8 ComponentCount;

	/** Stream output target slot, corresponding to the streams set by RHISetStreamOutTargets. */
	uint8 OutputSlot;

	FStreamOutElement() {}
	FStreamOutElement(uint32 InStream, const ANSICHAR* InSemanticName, uint32 InSemanticIndex, uint8 InComponentCount, uint8 InOutputSlot) :
		Stream(InStream),
		SemanticName(InSemanticName),
		SemanticIndex(InSemanticIndex),
		StartComponent(0),
		ComponentCount(InComponentCount),
		OutputSlot(InOutputSlot)
	{}
};

//#todo-RemoveStreamOut
typedef TArray<FStreamOutElement,TFixedAllocator<MaxVertexElementCount> > FStreamOutElementList;

struct FSamplerStateInitializerRHI
{
	FSamplerStateInitializerRHI() {}
	FSamplerStateInitializerRHI(
		ESamplerFilter InFilter,
		ESamplerAddressMode InAddressU = AM_Wrap,
		ESamplerAddressMode InAddressV = AM_Wrap,
		ESamplerAddressMode InAddressW = AM_Wrap,
		float InMipBias = 0,
		int32 InMaxAnisotropy = 0,
		float InMinMipLevel = 0,
		float InMaxMipLevel = FLT_MAX,
		uint32 InBorderColor = 0,
		/** Only supported in D3D11 */
		ESamplerCompareFunction InSamplerComparisonFunction = SCF_Never
		)
	:	Filter(InFilter)
	,	AddressU(InAddressU)
	,	AddressV(InAddressV)
	,	AddressW(InAddressW)
	,	MipBias(InMipBias)
	,	MinMipLevel(InMinMipLevel)
	,	MaxMipLevel(InMaxMipLevel)
	,	MaxAnisotropy(InMaxAnisotropy)
	,	BorderColor(InBorderColor)
	,	SamplerComparisonFunction(InSamplerComparisonFunction)
	{
	}
	TEnumAsByte<ESamplerFilter> Filter = SF_Point;
	TEnumAsByte<ESamplerAddressMode> AddressU = AM_Wrap;
	TEnumAsByte<ESamplerAddressMode> AddressV = AM_Wrap;
	TEnumAsByte<ESamplerAddressMode> AddressW = AM_Wrap;
	float MipBias = 0.0f;
	/** Smallest mip map level that will be used, where 0 is the highest resolution mip level. */
	float MinMipLevel = 0.0f;
	/** Largest mip map level that will be used, where 0 is the highest resolution mip level. */
	float MaxMipLevel = FLT_MAX;
	int32 MaxAnisotropy = 0;
	uint32 BorderColor = 0;
	TEnumAsByte<ESamplerCompareFunction> SamplerComparisonFunction = SCF_Never;


	RHI_API friend uint32 GetTypeHash(const FSamplerStateInitializerRHI& Initializer);
	RHI_API friend bool operator== (const FSamplerStateInitializerRHI& A, const FSamplerStateInitializerRHI& B);
};

struct FRasterizerStateInitializerRHI
{
	TEnumAsByte<ERasterizerFillMode> FillMode = FM_Point;
	TEnumAsByte<ERasterizerCullMode> CullMode = CM_None;
	float DepthBias = 0.0f;
	float SlopeScaleDepthBias = 0.0f;
	ERasterizerDepthClipMode DepthClipMode = ERasterizerDepthClipMode::DepthClip;
	bool bAllowMSAA = false;
	bool bEnableLineAA = false;

	FRasterizerStateInitializerRHI() = default;

	FRasterizerStateInitializerRHI(ERasterizerFillMode InFillMode, ERasterizerCullMode InCullMode, bool bInAllowMSAA, bool bInEnableLineAA)
		: FillMode(InFillMode)
		, CullMode(InCullMode)
		, bAllowMSAA(bInAllowMSAA)
		, bEnableLineAA(bInEnableLineAA)
	{
	}

	FRasterizerStateInitializerRHI(ERasterizerFillMode InFillMode, ERasterizerCullMode InCullMode, float InDepthBias, float InSlopeScaleDepthBias, ERasterizerDepthClipMode InDepthClipMode, bool bInAllowMSAA, bool bInEnableLineAA)
		: FillMode(InFillMode)
		, CullMode(InCullMode)
		, DepthBias(InDepthBias)
		, SlopeScaleDepthBias(InSlopeScaleDepthBias)
		, DepthClipMode(InDepthClipMode)
		, bAllowMSAA(bInAllowMSAA)
		, bEnableLineAA(bInEnableLineAA)
	{
	}

	friend FArchive& operator<<(FArchive& Ar,FRasterizerStateInitializerRHI& RasterizerStateInitializer)
	{
		Ar << RasterizerStateInitializer.FillMode;
		Ar << RasterizerStateInitializer.CullMode;
		Ar << RasterizerStateInitializer.DepthBias;
		Ar << RasterizerStateInitializer.SlopeScaleDepthBias;
		Ar << RasterizerStateInitializer.DepthClipMode;
		Ar << RasterizerStateInitializer.bAllowMSAA;
		Ar << RasterizerStateInitializer.bEnableLineAA;
		return Ar;
	}

	RHI_API friend uint32 GetTypeHash(const FRasterizerStateInitializerRHI& Initializer);
	RHI_API friend bool operator== (const FRasterizerStateInitializerRHI& A, const FRasterizerStateInitializerRHI& B);
};

struct FDepthStencilStateInitializerRHI
{
	bool bEnableDepthWrite;
	TEnumAsByte<ECompareFunction> DepthTest;

	bool bEnableFrontFaceStencil;
	TEnumAsByte<ECompareFunction> FrontFaceStencilTest;
	TEnumAsByte<EStencilOp> FrontFaceStencilFailStencilOp;
	TEnumAsByte<EStencilOp> FrontFaceDepthFailStencilOp;
	TEnumAsByte<EStencilOp> FrontFacePassStencilOp;
	bool bEnableBackFaceStencil;
	TEnumAsByte<ECompareFunction> BackFaceStencilTest;
	TEnumAsByte<EStencilOp> BackFaceStencilFailStencilOp;
	TEnumAsByte<EStencilOp> BackFaceDepthFailStencilOp;
	TEnumAsByte<EStencilOp> BackFacePassStencilOp;
	uint8 StencilReadMask;
	uint8 StencilWriteMask;

	FDepthStencilStateInitializerRHI(
		bool bInEnableDepthWrite = true,
		ECompareFunction InDepthTest = CF_LessEqual,
		bool bInEnableFrontFaceStencil = false,
		ECompareFunction InFrontFaceStencilTest = CF_Always,
		EStencilOp InFrontFaceStencilFailStencilOp = SO_Keep,
		EStencilOp InFrontFaceDepthFailStencilOp = SO_Keep,
		EStencilOp InFrontFacePassStencilOp = SO_Keep,
		bool bInEnableBackFaceStencil = false,
		ECompareFunction InBackFaceStencilTest = CF_Always,
		EStencilOp InBackFaceStencilFailStencilOp = SO_Keep,
		EStencilOp InBackFaceDepthFailStencilOp = SO_Keep,
		EStencilOp InBackFacePassStencilOp = SO_Keep,
		uint8 InStencilReadMask = 0xFF,
		uint8 InStencilWriteMask = 0xFF
		)
	: bEnableDepthWrite(bInEnableDepthWrite)
	, DepthTest(InDepthTest)
	, bEnableFrontFaceStencil(bInEnableFrontFaceStencil)
	, FrontFaceStencilTest(InFrontFaceStencilTest)
	, FrontFaceStencilFailStencilOp(InFrontFaceStencilFailStencilOp)
	, FrontFaceDepthFailStencilOp(InFrontFaceDepthFailStencilOp)
	, FrontFacePassStencilOp(InFrontFacePassStencilOp)
	, bEnableBackFaceStencil(bInEnableBackFaceStencil)
	, BackFaceStencilTest(InBackFaceStencilTest)
	, BackFaceStencilFailStencilOp(InBackFaceStencilFailStencilOp)
	, BackFaceDepthFailStencilOp(InBackFaceDepthFailStencilOp)
	, BackFacePassStencilOp(InBackFacePassStencilOp)
	, StencilReadMask(InStencilReadMask)
	, StencilWriteMask(InStencilWriteMask)
	{}
	
	friend FArchive& operator<<(FArchive& Ar,FDepthStencilStateInitializerRHI& DepthStencilStateInitializer)
	{
		Ar << DepthStencilStateInitializer.bEnableDepthWrite;
		Ar << DepthStencilStateInitializer.DepthTest;
		Ar << DepthStencilStateInitializer.bEnableFrontFaceStencil;
		Ar << DepthStencilStateInitializer.FrontFaceStencilTest;
		Ar << DepthStencilStateInitializer.FrontFaceStencilFailStencilOp;
		Ar << DepthStencilStateInitializer.FrontFaceDepthFailStencilOp;
		Ar << DepthStencilStateInitializer.FrontFacePassStencilOp;
		Ar << DepthStencilStateInitializer.bEnableBackFaceStencil;
		Ar << DepthStencilStateInitializer.BackFaceStencilTest;
		Ar << DepthStencilStateInitializer.BackFaceStencilFailStencilOp;
		Ar << DepthStencilStateInitializer.BackFaceDepthFailStencilOp;
		Ar << DepthStencilStateInitializer.BackFacePassStencilOp;
		Ar << DepthStencilStateInitializer.StencilReadMask;
		Ar << DepthStencilStateInitializer.StencilWriteMask;
		return Ar;
	}

	RHI_API friend uint32 GetTypeHash(const FDepthStencilStateInitializerRHI& Initializer);
	RHI_API friend bool operator== (const FDepthStencilStateInitializerRHI& A, const FDepthStencilStateInitializerRHI& B);
	
	RHI_API FString ToString() const;
	RHI_API void FromString(const FString& Src);
	RHI_API void FromString(const FStringView& Src);
};

class FBlendStateInitializerRHI
{
public:

	struct FRenderTarget
	{
		enum
		{
			NUM_STRING_FIELDS = 7
		};
		TEnumAsByte<EBlendOperation> ColorBlendOp;
		TEnumAsByte<EBlendFactor> ColorSrcBlend;
		TEnumAsByte<EBlendFactor> ColorDestBlend;
		TEnumAsByte<EBlendOperation> AlphaBlendOp;
		TEnumAsByte<EBlendFactor> AlphaSrcBlend;
		TEnumAsByte<EBlendFactor> AlphaDestBlend;
		TEnumAsByte<EColorWriteMask> ColorWriteMask;
		
		FRenderTarget(
			EBlendOperation InColorBlendOp = BO_Add,
			EBlendFactor InColorSrcBlend = BF_One,
			EBlendFactor InColorDestBlend = BF_Zero,
			EBlendOperation InAlphaBlendOp = BO_Add,
			EBlendFactor InAlphaSrcBlend = BF_One,
			EBlendFactor InAlphaDestBlend = BF_Zero,
			EColorWriteMask InColorWriteMask = CW_RGBA
			)
		: ColorBlendOp(InColorBlendOp)
		, ColorSrcBlend(InColorSrcBlend)
		, ColorDestBlend(InColorDestBlend)
		, AlphaBlendOp(InAlphaBlendOp)
		, AlphaSrcBlend(InAlphaSrcBlend)
		, AlphaDestBlend(InAlphaDestBlend)
		, ColorWriteMask(InColorWriteMask)
		{}
		
		friend FArchive& operator<<(FArchive& Ar,FRenderTarget& RenderTarget)
		{
			Ar << RenderTarget.ColorBlendOp;
			Ar << RenderTarget.ColorSrcBlend;
			Ar << RenderTarget.ColorDestBlend;
			Ar << RenderTarget.AlphaBlendOp;
			Ar << RenderTarget.AlphaSrcBlend;
			Ar << RenderTarget.AlphaDestBlend;
			Ar << RenderTarget.ColorWriteMask;
			return Ar;
		}
		
		RHI_API FString ToString() const;
		RHI_API void FromString(const TArray<FString>& Parts, int32 Index);
		RHI_API void FromString(TArrayView<const FStringView> Parts);
	};

	FBlendStateInitializerRHI() {}

	FBlendStateInitializerRHI(const FRenderTarget& InRenderTargetBlendState, bool bInUseAlphaToCoverage = false)
	:	bUseIndependentRenderTargetBlendStates(false)
	,	bUseAlphaToCoverage(bInUseAlphaToCoverage)
	{
		RenderTargets[0] = InRenderTargetBlendState;
	}

	template<uint32 NumRenderTargets>
	FBlendStateInitializerRHI(const TStaticArray<FRenderTarget,NumRenderTargets>& InRenderTargetBlendStates, bool bInUseAlphaToCoverage = false)
	:	bUseIndependentRenderTargetBlendStates(NumRenderTargets > 1)
	,	bUseAlphaToCoverage(bInUseAlphaToCoverage)
	{
		static_assert(NumRenderTargets <= MaxSimultaneousRenderTargets, "Too many render target blend states.");

		for(uint32 RenderTargetIndex = 0;RenderTargetIndex < NumRenderTargets;++RenderTargetIndex)
		{
			RenderTargets[RenderTargetIndex] = InRenderTargetBlendStates[RenderTargetIndex];
		}
	}

	TStaticArray<FRenderTarget,MaxSimultaneousRenderTargets> RenderTargets;
	bool bUseIndependentRenderTargetBlendStates;
	bool bUseAlphaToCoverage;
	
	friend FArchive& operator<<(FArchive& Ar,FBlendStateInitializerRHI& BlendStateInitializer)
	{
		Ar << BlendStateInitializer.RenderTargets;
		Ar << BlendStateInitializer.bUseIndependentRenderTargetBlendStates;
		Ar << BlendStateInitializer.bUseAlphaToCoverage;
		return Ar;
	}

	RHI_API friend uint32 GetTypeHash(const FBlendStateInitializerRHI::FRenderTarget& RenderTarget);
	RHI_API friend bool operator== (const FBlendStateInitializerRHI::FRenderTarget& A, const FBlendStateInitializerRHI::FRenderTarget& B);
	
	RHI_API friend uint32 GetTypeHash(const FBlendStateInitializerRHI& Initializer);
	RHI_API friend bool operator== (const FBlendStateInitializerRHI& A, const FBlendStateInitializerRHI& B);
	
	RHI_API FString ToString() const;
	RHI_API void FromString(const FString& Src);
	RHI_API void FromString(const FStringView& Src);
};



/**
 *	Viewport bounds structure to set multiple view ports for the geometry shader
 *  (needs to be 1:1 to the D3D11 structure)
 */
struct FViewportBounds
{
	float	TopLeftX;
	float	TopLeftY;
	float	Width;
	float	Height;
	float	MinDepth;
	float	MaxDepth;

	FViewportBounds() {}

	FViewportBounds(float InTopLeftX, float InTopLeftY, float InWidth, float InHeight, float InMinDepth = 0.0f, float InMaxDepth = 1.0f)
		:TopLeftX(InTopLeftX), TopLeftY(InTopLeftY), Width(InWidth), Height(InHeight), MinDepth(InMinDepth), MaxDepth(InMaxDepth)
	{
	}
};



struct FVRamAllocation
{
	FVRamAllocation() = default;
	FVRamAllocation(uint64 InAllocationStart, uint64 InAllocationSize)
		: AllocationStart(InAllocationStart)
		, AllocationSize(InAllocationSize)
	{
	}

	bool IsValid() const { return AllocationSize > 0; }

	// in bytes
	uint64 AllocationStart{};
	// in bytes
	uint64 AllocationSize{};
};

struct FRHIResourceInfo
{
	FName Name;
	ERHIResourceType Type{ RRT_None };
	FVRamAllocation VRamAllocation;
	bool IsTransient{ false };
	bool bValid{ true };
	bool bResident{ true };
};

struct FRHIDispatchIndirectParametersNoPadding
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;
};

struct FRHIDispatchIndirectParameters : public FRHIDispatchIndirectParametersNoPadding
{
#if PLATFORM_DISPATCH_INDIRECT_ARGUMENT_BOUNDARY_SIZE == 64
	uint32 Padding;			// pad to 32 bytes to prevent crossing of 64 byte boundary in ExecuteIndirect calls
#elif PLATFORM_DISPATCH_INDIRECT_ARGUMENT_BOUNDARY_SIZE != 0
	#error FRHIDispatchIndirectParameters does not account for PLATFORM_DISPATCH_INDIRECT_ARGUMENT_BOUNDARY_SIZE.
#endif
};
static_assert(PLATFORM_DISPATCH_INDIRECT_ARGUMENT_BOUNDARY_SIZE == 0 || PLATFORM_DISPATCH_INDIRECT_ARGUMENT_BOUNDARY_SIZE % sizeof(FRHIDispatchIndirectParameters) == 0);

struct FRHIDrawIndirectParameters
{
	uint32 VertexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartVertexLocation;
	uint32 StartInstanceLocation;
};

struct FRHIDrawIndexedIndirectParameters
{
	uint32 IndexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartIndexLocation;
	int32 BaseVertexLocation;
	uint32 StartInstanceLocation;
};

// RHI base resource types.
#include "RHIResources.h" // IWYU pragma: keep
#include "DynamicRHI.h" // IWYU pragma: keep

/** Initializes the RHI. */
extern RHI_API void RHIInit(bool bHasEditorToken);

/** Performs additional RHI initialization before the render thread starts. */
extern RHI_API void RHIPostInit(const TArray<uint32>& InPixelFormatByteWidth);

/** Shuts down the RHI. */
extern RHI_API void RHIExit();


// Panic delegate is called when when a fatal condition is encountered within RHI function.
DECLARE_DELEGATE_OneParam(FRHIPanicEvent, const FName&);
extern RHI_API FRHIPanicEvent& RHIGetPanicDelegate();

// RHI utility functions that depend on the RHI definitions.
#include "RHIUtilities.h" // IWYU pragma: keep

// Return what the expected number of samplers will be supported by a feature level
// Note that since the Feature Level is pretty orthogonal to the RHI/HW, this is not going to be perfect
// If should only be used for a guess at the limit, the real limit will not be known until runtime
inline uint32 GetExpectedFeatureLevelMaxTextureSamplers(const FStaticFeatureLevel FeatureLevel)
{
	return 16;
}

RHI_API ERHIBindlessConfiguration RHIGetBindlessResourcesConfiguration(EShaderPlatform Platform);
RHI_API ERHIBindlessConfiguration RHIGetBindlessSamplersConfiguration(EShaderPlatform Platform);

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIStrings.h"
#endif
