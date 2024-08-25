// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLDrv.h: Public OpenGL RHI definitions.
=============================================================================*/

#pragma once

// Dependencies.

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "IOpenGLDynamicRHI.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "GPUProfiler.h"
#include "RenderResource.h"
#include "Templates/EnableIf.h"
#include "BoundShaderStateHistory.h"

// @todo platplug: Replace all of these includes with a call to COMPILED_PLATFORM_HEADER(OpenGLDrvPrivate.h)
//TODO: Move these to OpenGLDrvPrivate.h
#if PLATFORM_WINDOWS
	#include "Windows/OpenGLWindows.h"
#elif PLATFORM_LINUX
	#include "Linux/OpenGLLinux.h"
#elif PLATFORM_ANDROID
	#include "Android/AndroidOpenGL.h"
#else
#include COMPILED_PLATFORM_HEADER(OpenGLDrvPrivate.h)
#endif

// Define here so don't have to do platform filtering
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

// OpenGL RHI public headers.
#include "OpenGLUtil.h"
#include "OpenGLState.h"
#include "RenderUtils.h"

#define FOpenGLCachedUniformBuffer_Invalid 0xFFFFFFFF

class FOpenGLDynamicRHI;
class FResourceBulkDataInterface;
struct FOpenGLResourceBinder;
struct Rect;

template<class T> struct TOpenGLResourceTraits;

// This class has multiple inheritance but really FGPUTiming is a static class
class FOpenGLBufferedGPUTiming : public FGPUTiming
{
public:

	/**
	 * Constructor.
	 *
	 * @param InOpenGLRHI			RHI interface
	 * @param InBufferSize		Number of buffered measurements
	 */
	FOpenGLBufferedGPUTiming(class FOpenGLDynamicRHI* InOpenGLRHI, int32 BufferSize);

	void	StartTiming();

	/**
	 * End a GPU timing measurement.
	 * The timing for this particular measurement will be resolved at a later time by the GPU.
	 */
	void	EndTiming();

	/**
	 * Retrieves the most recently resolved timing measurement.
	 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
	 *
	 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	 */
	uint64	GetTiming(bool bGetCurrentResultsAndBlock = false);

	void InitResources();
	void ReleaseResources();


private:

	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	/** RHI interface */
	FOpenGLDynamicRHI*					OpenGLRHI;
	/** Number of timestamps created in 'StartTimestamps' and 'EndTimestamps'. */
	int32								BufferSize;
	/** Current timing being measured on the CPU. */
	int32								CurrentTimestamp;
	/** Number of measurements in the buffers (0 - BufferSize). */
	int32								NumIssuedTimestamps;
	/** Timestamps for all StartTimings. */
	TArray<FOpenGLRenderQuery *>		StartTimestamps;
	/** Timestamps for all EndTimings. */
	TArray<FOpenGLRenderQuery *>		EndTimestamps;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool								bIsTiming;
};

/**
  * Used to track whether a period was disjoint on the GPU, which means GPU timings are invalid.
  * OpenGL lacks this concept at present, so the class is just a placeholder
  * Timings are all assumed to be non-disjoint
  */
class FOpenGLDisjointTimeStampQuery
{
public:
	FOpenGLDisjointTimeStampQuery(class FOpenGLDynamicRHI* InOpenGLRHI=NULL);

	void Init(class FOpenGLDynamicRHI* InOpenGLRHI)
	{
		OpenGLRHI = InOpenGLRHI;
		InitResources();
	}

	void StartTracking();
	void EndTracking();
	bool IsResultValid();
	bool GetResult(uint64* OutResult=NULL);
	static uint64 GetTimingFrequency()
	{
		return 1000000000ull;
	}
	static bool IsSupported()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return FOpenGL::SupportsDisjointTimeQueries();
#endif
	}

	void InitResources();
	void ReleaseResources();


private:
	bool	bIsResultValid;
	GLuint	DisjointQuery;
	uint64	Context;

	FOpenGLDynamicRHI* OpenGLRHI;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FOpenGLEventNode : public FGPUProfilerEventNode
{
public:

	FOpenGLEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent, class FOpenGLDynamicRHI* InRHI)
	:	FGPUProfilerEventNode(InName, InParent)
	,	Timing(InRHI, 1)
	{
		// Initialize Buffered timestamp queries 
		Timing.InitResources();
	}

	virtual ~FOpenGLEventNode()
	{
		Timing.ReleaseResources();
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	float GetTiming() override;

	virtual void StartTiming() override
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override
	{
		Timing.EndTiming();
	}

	FOpenGLBufferedGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FOpenGLEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:
	FOpenGLEventNodeFrame(class FOpenGLDynamicRHI* InRHI) :
		FGPUProfilerEventNodeFrame(),
		RootEventTiming(InRHI, 1),
		DisjointQuery(InRHI)
	{
	  RootEventTiming.InitResources();
	  DisjointQuery.InitResources();
	}

	~FOpenGLEventNodeFrame()
	{

		RootEventTiming.ReleaseResources();
		DisjointQuery.ReleaseResources();
	}

	/** Start this frame of per tracking */
	void StartFrame() override;

	/** End this frame of per tracking, but do not block yet */
	void EndFrame() override;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;

	virtual void LogDisjointQuery() override;

	/** Timer tracking inclusive time spent in the root nodes. */
	FOpenGLBufferedGPUTiming RootEventTiming;

	/** Disjoint query tracking whether the times reported by DumpEventTree are reliable. */
	FOpenGLDisjointTimeStampQuery DisjointQuery;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FOpenGLGPUProfiler : public FGPUProfiler
{
	/** Used to measure GPU time per frame. */
	FOpenGLBufferedGPUTiming FrameTiming;

	/** Measuring GPU frame time with a disjoint query. */
	static const int MAX_GPUFRAMEQUERIES = 4;
	FOpenGLDisjointTimeStampQuery DisjointGPUFrameTimeQuery[MAX_GPUFRAMEQUERIES];
	int CurrentGPUFrameQueryIndex;

	class FOpenGLDynamicRHI* OpenGLRHI;
	// count the number of beginframe calls without matching endframe calls.
	int32 NestedFrameCount;

	uint32 ExternalGPUTime;

	/** GPU hitch profile histories */
	TIndirectArray<FOpenGLEventNodeFrame> GPUHitchEventNodeFrames;

	FOpenGLGPUProfiler(class FOpenGLDynamicRHI* InOpenGLRHI)
	:	FGPUProfiler()
	,	FrameTiming(InOpenGLRHI, 4)
	,	CurrentGPUFrameQueryIndex(0)
	,	OpenGLRHI(InOpenGLRHI)
	,	NestedFrameCount(0)
	,	ExternalGPUTime(0)
	{
		FrameTiming.InitResources();
		for (int32 Index = 0; Index < MAX_GPUFRAMEQUERIES; ++Index)
		{
			DisjointGPUFrameTimeQuery[Index].Init(OpenGLRHI);
		}
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override
	{
		FOpenGLEventNode* EventNode = new FOpenGLEventNode(InName, InParent, OpenGLRHI);
		return EventNode;
	}

	void Cleanup();

	virtual void PushEvent(const TCHAR* Name, FColor Color) override;
	virtual void PopEvent() override;

	void BeginFrame(class FOpenGLDynamicRHI* InRHI);
	void EndFrame();
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/** The interface which is implemented by the dynamically bound RHI. */
class OPENGLDRV_API FOpenGLDynamicRHI final : public IOpenGLDynamicRHI, public IRHICommandContextPSOFallback
{
	static inline FOpenGLDynamicRHI* Singleton = nullptr;

public:
	static inline FOpenGLDynamicRHI& Get() { return *Singleton; }

	friend class FOpenGLViewport;

	/** Initialization constructor. */
	FOpenGLDynamicRHI();

	/** Destructor */
	~FOpenGLDynamicRHI() {}

	// IOpenGLDynamicRHI interface.
	virtual int32 RHIGetGLMajorVersion() const final override;
	virtual int32 RHIGetGLMinorVersion() const final override;
	virtual bool RHISupportsFramebufferSRGBEnable() const final override;
	virtual FTexture2DRHIRef RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags Flags) final override;
	virtual FTexture2DRHIRef RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags Flags) final override;
	virtual FTextureCubeRHIRef RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags Flags) final override;
	virtual GLuint RHIGetResource(FRHITexture* InTexture) const final override;
	virtual bool RHIIsValidTexture(GLuint InTexture) const final override;
	virtual void RHISetExternalGPUTime(uint32 InExternalGPUTime) final override;

#if PLATFORM_ANDROID
	virtual EGLDisplay RHIGetEGLDisplay() const final override;
	virtual EGLSurface RHIGetEGLSurface() const final override;
	virtual EGLConfig  RHIGetEGLConfig() const final override;
	virtual EGLContext RHIGetEGLContext() const final override;
	virtual ANativeWindow* RHIGetEGLNativeWindow() const final override;
	virtual bool RHIEGLSupportsNoErrorContext() const final override;
	virtual void RHIInitEGLInstanceGLES2() final override;
	virtual void RHIInitEGLBackBuffer() final override;
	virtual void RHIEGLSetCurrentRenderingContext() final override;
	virtual void RHIEGLTerminateContext() final override;
#endif

	// FDynamicRHI interface.
	virtual void Init() override;

	virtual void Shutdown() override;
	virtual const TCHAR* GetName() override { return TEXT("OpenGL"); }

	template<typename TRHIType>
	static auto* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TOpenGLResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}

	static FOpenGLTexture* ResourceCast(FRHITexture* TextureRHI)
	{
		if (!TextureRHI)
		{
			return nullptr;
		}
		else
		{
			return static_cast<FOpenGLTexture*>(TextureRHI->GetTextureBaseRHI());
		}
	}

	void BindUniformBuffer(EShaderFrequency ShaderFrequency, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI);

	void SetShaderParametersCommon(EShaderFrequency ShaderFrequency, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters);
	void SetShaderUnbindsCommon(EShaderFrequency ShaderFrequency, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds);

	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) final override;
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) final override;
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) final override;
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) final override;

	virtual FPixelShaderRHIRef RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FVertexShaderRHIRef RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FGeometryShaderRHIRef RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FComputeShaderRHIRef RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;

	// SRV / UAV creation functions
	virtual FShaderResourceViewRHIRef  RHICreateShaderResourceView (class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc) final override;

	virtual FUniformBufferRHIRef RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation) final override;
	virtual void RHIUpdateUniformBuffer(FRHICommandListBase& RHICmdList, FRHIUniformBuffer* UniformBufferRHI, const void* Contents) final override;
	virtual FBufferRHIRef RHICreateBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& Desc, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void RHICopyBuffer(FRHIBuffer* SourceBuffer, FRHIBuffer* DestBuffer) final override;
	virtual void RHITransferBufferUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer) final override;
	virtual void* LockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode) final override;
	virtual void UnlockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer) final override;
	virtual FRHICalcTextureSizeResult RHICalcTexturePlatformSize(FRHITextureDesc const& Desc, uint32 FirstMipIndex) final override;
	virtual void RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats) final override;
	virtual bool RHIGetTextureMemoryVisualizeData(FColor* TextureData,int32 SizeX,int32 SizeY,int32 Pitch,int32 PixelSize) final override;
	virtual FTextureRHIRef RHICreateTexture(FRHICommandListBase& RHICmdList, const FRHITextureCreateDesc& CreateDesc) final override;
	virtual FTextureRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess InResourceState, void** InitialMipData, uint32 NumInitialMips, const TCHAR* DebugName, FGraphEventRef& OutCompletionEvent) final override;
	virtual void RHIGenerateMips(FRHITexture* Texture) final override;
	virtual uint32 RHIComputeMemorySize(FRHITexture* TextureRHI) final override;
	virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D(FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) final override;
	virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual void* RHILockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, uint64* OutLockedByteCount = nullptr) final override;
	virtual void RHIUnlockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void* RHILockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIUpdateTexture2D(FRHICommandListBase& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData) final override;
	virtual void RHIUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData) final override;
	virtual void* RHILockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHITexture* Texture, const TCHAR* Name) final override;
	virtual void RHIReadSurfaceData(FRHITexture* Texture,FIntRect Rect,TArray<FColor>& OutData,FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIReadSurfaceData(FRHITexture* Texture, FIntRect Rect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIMapStagingSurface(FRHITexture* Texture, FRHIGPUFence* Fence, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex = 0) final override;
	virtual void RHIUnmapStagingSurface(FRHITexture* Texture, uint32 GPUIndex = 0) final override;
	virtual void RHIReadSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,TArray<FFloat16Color>& OutData,ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex) final override;
	virtual void RHIRead3DSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData) final override;
	virtual FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType) final override;
	virtual bool RHIGetRenderQueryResult(FRHIRenderQuery* RenderQuery, uint64& OutResult, bool bWait, uint32 GPUIndex = INDEX_NONE) final override;
	virtual FTexture2DRHIRef RHIGetViewportBackBuffer(FRHIViewport* Viewport) final override;
	virtual void RHIAliasTextureResources(FTextureRHIRef& DestTextureRHI, FTextureRHIRef& SrcTextureRHI) final override;
	virtual FTextureRHIRef RHICreateAliasedTexture(FTextureRHIRef& SourceTexture) final override;
	virtual void RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* Viewport) final override;
	virtual void RHIAcquireThreadOwnership() final override;
	virtual void RHIReleaseThreadOwnership() final override;
	virtual void RHIFlushResources() final override;
	virtual uint32 RHIGetGPUFrameCycles(uint32 GPUIndex = 0) final override;
	virtual FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
	virtual void RHIResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen) final override;
	virtual EPixelFormat RHIPreferredPixelFormatHint(EPixelFormat PreferredPixelFormat) final override;
	virtual void RHITick(float DeltaTime) final override;
	virtual void RHIBlockUntilGPUIdle() final override;
	virtual void RHISubmitCommandsAndFlushGPU() final override;
	virtual void RHIPollOcclusionQueries() final override;
	virtual bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate) final override;
	virtual void RHIGetSupportedResolution(uint32& Width, uint32& Height) final override;
	virtual void* RHIGetNativeDevice() final override;
	virtual void* RHIGetNativeInstance() final override;
	virtual class IRHICommandContext* RHIGetDefaultContext() final override;
	virtual IRHIComputeContext* RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask) final override;
	virtual IRHIPlatformCommandList* RHIFinalizeContext(IRHIComputeContext* Context) final override;
	virtual void RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists, bool bFlushResources) final override;

	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName) final override;
	virtual void RHIEndRenderPass() final override;
	virtual void RHINextSubpass() final override;
	
	virtual void RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions) override final;
	virtual void RHIEndTransitions(TArrayView<const FRHITransition*> Transitions) override final;

	virtual void RHISetComputeShader(FRHIComputeShader* ComputeShader) final override;
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	virtual void RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	virtual void RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values) final override;
	virtual void RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values) final override;
	virtual void RHIBeginRenderQuery(FRHIRenderQuery* RenderQuery) final override;
	virtual void RHIEndRenderQuery(FRHIRenderQuery* RenderQuery) final override;
	virtual void RHISubmitCommandsHint() final override;
	virtual void RHIBeginDrawingViewport(FRHIViewport* Viewport, FRHITexture* RenderTargetRHI) final override;
	virtual void RHIEndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync) final override;
	using FDynamicRHI::RHIBeginFrame;
	virtual void RHIBeginFrame() final override;
	virtual void RHIEndFrame() final override;
	virtual void RHIBeginScene() final override;
	virtual void RHIEndScene() final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset) final override;
	virtual void RHISetRasterizerState(FRHIRasterizerState* NewState) final override;
	virtual void RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) final override;
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	virtual void RHISetBoundShaderState(FRHIBoundShaderState* BoundShaderState) final override;
	virtual void RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers) final override;
	virtual void RHISetStaticUniformBuffer(FUniformBufferStaticSlot Slot, FRHIUniformBuffer* Buffer) final override;
	virtual void RHISetUniformBufferDynamicOffset(FUniformBufferStaticSlot Slot, uint32 Offset) final override;
	virtual void RHISetShaderParameters(FRHIGraphicsShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override;
	virtual void RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override;
	virtual void RHISetShaderUnbinds(FRHIComputeShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds) final override;
	virtual void RHISetShaderUnbinds(FRHIGraphicsShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds) final override;
	virtual void RHISetDepthStencilState(FRHIDepthStencilState* NewState, uint32 StencilRef) final override;
	virtual void RHISetStencilRef(uint32 StencilRef) final override;
	virtual void RHISetBlendState(FRHIBlendState* NewState, const FLinearColor& BlendFactor) final override;
	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) final override
	{
		// Currently ignored as well as on RHISetBlendState()...
	}

	void SetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget);
	void SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo);

	virtual void RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIEnableDepthBoundsTest(bool bEnable) final override;
	virtual void RHISetDepthBounds(float MinDepth, float MaxDepth) final override;
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override;
	virtual void RHIPopEvent() final override;
	virtual void RHIInvalidateCachedState() final override;
	virtual void RHIDiscardRenderTargets(bool Depth,bool Stencil,uint32 ColorBitMask) final override;

	// FIXME: Broken on Android for cubemaps
	virtual void RHICopyTexture(FRHITexture* SourceTexture, FRHITexture* DestTexture, const FRHICopyTextureInfo& CopyInfo) final override;

	virtual void RHICopyBufferRegion(FRHIBuffer* DestBuffer, uint64 DstOffset, FRHIBuffer* SourceBuffer, uint64 SrcOffset, uint64 NumBytes) final override;

	// Inline copy
	virtual void RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* DestinationStagingBufferRHI, uint32 InOffset, uint32 InNumBytes) final override;
	virtual void RHIWriteGPUFence(FRHIGPUFence* FenceRHI) final override;
	virtual void* RHILockStagingBuffer(FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI) final override;
	virtual void RHIUnlockStagingBuffer(FRHIStagingBuffer* StagingBuffer) final override;
	virtual void* LockStagingBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI) final override;
	virtual void UnlockStagingBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIStagingBuffer* StagingBuffer) final override;
	virtual void RHIMapStagingSurface_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, uint32 GPUIndex, FRHIGPUFence* Fence, void*& OutData, int32& OutWidth, int32& OutHeight) final override;
	virtual void RHIUnmapStagingSurface_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, uint32 GPUIndex) final override;


	virtual FRenderQueryPoolRHIRef RHICreateRenderQueryPool(ERenderQueryType QueryType, uint32 NumQueries = UINT32_MAX) final override;
	virtual FStagingBufferRHIRef RHICreateStagingBuffer() final override;
	virtual FGPUFenceRHIRef RHICreateGPUFence(const FName &Name) final override;

	virtual bool RHIRequiresComputeGenerateMips() const override;

	virtual uint64 RHIGetMinimumAlignmentForBufferBackedSRV(EPixelFormat Format) final override;

	// Compute the hash of the state components of the PSO initializer for PSO Precaching (only hash data relevant for the RHI specific PSO)
	virtual uint64 RHIComputeStatePrecachePSOHash(const FGraphicsPipelineStateInitializer& Initializer) final override;

	// Compute the hash of the PSO initializer for PSO Precaching (only hash data relevant for the RHI specific PSO)
	virtual uint64 RHIComputePrecachePSOHash(const FGraphicsPipelineStateInitializer& Initializer) final override;

	// Check if PSO Initializers are the same used during PSO Precaching (only compare data relevant for the RHI specific PSO)
	virtual bool RHIMatchPrecachePSOInitializers(const FGraphicsPipelineStateInitializer& LHS, const FGraphicsPipelineStateInitializer& RHS) final override;

	void Cleanup();

	void PurgeFramebufferFromCaches(GLuint Framebuffer);
	void OnBufferDeletion(GLuint VertexBufferResource);
	void OnPixelBufferDeletion(GLuint PixelBufferResource);
	void OnUniformBufferDeletion(GLuint UniformBufferResource,uint32 AllocatedSize,bool bStreamDraw);
	void OnProgramDeletion(GLint ProgramResource);
	void InvalidateTextureResourceInCache(GLuint Resource);
	void InvalidateUAVResourceInCache(GLuint Resource);
	/** Set a resource on texture target of a specific real OpenGL stage. Goes through cache to eliminate redundant calls. */
	FORCEINLINE void CachedSetupTextureStage(FOpenGLContextState& ContextState, GLint TextureIndex, GLenum Target, GLuint Resource, GLint BaseMip, GLint NumMips)
	{
		FTextureStage& TextureState = ContextState.Textures[TextureIndex];
		const bool bSameTarget = (TextureState.Target == Target);
		const bool bSameResource = (TextureState.Resource == Resource);

		if (bSameTarget && bSameResource)
		{
			// Nothing changed, no need to update
			return;
		}
		CachedSetupTextureStageInner(ContextState, TextureIndex, Target, Resource, BaseMip, NumMips);
	}

	void CachedSetupTextureStageInner(FOpenGLContextState& ContextState, GLint TextureIndex, GLenum Target, GLuint Resource, GLint BaseMip, GLint NumMips);
	void CachedSetupUAVStage(FOpenGLContextState& ContextState, GLint UAVIndex, GLenum Format, GLuint Resource, bool bLayered, GLint Layer, GLenum Access);
	void UpdateSRV(FOpenGLShaderResourceView* SRV);
	FOpenGLContextState& GetContextStateForCurrentContext(bool bAssertIfInvalid = true);

	FORCEINLINE void CachedBindArrayBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		if( ContextState.ArrayBufferBound != Buffer )
		{
			glBindBuffer( GL_ARRAY_BUFFER, Buffer );
			ContextState.ArrayBufferBound = Buffer;
		}
	}

	void CachedBindElementArrayBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		if( ContextState.ElementArrayBufferBound != Buffer )
		{
			glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Buffer );
			ContextState.ElementArrayBufferBound = Buffer;
		}
	}
	
	FORCEINLINE void CachedBindStorageBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		if( ContextState.StorageBufferBound != Buffer )
		{
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, Buffer );
			ContextState.StorageBufferBound = Buffer;
		}
	}

	void CachedBindPixelUnpackBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();

		if( ContextState.PixelUnpackBufferBound != Buffer )
		{
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, Buffer );
			ContextState.PixelUnpackBufferBound = Buffer;
		}
	}

	void CachedBindUniformBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		check(IsInRenderingThread()||IsInRHIThread());
		if( ContextState.UniformBufferBound != Buffer )
		{
			glBindBuffer( GL_UNIFORM_BUFFER, Buffer );
			ContextState.UniformBufferBound = Buffer;
		}
	}

	bool IsUniformBufferBound( FOpenGLContextState& ContextState, GLuint Buffer ) const
	{
		return ( ContextState.UniformBufferBound == Buffer );
	}

	/** Add query to Queries list upon its creation. */
	void RegisterQuery( FOpenGLRenderQuery* Query );

	/** Remove query from Queries list upon its deletion. */
	void UnregisterQuery( FOpenGLRenderQuery* Query );

	/** Inform all queries about the need to recreate themselves after OpenGL context they're in gets deleted. */
	void InvalidateQueries();

	void BeginRenderQuery_OnThisThread(FOpenGLRenderQuery* Query);
	void EndRenderQuery_OnThisThread(FOpenGLRenderQuery* Query);
	void GetRenderQueryResult_OnThisThread(FOpenGLRenderQuery* Query, bool bWait);

	FOpenGLSamplerState* GetPointSamplerState() const { return (FOpenGLSamplerState*)PointSamplerState.GetReference(); }

	void InitializeGLTexture(FOpenGLTexture* Texture, const void* BulkDataPtr, uint64 BulkDataSize);

	void* GetOpenGLCurrentContextHandle();

	void SetCustomPresent(class FRHICustomPresent* InCustomPresent);

#define RHITHREAD_GLTRACE 1
#if RHITHREAD_GLTRACE 
	#define RHITHREAD_GLTRACE_BLOCKING QUICK_SCOPE_CYCLE_COUNTER(STAT_OGLRHIThread_Flush);
//#define RHITHREAD_GLTRACE_BLOCKING FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FLUSHING %s!\n"), ANSI_TO_TCHAR(__FUNCTION__))
//#define RHITHREAD_GLTRACE_BLOCKING UE_LOG(LogRHI, Warning,TEXT("FLUSHING %s!\n"), ANSI_TO_TCHAR(__FUNCTION__));
#else
	#define RHITHREAD_GLTRACE_BLOCKING 
#endif
#define RHITHREAD_GLCOMMAND_PROLOGUE() auto GLCommand= [&]() {

#define RHITHREAD_GLCOMMAND_EPILOGUE_RETURN(x) };\
		if (RHICmdList.Bypass() ||  !IsRunningRHIInSeparateThread() || IsInRHIThread())\
		{\
			return GLCommand();\
		}\
		else\
		{\
			x ReturnValue = (x)0;\
			ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([&ReturnValue, GLCommand = MoveTemp(GLCommand)]() { ReturnValue = GLCommand(); }); \
			RHITHREAD_GLTRACE_BLOCKING;\
			RHICmdList.GetAsImmediate().ImmediateFlush(EImmediateFlushType::FlushRHIThread);\
			return ReturnValue;\
		}\

#define RHITHREAD_GLCOMMAND_EPILOGUE_GET_RETURN(x) };\
		x ReturnValue = (x)0;\
		if (RHICmdList.Bypass() ||  !IsRunningRHIInSeparateThread() || IsInRHIThread() )\
		{\
			ReturnValue = GLCommand();\
		}\
		else\
		{\
			ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([&ReturnValue, GLCommand = MoveTemp(GLCommand)]() { ReturnValue = GLCommand(); }); \
			RHITHREAD_GLTRACE_BLOCKING;\
			RHICmdList.GetAsImmediate().ImmediateFlush(EImmediateFlushType::FlushRHIThread);\
		}\


#define RHITHREAD_GLCOMMAND_EPILOGUE() };\
		if (ShouldRunGLRenderContextOpOnThisThread(RHICmdList))\
		{\
			return GLCommand();\
		}\
		else\
		{\
			ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)( MoveTemp(GLCommand) ); \
			RHITHREAD_GLTRACE_BLOCKING;\
			RHICmdList.GetAsImmediate().ImmediateFlush(EImmediateFlushType::FlushRHIThread);\
		}\

#define RHITHREAD_GLCOMMAND_EPILOGUE_NORETURN() };\
		if (ShouldRunGLRenderContextOpOnThisThread(RHICmdList))\
		{\
			GLCommand();\
		}\
		else\
		{\
			ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)(  MoveTemp(GLCommand) ); \
			RHITHREAD_GLTRACE_BLOCKING;\
			RHICmdList.GetAsImmediate().ImmediateFlush(EImmediateFlushType::FlushRHIThread);\
		}\

	struct FTextureLockTracker
	{
		struct FLockParams
		{
			void* RHIBuffer;
			void* Buffer;
			uint32 MipIndex;
			uint32 ArrayIndex;
			uint32 BufferSize;
			uint32 Stride;
			EResourceLockMode LockMode;

			FORCEINLINE_DEBUGGABLE FLockParams(void* InRHIBuffer, void* InBuffer, uint32 InArrayIndex, uint32 InMipIndex, uint32 InStride, uint32 InBufferSize, EResourceLockMode InLockMode)
				: RHIBuffer(InRHIBuffer)
				, Buffer(InBuffer)
				, MipIndex(InMipIndex)
				, ArrayIndex(InArrayIndex)
				, BufferSize(InBufferSize)
				, Stride(InStride)
				, LockMode(InLockMode)
			{
			}
		};
		TArray<FLockParams, TInlineAllocator<16> > OutstandingLocks;
		uint32 TotalMemoryOutstanding;

		FTextureLockTracker()
		{
			TotalMemoryOutstanding = 0;
		}

		FORCEINLINE_DEBUGGABLE void Lock(void* RHIBuffer, void* Buffer, uint32 ArrayIndex, uint32 MipIndex, uint32 Stride, uint32 SizeRHI, EResourceLockMode LockMode)
		{
//#if DO_CHECK
			for (auto& Parms : OutstandingLocks)
			{
				check(Parms.RHIBuffer != RHIBuffer || Parms.MipIndex != MipIndex || Parms.ArrayIndex != ArrayIndex);
			}
//#endif
			OutstandingLocks.Add(FLockParams(RHIBuffer, Buffer, ArrayIndex, MipIndex, Stride, SizeRHI, LockMode));
			TotalMemoryOutstanding += SizeRHI;
		}

		FORCEINLINE_DEBUGGABLE FLockParams Unlock(void* RHIBuffer, uint32 ArrayIndex, uint32 MipIndex)
		{
			for (int32 Index = 0; Index < OutstandingLocks.Num(); Index++)
			{
				FLockParams& CurrentLock = OutstandingLocks[Index];
				if (CurrentLock.RHIBuffer == RHIBuffer && CurrentLock.MipIndex == MipIndex && CurrentLock.ArrayIndex == ArrayIndex)
				{
					FLockParams Result = OutstandingLocks[Index];
					OutstandingLocks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
					TotalMemoryOutstanding -= Result.BufferSize;
					return Result;
				}
			}
			check(!"Mismatched RHI buffer locks.");
			return FLockParams(nullptr, nullptr, 0, 0, 0, 0, RLM_WriteOnly);
		}
	};

	virtual void* LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush, uint64* OutLockedByteCount = nullptr) final override;
	virtual void UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush) final override;
	virtual void* RHILockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void* LockTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2DArray* Texture, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void UnlockTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2DArray* Texture, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;

	virtual FTexture2DRHIRef AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) final override;

	virtual ETextureReallocationStatus FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override
	{
		return this->RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
	}

	virtual ETextureReallocationStatus CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override
	{
		return this->RHICancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
	}

	virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) final override;

	virtual FGraphicsPipelineStateRHIRef RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer) override
	{
		PrepareGFXBoundShaderState(Initializer);

		return new FRHIGraphicsPipelineStateFallBack(Initializer);
	}

	virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState) final override;

	virtual FBoundShaderStateRHIRef RHICreateBoundShaderState(
		FRHIVertexDeclaration* VertexDeclarationRHI,
		FRHIVertexShader* VertexShaderRHI,
		FRHIPixelShader* PixelShaderRHI,
		FRHIGeometryShader* GeometryShaderRHI
	) final override
	{
		return RHICreateBoundShaderState_internal(
			VertexDeclarationRHI,
			VertexShaderRHI,
			PixelShaderRHI,
			GeometryShaderRHI,
			false);
	}

	bool LinkComputeShader(FRHIComputeShader* ComputeShaderRHI, FOpenGLComputeShader* ComputeShader);
	class FOpenGLLinkedProgram* GetLinkedComputeProgram(FRHIComputeShader* ComputeShaderRHI);

	FBoundShaderStateRHIRef RHICreateBoundShaderState_OnThisThread(FRHIVertexDeclaration* VertexDeclaration, FRHIVertexShader* VertexShader, FRHIPixelShader* PixelShader, FRHIGeometryShader* GeometryShader, bool FromPSOFileCache);
	void RHIPerFrameRHIFlushComplete();

	virtual void RHIPostExternalCommandsReset() final override;

	FOpenGLGPUProfiler& GetGPUProfilingData() {
		return GPUProfilingData;
	}

	GLuint GetOpenGLFramebuffer(uint32 NumSimultaneousRenderTargets, FOpenGLTexture** RenderTargets, const uint32* ArrayIndices, const uint32* MipmapLevels, FOpenGLTexture* DepthStencilTarget);
	GLuint GetOpenGLFramebuffer(uint32 NumSimultaneousRenderTargets, FOpenGLTexture** RenderTargets, const uint32* ArrayIndices, const uint32* MipmapLevels, FOpenGLTexture* DepthStencilTarget, int32 NumRenderingSamples);
	
private:

	FBoundShaderStateRHIRef RHICreateBoundShaderState_internal(
		FRHIVertexDeclaration* VertexDeclarationRHI,
		FRHIVertexShader* VertexShaderRHI,
		FRHIPixelShader* PixelShaderRHI,
		FRHIGeometryShader* GeometryShaderRHI,
		bool FromPSOFileCache
	)
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		RHITHREAD_GLCOMMAND_PROLOGUE()
			return RHICreateBoundShaderState_OnThisThread(VertexDeclarationRHI,
				VertexShaderRHI,
				PixelShaderRHI,
				GeometryShaderRHI,
				FromPSOFileCache);
		RHITHREAD_GLCOMMAND_EPILOGUE_RETURN(FBoundShaderStateRHIRef);
	}

	void PrepareGFXBoundShaderState(const FGraphicsPipelineStateInitializer& Initializer);

	FOpenGLLinkedProgram* LinkProgram(const class FOpenGLLinkedProgramConfiguration& Config);

	/** called once per frame, used for resource processing */
	void EndFrameTick();

	/** Counter incremented each time RHIBeginScene is called. */
	uint32 SceneFrameCounter;

	/** Value used to detect when resource tables need to be recached. INDEX_NONE means always recache. */
	uint32 ResourceTableFrameCounter;

	/** RHI device state, independent of underlying OpenGL context used */
	FOpenGLRHIState						PendingState;
	FSamplerStateRHIRef					PointSamplerState;

	/** A list of all viewport RHIs that have been created. */
	TArray<FOpenGLViewport*> Viewports;
	TRefCountPtr<FOpenGLViewport>		DrawingViewport;
	bool								bRevertToSharedContextAfterDrawingViewport;

	bool								bIsRenderingContextAcquired;

	EPrimitiveType						PrimitiveType = PT_Num;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<10000> > BoundShaderStateHistory;

	/** Per-context state caching */
	FOpenGLContextState InvalidContextState;
	FOpenGLContextState	SharedContextState;
	FOpenGLContextState	RenderingContextState;
	// Cached context type on BeginScene
	int32 BeginSceneContextType;

	template <typename TRHIShader>
	void ApplyStaticUniformBuffers(TRHIShader* Shader);

	TArray<FRHIUniformBuffer*> GlobalUniformBuffers;

	/** Cached mip-limits for textures when ARB_texture_view is unavailable */
	TMap<GLuint, TPair<GLenum, GLenum>> TextureMipLimits;

	/** Underlying platform-specific data */
	struct FPlatformOpenGLDevice* PlatformDevice;

	/** Query list. This is used to inform queries they're no longer valid when OpenGL context they're in gets released from another thread. */
	TArray<FOpenGLRenderQuery*> Queries;

	/** A critical section to protect modifications and iteration over Queries list */
	FCriticalSection QueriesListCriticalSection;

	FOpenGLGPUProfiler GPUProfilingData;
	friend FOpenGLGPUProfiler;

	FCriticalSection CustomPresentSection;
	TRefCountPtr<class FRHICustomPresent> CustomPresent;

	void InitializeStateResources();

	void SetupVertexArrays(FOpenGLContextState& ContextCache, uint32 BaseVertexIndex, FOpenGLStream* Streams, uint32 NumStreams, uint32 MaxVertices);

	void SetupBindlessTextures( FOpenGLContextState& ContextState, const TArray<FOpenGLBindlessSamplerInfo> &Samplers );

	/** needs to be called before each draw call */
	void BindPendingFramebuffer( FOpenGLContextState& ContextState );
	void BindPendingShaderState( FOpenGLContextState& ContextState );
	void BindPendingComputeShaderState( FOpenGLContextState& ContextState, FOpenGLComputeShader* ComputeShader );
	void UpdateRasterizerStateInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateDepthStencilStateInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateScissorRectInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateViewportInOpenGLContext( FOpenGLContextState& ContextState );
	
	template <class ShaderType> void SetResourcesFromTables(ShaderType* Shader);
	FORCEINLINE void CommitGraphicsResourceTables()
	{
		if (PendingState.bAnyDirtyGraphicsUniformBuffers)
		{
			CommitGraphicsResourceTablesInner();
		}
	}
	void CommitGraphicsResourceTablesInner();
	void CommitComputeResourceTables(FOpenGLComputeShader* ComputeShader);
	void CommitNonComputeShaderConstants();
	void CommitComputeShaderConstants(FOpenGLComputeShader* ComputeShader);
	void SetPendingBlendStateForActiveRenderTargets( FOpenGLContextState& ContextState );
	
	void SetupTexturesForDraw( FOpenGLContextState& ContextState);
	template <typename StateType>
	void SetupTexturesForDraw( FOpenGLContextState& ContextState, const StateType& ShaderState, int32 MaxTexturesNeeded);

	void SetupUAVsForDraw(FOpenGLContextState& ContextState);
	void SetupUAVsForCompute(FOpenGLContextState& ContextState, const FOpenGLComputeShader* ComputeShader);
	void SetupUAVsForProgram(FOpenGLContextState& ContextState, const TBitArray<>& NeededBits, int32 MaxUAVUnitUsed);

	void RHIClearMRT(const bool* bClearColorArray, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

public:
	/** Remember what RHI user wants set on a specific OpenGL texture stage, translating from Stage and TextureIndex for stage pair. */
	void InternalSetShaderTexture(FOpenGLTexture* Texture, FOpenGLShaderResourceView* SRV, GLint TextureIndex, GLenum Target, GLuint Resource, int NumMips, int LimitMip);
	void InternalSetShaderImageUAV(GLint UAVIndex, GLenum Format, GLuint Resource, bool bLayered, GLint Layer, GLenum Access);
	void InternalSetShaderBufferUAV(GLint UAVIndex, GLuint Resource);
	void InternalSetSamplerStates(GLint TextureIndex, FOpenGLSamplerState* SamplerState);
	void InitializeGLTextureInternal(FOpenGLTexture* Texture, void const* BulkDataPtr, uint64 BulkDataSize);
private:

	void ApplyTextureStage(FOpenGLContextState& ContextState, GLint TextureIndex, const FTextureStage& TextureStage, FOpenGLSamplerState* SamplerState);

	void ReadSurfaceDataRaw(FOpenGLContextState& ContextState, FRHITexture* TextureRHI,FIntRect Rect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	void BindUniformBufferBase(FOpenGLContextState& ContextState, int32 NumUniformBuffers, FRHIUniformBuffer** BoundUniformBuffers, uint32* DynamicOffsets, uint32 FirstUniformBuffer, bool ForceUpdate);

	void ClearCurrentFramebufferWithCurrentScissor(FOpenGLContextState& ContextState, int8 ClearType, int32 NumClearColors, const bool* bClearColorArray, const FLinearColor* ClearColorArray, float Depth, uint32 Stencil);

	FTextureLockTracker GLLockTracker;

};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

/** Implements the OpenGLDrv module as a dynamic RHI providing module. */
class FOpenGLDynamicRHIModule : public IDynamicRHIModule
{
public:
	
	// IModuleInterface
	virtual bool SupportsDynamicReloading() override { return false; }

	// IDynamicRHIModule
	virtual bool IsSupported() override;

	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) override;
};

extern ERHIFeatureLevel::Type GRequestedFeatureLevel;
