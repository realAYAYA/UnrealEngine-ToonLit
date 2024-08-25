// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RHIPrivate.h: Private D3D RHI definitions.
	=============================================================================*/

#pragma once

#include "D3D12RHICommon.h"
#include "ID3D12DynamicRHI.h"

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadManager.h"
#include "Containers/ResourceArray.h"
#include "Serialization/MemoryReader.h"
#include "EngineGlobals.h"
#include "StaticBoundShaderState.h"

// Dependencies.
#include "DXGIUtilities.h"
#include "GPUProfiler.h"
#include "ShaderCore.h"
#include "HDRHelper.h"

#include "D3D12Submission.h"
#include "D3D12RHIDefinitions.h"

// TODO reorder includes so we just include D3D12PipelineState.h here
#include COMPILED_PLATFORM_HEADER(D3D12PipelineState.h)

#include "D3D12DiskCache.h"
#include "D3D12NvidiaExtensions.h"
#include "D3D12IntelExtensions.h"
#include "D3D12Residency.h"

// D3D RHI public headers.
#include "D3D12Util.h"
#include "D3D12State.h"
#include "D3D12Resources.h"
#include "D3D12RootSignature.h"
#include "D3D12Shader.h"
#include "D3D12View.h"
#include "D3D12CommandList.h"
#include "D3D12Texture.h"
#include "D3D12DirectCommandListManager.h"
#include "D3D12Viewport.h"
#include "D3D12ConstantBuffer.h"
#include "D3D12Query.h"
#include "D3D12DescriptorCache.h"
#include "D3D12StateCachePrivate.h"
#include "D3D12Allocation.h"
#include "D3D12TransientResourceAllocator.h"
#include "D3D12CommandContext.h"
#include "D3D12Stats.h"
#include "D3D12Device.h"
#include "D3D12Adapter.h"

template< typename t_A, typename t_B >
inline t_A RoundUpToNextMultiple(const t_A& a, const t_B& b)
{
	return ((a - 1) / b + 1) * b;
}

using namespace D3D12RHI;

extern TAutoConsoleVariable<int32> GD3D12DebugCvar;

static bool D3D12RHI_ShouldCreateWithWarp()
{
	// Use the warp adapter if specified on the command line.
	static bool bCreateWithWarp = FParse::Param(FCommandLine::Get(), TEXT("warp"));
	return bCreateWithWarp;
}

static bool D3D12RHI_AllowSoftwareFallback()
{
	static bool bAllowSoftwareRendering = FParse::Param(FCommandLine::Get(), TEXT("AllowSoftwareRendering"));
	return bAllowSoftwareRendering;
}

static bool D3D12RHI_ShouldAllowAsyncResourceCreation()
{
	static bool bAllowAsyncResourceCreation = !FParse::Param(FCommandLine::Get(), TEXT("nod3dasync"));
	return bAllowAsyncResourceCreation;
}

static bool D3D12RHI_ShouldForceCompatibility()
{
	// Suppress the use of newer D3D12 features.
	static bool bForceCompatibility =
		FParse::Param(FCommandLine::Get(), TEXT("d3dcompat")) ||
		FParse::Param(FCommandLine::Get(), TEXT("d3d12compat"));
	return bForceCompatibility;
}

static bool D3D12RHI_IsRenderDocPresent(ID3D12Device* Device)
{
	IID RenderDocID;
	if (SUCCEEDED(IIDFromString(L"{A7AA6116-9C8D-4BBA-9083-B4D816B71B78}", &RenderDocID)))
	{
		TRefCountPtr<IUnknown> RenderDoc;
		if (SUCCEEDED(Device->QueryInterface(RenderDocID, (void**)RenderDoc.GetInitReference())))
		{
			return true;
		}
	}

	return false;
}

struct FD3D12UpdateTexture3DData
{
	FD3D12ResourceLocation* UploadHeapResourceLocation;
	bool bComputeShaderCopy;
};

/**
* Structure that represents various RTPSO properties (0 if unknown).
* These can be used to report performance characteristics, sort shaders by occupancy, etc.
*/
struct FD3D12RayTracingPipelineInfo
{
	static constexpr uint32 MaxPerformanceGroups = 10;

	// Estimated RTPSO group based on occupancy or other platform-specific heuristics.
	// Group 0 is expected to be performing worst, 9 (MaxPerformanceGroups-1) is expected to be the best.
	uint32 PerformanceGroup = 0;

	uint32 NumVGPR = 0;
	uint32 NumSGPR = 0;
	uint32 StackSize = 0;
	uint32 ScratchSize = 0;
};

struct FD3D12WorkaroundFlags
{
	/** 
	* Certain drivers crash when GetShaderIdentifier() is called on a ray tracing pipeline collection.
	* If we detect such driver, we have to fall back to the path that queries identifiers on full linked RTPSO.
	* This is less efficient and can also trigger another known issue with D3D12 Agility version <= 4.
	*/
	bool bAllowGetShaderIdentifierOnCollectionSubObject = true;

	/**
	* Certain drivers can cause texture corruption when sub allocating textures from a shared heap. Committed resource
	* allocations are used then
	*/
	bool bForceCommittedResourceTextureAllocation = false;
};

extern FD3D12WorkaroundFlags GD3D12WorkaroundFlags;

/** Forward declare the context for the AMD AGS utility library. */
struct AGSContext;

struct INTCExtensionContext;

/** The interface which is implemented by the dynamically bound RHI. */
class FD3D12DynamicRHI : public ID3D12PlatformDynamicRHI
{
	friend class FD3D12CommandContext;

	static FD3D12DynamicRHI* SingleD3DRHI;

public:

	static FD3D12DynamicRHI* GetD3DRHI() { return SingleD3DRHI; }

private:

	/** Texture pool size */
	int64 RequestedTexturePoolSize;

	friend class FD3D12Thread;
	class FD3D12Thread* SubmissionThread = nullptr;
	class FD3D12Thread* InterruptThread = nullptr;

	enum class EQueueStatus
	{
		None      = 0,

		// Work was processed through the queue.
		Processed = 1 << 0,

		// The queue has further, unprocessed work.
		Pending   = 1 << 1
	};
	FRIEND_ENUM_CLASS_FLAGS(EQueueStatus);

	struct FProcessResult
	{
		EQueueStatus Status = EQueueStatus::None;
		uint32 WaitTimeout = INFINITE;
	};

	FCriticalSection SubmissionCS;
	FCriticalSection InterruptCS;

	FProcessResult ProcessSubmissionQueue();
	FProcessResult ProcessInterruptQueue();

	static FD3D12CommandList* GenerateBarrierCommandListAndUpdateState(FD3D12CommandList* SourceCommandList);

	FCriticalSection ObjectsToDeleteCS;
	TArray<FD3D12DeferredDeleteObject> ObjectsToDelete;

public:
	template <typename ...Args>
	void DeferredDelete(Args&&... InArgs)
	{
		FScopeLock Lock(&ObjectsToDeleteCS);
		ObjectsToDelete.Emplace(Forward<Args>(InArgs)...);
	}

	void SubmitCommands(TConstArrayView<struct FD3D12FinalizedCommands*> Commands);
	void SubmitPayloads(TArrayView<FD3D12Payload*> Payloads);

	// Processes the interrupt queue on the calling thread, until the specified GraphEvent is signaled.
	// If the GraphEvent is nullptr, processes the queue until no further progress is made.
	void ProcessInterruptQueueUntil(FGraphEvent* GraphEvent);

	TUniquePtr<TIndirectArray<FD3D12Timing>> CurrentTiming;
	void FlushTiming(bool bCreateNew);
	void ProcessTimestamps(TIndirectArray<FD3D12Timing>& Timing);

	void InitializeSubmissionPipe();
	void ShutdownSubmissionPipe();

	// Inserts a task graph task which is executed once all previously submitted GPU work has completed (across all queues, device and adapters).
	void EnqueueEndOfPipeTask(TUniqueFunction<void()> TaskFunc, TUniqueFunction<void(FD3D12Payload&)> ModifyPayloadCallback = {});
	FGraphEventRef EopTask;

	// Enumerates all queues across all devices and active adapters
	void ForEachQueue(TFunctionRef<void(FD3D12Queue&)> Callback);

	/** Initialization constructor. */
	FD3D12DynamicRHI(const TArray<TSharedPtr<FD3D12Adapter>>& ChosenAdaptersIn, bool bInPixEventEnabled);

	/** Destructor */
	virtual ~FD3D12DynamicRHI();

	// FDynamicRHI interface.
	virtual void Init() override;
	virtual void PostInit() override;
	virtual void Shutdown() override;
	virtual const TCHAR* GetName() override { return TEXT("D3D12"); }

	template<typename TRHIType, typename TReturnType = typename TD3D12ResourceTraits<TRHIType>::TConcreteType>
	static FORCEINLINE TReturnType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<TReturnType*>(Resource);
	}

	template<typename TRHIType, typename TReturnType = typename TD3D12ResourceTraits<TRHIType>::TConcreteType>
	static FORCEINLINE_DEBUGGABLE TReturnType* ResourceCast(TRHIType* Resource, uint32 GPUIndex)
	{
		TReturnType* Object = ResourceCast<TRHIType, TReturnType>(Resource);
		return Object ? static_cast<TReturnType*>(Object->GetLinkedObject(GPUIndex)) : nullptr;
	}

	virtual bool QueueSupportsTileMapping(ED3D12QueueType /*InQueueType*/) { return true; }

	virtual FD3D12CommandContext* CreateCommandContext(FD3D12Device* InParent, ED3D12QueueType InQueueType, bool InIsDefaultContext);
	virtual void CreateCommandQueue(FD3D12Device* Device, const D3D12_COMMAND_QUEUE_DESC& Desc, TRefCountPtr<ID3D12CommandQueue>& OutCommandQueue);

	virtual bool GetHardwareGPUFrameTime(double& OutGPUFrameTime) const
	{
		OutGPUFrameTime = 0.0;
		return false;
	}

	virtual void RHIBeginFrame(FRHICommandListImmediate& RHICmdList) final override;
	virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) final override;
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) final override;
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) final override;
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) final override;
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) final override;
	virtual FPixelShaderRHIRef RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FVertexShaderRHIRef RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FMeshShaderRHIRef RHICreateMeshShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FAmplificationShaderRHIRef RHICreateAmplificationShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FGeometryShaderRHIRef RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FComputeShaderRHIRef RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash) override;
	virtual FGPUFenceRHIRef RHICreateGPUFence(const FName& Name) final override;
	virtual FStagingBufferRHIRef RHICreateStagingBuffer() final override;
	virtual void* RHILockStagingBuffer(FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI) final override;
    virtual void RHIUnlockStagingBuffer(FRHIStagingBuffer* StagingBuffer) final override;
	virtual FBoundShaderStateRHIRef RHICreateBoundShaderState(FRHIVertexDeclaration* VertexDeclaration, FRHIVertexShader* VertexShader, FRHIPixelShader* PixelShader, FRHIGeometryShader* GeometryShader) final override;
	virtual FGraphicsPipelineStateRHIRef RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer) final override;
	virtual FComputePipelineStateRHIRef RHICreateComputePipelineState(FRHIComputeShader* ComputeShader) final override;
	virtual void RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& CreateInfo) final override;
	virtual void RHIReleaseTransition(FRHITransition* Transition) final override;
	virtual FUniformBufferRHIRef RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation) final override;
	virtual void RHIUpdateUniformBuffer(FRHICommandListBase& RHICmdList, FRHIUniformBuffer* UniformBufferRHI, const void* Contents) final override;
	virtual FBufferRHIRef RHICreateBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& Desc, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void RHITransferBufferUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer) final override;
	virtual void RHICopyBuffer(FRHIBuffer* SourceBuffer, FRHIBuffer* DestBuffer) final override;
	virtual void* RHILockBuffer(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode) final override;
	virtual void* RHILockBufferMGPU(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 GPUIndex, uint32 Offset, uint32 Size, EResourceLockMode LockMode) final override;
	virtual void RHIUnlockBuffer(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer) final override;
	virtual void RHIUnlockBufferMGPU(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 GPUIndex) final override;
	virtual FTextureReferenceRHIRef RHICreateTextureReference(FRHICommandListBase& RHICmdList, FRHITexture* InReferencedTexture) final override;
	virtual void RHIUpdateTextureReference(FRHICommandListBase& RHICmdList, FRHITextureReference* TextureRef, FRHITexture* NewTexture) final override;
	virtual FRHICalcTextureSizeResult RHICalcTexturePlatformSize(const FRHITextureDesc& Desc, uint32 FirstMipIndex) override;
	virtual void RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats) final override;
	virtual bool RHIGetTextureMemoryVisualizeData(FColor* TextureData, int32 SizeX, int32 SizeY, int32 Pitch, int32 PixelSize) final override;
	virtual FTextureRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess InResourceState, void** InitialMipData, uint32 NumInitialMips, const TCHAR* DebugName, FGraphEventRef& OutCompletionEvent) final override;
	virtual FTextureRHIRef RHICreateTexture(FRHICommandListBase& RHICmdList, const FRHITextureCreateDesc& CreateDesc) override;
	virtual uint32 RHIComputeMemorySize(FRHITexture* TextureRHI) final override;
	virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D(FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) override;
	virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual void* RHILockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, uint64* OutLockedByteCount = nullptr) final override;
	virtual void RHIUnlockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void* RHILockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void* RHILockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* TextureRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* TextureRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIUpdateTexture2D(FRHICommandListBase& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData) final override;
	virtual void RHIUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData) final override;
	virtual FUpdateTexture3DData RHIBeginUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion) final override;
	virtual void RHIEndUpdateTexture3D(FRHICommandListBase& RHICmdList, FUpdateTexture3DData& UpdateData) final override;
	virtual void RHIEndMultiUpdateTexture3D(FRHICommandListBase& RHICmdList, TArray<FUpdateTexture3DData>& UpdateDataArray) final override;
	virtual void* RHILockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHITexture* Texture, const TCHAR* Name) final override;
	virtual void RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, const TCHAR* Name) final override;
	virtual void RHIReadSurfaceData(FRHITexture* Texture, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIReadSurfaceData(FRHITexture* TextureRHI, FIntRect InRect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIMapStagingSurface(FRHITexture* Texture, FRHIGPUFence* Fence, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex = 0) final override;
	virtual void RHIUnmapStagingSurface(FRHITexture* Texture, uint32 GPUIndex = 0) final override;
	virtual void RHIReadSurfaceFloatData(FRHITexture* Texture, FIntRect Rect, TArray<FFloat16Color>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIReadSurfaceFloatData(FRHITexture* Texture, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace, int32 ArrayIndex, int32 MipIndex) final override;
	virtual void RHIRead3DSurfaceFloatData(FRHITexture* Texture, FIntRect Rect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData) final override;
	virtual void RHIRead3DSurfaceFloatData(FRHITexture* Texture, FIntRect Rect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType) final override;
	virtual void RHIBeginOcclusionQueryBatch_TopOfPipe(FRHICommandListBase& RHICmdList, uint32 NumQueriesInBatch) final override;
	virtual void RHIEndOcclusionQueryBatch_TopOfPipe(FRHICommandListBase& RHICmdList) final override;
	virtual void RHIBeginRenderQuery_TopOfPipe(FRHICommandListBase& RHICmdList, FRHIRenderQuery* RenderQuery) final override;
	virtual void RHIEndRenderQuery_TopOfPipe(FRHICommandListBase& RHICmdList, FRHIRenderQuery* RenderQuery) final override;
	virtual bool RHIGetRenderQueryResult(FRHIRenderQuery* RenderQuery, uint64& OutResult, bool bWait, uint32 GPUIndex = INDEX_NONE) final override;
	virtual uint32 RHIGetViewportNextPresentGPUIndex(FRHIViewport* Viewport) final override;
	virtual FTexture2DRHIRef RHIGetViewportBackBuffer(FRHIViewport* Viewport) final override;
	virtual FUnorderedAccessViewRHIRef RHIGetViewportBackBufferUAV(FRHIViewport* Viewport) final override;
	virtual void RHIAliasTextureResources(FTextureRHIRef& DestTexture, FTextureRHIRef& SrcTexture) final override;
	virtual FTextureRHIRef RHICreateAliasedTexture(FTextureRHIRef& SourceTexture) final override;
	virtual void RHIGetDisplaysInformation(FDisplayInformationArray& OutDisplayInformation) final override;
	virtual uint64 RHIComputePrecachePSOHash(const FGraphicsPipelineStateInitializer& Initializer) final override;
	virtual bool RHIMatchPrecachePSOInitializers(const FGraphicsPipelineStateInitializer& LHS, const FGraphicsPipelineStateInitializer& RHS) final override;
	virtual void RHIAdvanceFrameFence() final override;
	virtual void RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* Viewport) final override;
	virtual void RHIAcquireThreadOwnership() final override;
	virtual void RHIReleaseThreadOwnership() final override;
	virtual void RHIFlushResources() final override;
	virtual uint32 RHIGetGPUFrameCycles(uint32 GPUIndex = 0) final override;
	virtual FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
	virtual void RHIResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen) final override;
	virtual void RHIResizeViewport(FRHIViewport* ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
#if PLATFORM_WINDOWS
	virtual void RHIHandleDisplayChange() final override;
#endif
	virtual void RHITick(float DeltaTime) final override;
	virtual void RHIBlockUntilGPUIdle() final override;
	virtual void RHISubmitCommandsAndFlushGPU() final override;
	virtual bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate) final override;
	virtual void RHIGetSupportedResolution(uint32& Width, uint32& Height) final override;
	virtual void* RHIGetNativeDevice() final override;
	virtual void* RHIGetNativeGraphicsQueue() final override;
	virtual void* RHIGetNativeComputeQueue() final override;
	virtual void* RHIGetNativeInstance() final override;
	virtual class IRHICommandContext* RHIGetDefaultContext() final override;
	virtual class IRHIComputeContext* RHIGetDefaultAsyncComputeContext() final override;
	virtual IRHIComputeContext* RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask) final override;
	virtual IRHIPlatformCommandList* RHIFinalizeContext(IRHIComputeContext* Context) final override;
	virtual void RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists, bool bFlushResources) final override;

	virtual void RHIRunOnQueue(ED3D12RHIRunOnQueueType QueueType, TFunction<void(ID3D12CommandQueue*)>&& CodeToRun, bool bWaitForSubmission) final override;

	virtual IRHITransientResourceAllocator* RHICreateTransientResourceAllocator() override;

	virtual void RHIWriteGPUFence_TopOfPipe(FRHICommandListBase& RHICmdList, FRHIGPUFence* FenceRHI) final override;

	// SRV / UAV creation functions
	virtual FShaderResourceViewRHIRef  RHICreateShaderResourceView (class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc) override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc) override;

	// ID3D12DynamicRHI interface.
	virtual TArray<FD3D12MinimalAdapterDesc> RHIGetAdapterDescs() const final override;
	virtual bool RHIIsPixEnabled() const final override;
	virtual ID3D12CommandQueue* RHIGetCommandQueue() const final override;
	virtual ID3D12Device* RHIGetDevice(uint32 InIndex) const final override;
	virtual uint32 RHIGetDeviceNodeMask(uint32 InIndex) const final override;
	virtual ID3D12GraphicsCommandList* RHIGetGraphicsCommandList(uint32 InDeviceIndex) const final override;
	virtual DXGI_FORMAT RHIGetSwapChainFormat(EPixelFormat InFormat) const final override;
	virtual FTexture2DRHIRef RHICreateTexture2DFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource) final override;
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArrayFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource) final override;
	virtual FTextureCubeRHIRef RHICreateTextureCubeFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource) final override;
	virtual ID3D12Resource* RHIGetResource(FRHIBuffer* InBuffer) const final override;
	virtual uint32 RHIGetResourceDeviceIndex(FRHIBuffer* InBuffer) const final override;
	virtual int64 RHIGetResourceMemorySize(FRHIBuffer* InBuffer) const final override;
	virtual bool RHIIsResourcePlaced(FRHIBuffer* InBuffer) const final override;
	virtual ID3D12Resource* RHIGetResource(FRHITexture* InTexture) const final override;
	virtual uint32 RHIGetResourceDeviceIndex(FRHITexture* InTexture) const final override;
	virtual int64 RHIGetResourceMemorySize(FRHITexture* InTexture) const final override;
	virtual bool RHIIsResourcePlaced(FRHITexture* InTexture) const final override;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE RHIGetRenderTargetView(FRHITexture* InTexture, int32 InMipIndex = 0, int32 InArraySliceIndex = 0) const final override;
	virtual void RHIFinishExternalComputeWork(uint32 InDeviceIndex, ID3D12GraphicsCommandList* InCommandList) final override;
	virtual void RHITransitionResource(FRHICommandList& RHICmdList, FRHITexture* InTexture, D3D12_RESOURCE_STATES InState, uint32 InSubResource) final override;
	virtual void RHISignalManualFence(FRHICommandList& RHICmdList, ID3D12Fence* Fence, uint64 Value) final override;
	virtual void RHIWaitManualFence(FRHICommandList& RHICmdList, ID3D12Fence* Fence, uint64 Value) final override;
	virtual void RHIVerifyResult(ID3D12Device* Device, HRESULT Result, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, FString Message = FString()) const final override;

	//
	// The Following functions are the _RenderThread version of the above functions. They allow the RHI to control the thread synchronization for greater efficiency.
	// These will be un-commented as they are implemented.
	//

	virtual void* LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true, uint64* OutLockedByteCount = nullptr) override final;
	virtual void UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true) override final;

	virtual void* LockTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2DArray* Texture, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) override final;
	virtual void UnlockTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2DArray* Texture, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) override final;

	virtual FTexture2DRHIRef AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus);
	virtual ETextureReallocationStatus FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
	{
		return RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
	}
	virtual ETextureReallocationStatus CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
	{
		return RHICancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
	}

	void RHICalibrateTimers() override;

#if D3D12_RHI_RAYTRACING

	virtual FRayTracingAccelerationStructureSize RHICalcRayTracingSceneSize(uint32 MaxInstances, ERayTracingAccelerationStructureFlags Flags) final override;
	virtual FRayTracingAccelerationStructureSize RHICalcRayTracingGeometrySize(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& Initializer) final override;

	virtual FRayTracingGeometryRHIRef RHICreateRayTracingGeometry(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& Initializer) final override;
	virtual FRayTracingSceneRHIRef RHICreateRayTracingScene(FRayTracingSceneInitializer2 Initializer) final override;
	virtual FRayTracingShaderRHIRef RHICreateRayTracingShader(TArrayView<const uint8> Code, const FSHAHash& Hash, EShaderFrequency ShaderFrequency) final override;
	virtual FRayTracingPipelineStateRHIRef RHICreateRayTracingPipelineState(const FRayTracingPipelineStateInitializer& Initializer) final override;
	virtual void RHITransferRayTracingGeometryUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry) final override;
#endif //D3D12_RHI_RAYTRACING

	virtual FShaderBundleRHIRef RHICreateShaderBundle(uint32 NumRecords) override;

	bool CheckGpuHeartbeat() const override;

	virtual void HandleGpuTimeout(FD3D12Payload* Payload, double SecondsSinceSubmission);

	bool RHIRequiresComputeGenerateMips() const override { return true; };
	bool RHIIncludeOptionalFlushes() const override { return false; }

	bool IsQuadBufferStereoEnabled() const;
	void DisableQuadBufferStereo();

	FBufferRHIRef CreateBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& BufferDesc, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo);

	void* LockBuffer(FRHICommandListBase& RHICmdList, FD3D12Buffer* Buffer, uint32 BufferSize, EBufferUsageFlags BufferUsage, uint32 Offset, uint32 Size, EResourceLockMode LockMode);
	void UnlockBuffer(FRHICommandListBase& RHICmdList, FD3D12Buffer* Buffer, EBufferUsageFlags BufferUsage);

	virtual bool BeginUpdateTexture3D_ComputeShader(FUpdateTexture3DData& UpdateData, FD3D12UpdateTexture3DData* UpdateDataD3D12)
	{
		// Not supported on PC
		return false;
	}
	virtual void EndUpdateTexture3D_ComputeShader(FRHIComputeCommandList& RHICmdList, FUpdateTexture3DData& UpdateData, FD3D12UpdateTexture3DData* UpdateDataD3D12)
	{
		// Not supported on PC
	}

	FUpdateTexture3DData BeginUpdateTexture3D_Internal(FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion);
	void EndUpdateTexture3D_Internal(FRHICommandListBase& RHICmdList, FUpdateTexture3DData& UpdateData);

public:

#if	PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	virtual void* CreateVirtualTexture(ETextureCreateFlags InFlags, D3D12_RESOURCE_DESC& ResourceDesc, const struct FD3D12TextureLayout& TextureLayout, FD3D12Resource** ppResource, FPlatformMemory::FPlatformVirtualMemoryBlock& RawTextureBlock, D3D12_RESOURCE_STATES InitialUsage = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) = 0;
	virtual void DestroyVirtualTexture(ETextureCreateFlags InFlags, void* RawTextureMemory, FPlatformMemory::FPlatformVirtualMemoryBlock& RawTextureBlock, uint64 CommittedTextureSize) = 0;
#endif
	virtual bool HandleSpecialLock(void*& MemoryOut, uint32 MipIndex, uint32 ArrayIndex, FD3D12Texture* InTexture, EResourceLockMode LockMode, uint32& DestStride, uint64* OutLockedByteCount = nullptr) { return false; }
	virtual bool HandleSpecialUnlock(FRHICommandListBase* RHICmdList, uint32 MipIndex, FD3D12Texture* InTexture) { return false; }

	FD3D12Adapter& GetAdapter(uint32_t Index = 0) { return *ChosenAdapters[Index]; }
	const FD3D12Adapter& GetAdapter(uint32_t Index = 0) const { return *ChosenAdapters[Index]; }

	uint32 GetNumAdapters() const { return ChosenAdapters.Num(); }

	bool IsPixEventEnabled() const { return bPixEventEnabled; }

	template<typename PerDeviceFunction>
	void ForEachDevice(ID3D12Device* inDevice, const PerDeviceFunction& pfPerDeviceFunction);

	AGSContext* GetAmdAgsContext() { return AmdAgsContext; }
	void SetAmdSupportedExtensionFlags(uint32 Flags) { AmdSupportedExtensionFlags = Flags; }
	uint32 GetAmdSupportedExtensionFlags() const { return AmdSupportedExtensionFlags; }

	INTCExtensionContext* GetIntelExtensionContext() { return IntelExtensionContext; }

protected:

	TArray<TSharedPtr<FD3D12Adapter>> ChosenAdapters;

#if D3D12RHI_SUPPORTS_WIN_PIX
	void* WinPixGpuCapturerHandle = nullptr;
#endif

	/** Can pix events be used */
	bool bPixEventEnabled = false;

	/** The feature level of the device. */
	D3D_FEATURE_LEVEL FeatureLevel;

	/**
	 * The context for the AMD AGS utility library.
	 * AGSContext does not implement AddRef/Release.
	 * Just use a bare pointer.
	 */
	AGSContext* AmdAgsContext;
	uint32 AmdSupportedExtensionFlags;

	INTCExtensionContext* IntelExtensionContext = nullptr;

	/** A buffer in system memory containing all zeroes of the specified size. */
	void* ZeroBuffer;
	uint32 ZeroBufferSize;

#if PLATFORM_WINDOWS
	TRefCountPtr<IDXGIFactory2> DXGIFactoryForDisplayList;
#endif

public:

	virtual FD3D12ResourceDesc GetResourceDesc(const FRHITextureDesc& CreateInfo) const;

	virtual FD3D12Texture* CreateD3D12Texture(const FRHITextureCreateDesc& CreateDesc, FRHICommandListBase* RHICmdList, ID3D12ResourceAllocator* ResourceAllocator = nullptr);
	FD3D12Buffer* CreateD3D12Buffer(class FRHICommandListBase* RHICmdList, FRHIBufferDesc const& BufferDesc, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo, ID3D12ResourceAllocator* ResourceAllocator = nullptr);
	virtual FD3D12Texture* CreateNewD3D12Texture(const FRHITextureCreateDesc& CreateDesc, class FD3D12Device* Device);

	FRHIBuffer* CreateBuffer(const FRHIBufferCreateInfo& CreateInfo, const TCHAR* DebugName, ERHIAccess InitialState, ID3D12ResourceAllocator* ResourceAllocator);

	bool SetupDisplayHDRMetaData();

protected:

	FD3D12Texture* CreateTextureFromResource(bool bTextureArray, bool bCubeTexture, EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource);
	FD3D12Texture* CreateAliasedD3D12Texture2D(FD3D12Texture* SourceTexture);

	/**
	 * Gets the best supported MSAA settings from the provided MSAA count to check against.
	 *
	 * @param PlatformFormat		The format of the texture being created
	 * @param MSAACount				The MSAA count to check against.
	 * @param OutBestMSAACount		The best MSAA count that is suppored.  Could be smaller than MSAACount if it is not supported
	 * @param OutMSAAQualityLevels	The number MSAA quality levels for the best msaa count supported
	 */
	void GetBestSupportedMSAASetting(DXGI_FORMAT PlatformFormat, uint32 MSAACount, uint32& OutBestMSAACount, uint32& OutMSAAQualityLevels);

	/**
	* Returns a pointer to a texture resource that can be used for CPU reads.
	* Note: the returned resource could be the original texture or a new temporary texture.
	* @param TextureRHI - Source texture to create a staging texture from.
	* @param InRect - rectangle to 'stage'.
	* @param StagingRectOUT - parameter is filled with the rectangle to read from the returned texture.
	* @return The CPU readable Texture object.
	*/
	TRefCountPtr<FD3D12Resource> GetStagingTexture(FRHITexture* TextureRHI, FIntRect InRect, FIntRect& OutRect, FReadSurfaceDataFlags InFlags, D3D12_PLACED_SUBRESOURCE_FOOTPRINT &readBackHeapDesc, uint32 GPUIndex);

	void ReadSurfaceDataNoMSAARaw(FRHITexture* TextureRHI, FIntRect Rect, TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	void ReadSurfaceDataMSAARaw(FRHITexture* TextureRHI, FIntRect Rect, TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	// This should only be called by Dynamic RHI member functions
	FD3D12Device* GetRHIDevice(uint32 GPUIndex) const;

	void SetupD3D12Debug();

	HANDLE FlipEvent;

	const bool bAllowVendorDevice;

	FDisplayInformationArray DisplayList;

	void ProcessDeferredDeletionQueue();
	void ProcessDeferredDeletionQueue_Platform();
};

ENUM_CLASS_FLAGS(FD3D12DynamicRHI::EQueueStatus);

/** Implements the D3D12RHI module as a dynamic RHI providing module. */
class FD3D12DynamicRHIModule : public IDynamicRHIModule
{
public:

	FD3D12DynamicRHIModule()
	{
	}

	~FD3D12DynamicRHIModule()
	{
	}

	// IModuleInterface
	virtual bool SupportsDynamicReloading() override { return false; }
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IDynamicRHIModule
	virtual bool IsSupported() override { return IsSupported(ERHIFeatureLevel::SM5); }
	virtual bool IsSupported(ERHIFeatureLevel::Type RequestedFeatureLevel) override;
	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) override;

private:

#if D3D12RHI_SUPPORTS_WIN_PIX
	void* WindowsPixDllHandle = nullptr;
	void* WinPixGpuCapturerHandle = nullptr;
#endif

	TArray<TSharedPtr<FD3D12Adapter>> ChosenAdapters;

	// set MaxSupportedFeatureLevel and ChosenAdapter
	void FindAdapter();
};

// Helper to push/pop a desired state on a resource. Handles both tracked and untracked resources.
class FScopedResourceBarrier
{
private:
	FD3D12ContextCommon&        Context;
	FD3D12Resource* const Resource;
	FD3D12ResourceLocation* const ResourceLocation;
	D3D12_RESOURCE_STATES const DesiredState;
	uint32                const Subresource;

	bool bRestoreState = false;

public:
	FScopedResourceBarrier(FD3D12ContextCommon& Context, FD3D12Resource* Resource, FD3D12ResourceLocation* InResourceLocation, D3D12_RESOURCE_STATES DesiredState, uint32 Subresource)
		: Context     (Context)
		, Resource(Resource)
		, ResourceLocation(InResourceLocation)
		, DesiredState(DesiredState)
		, Subresource (Subresource)
	{
		if (Resource->RequiresResourceStateTracking())
		{
			Context.TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, DesiredState, Subresource);
		}
		else
		{
			// Resources that do not require state tracking are kept in a default state, meaning the previous state for the transition is known.
			D3D12_RESOURCE_STATES CurrentState = Resource->GetDefaultResourceState();

			// Some states such as D3D12_RESOURCE_STATE_GENERIC_READ already includes D3D12_RESOURCE_STATE_COPY_SOURCE as well as other states, therefore transition isn't required.
			if (CurrentState != DesiredState && !EnumHasAllFlags(CurrentState, DesiredState))
			{
				Context.AddTransitionBarrier(Resource, CurrentState, DesiredState, Subresource);

				// We will add a transition, we need to transition back to the default state when the scoped object dies.
				bRestoreState = true;
			}
		}
	}

	~FScopedResourceBarrier()
	{
		if (bRestoreState)
		{
			// Only untracked resources should get restored.
			check(!Resource->RequiresResourceStateTracking());

			Context.AddTransitionBarrier(Resource, DesiredState, Resource->GetDefaultResourceState(), Subresource);
		}
		else if (Resource->RequiresResourceStateTracking() && ResourceLocation)
		{
			Context.ConditionalClearShaderResource(ResourceLocation, EShaderParameterTypeMask::SRVMask | EShaderParameterTypeMask::UAVMask);
		}
	}
};

// Returns the given format as a string. Unsupported formats are treated as DXGI_FORMAT_UNKNOWN.
const TCHAR* LexToString(DXGI_FORMAT Format);
