// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11RHIPrivate.h: Private D3D RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ID3D11DynamicRHI.h"
#include "D3D11RHI.h"
// Dependencies.
#include "RHI.h"
#include "GPUProfiler.h"
#include "ShaderCore.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "HDRHelper.h"
#include "BoundShaderStateHistory.h"
#include "DXGIUtilities.h"

DECLARE_LOG_CATEGORY_EXTERN(LogD3D11RHI, Log, All);

#include "Containers/StaticArray.h"

// D3D RHI public headers.
#include "D3D11Util.h"
#include "D3D11State.h"
#include "D3D11Resources.h"
#include "D3D11Viewport.h"
#include "D3D11ConstantBuffer.h"
#include "D3D11StateCache.h"
#include "RHIValidationCommon.h"
#include "RHICoreShader.h"

#ifndef WITH_DX_PERF
#define WITH_DX_PERF	1
#endif

#if NV_AFTERMATH
#define GFSDK_Aftermath_WITH_DX11 1
#include "GFSDK_Aftermath.h"
#include "GFSDK_Aftermath_GpuCrashdump.h"
#undef GFSDK_Aftermath_WITH_DX11
extern bool GDX11NVAfterMathEnabled;
extern bool GDX11NVAfterMathMarkers;
extern float GDX11NVAfterMathDumpWaitTime;
#endif

#if INTEL_EXTENSIONS
THIRD_PARTY_INCLUDES_START
	#define INTC_IGDEXT_D3D11 1
	#include "igdext.h"
THIRD_PARTY_INCLUDES_END
#endif // INTEL_EXTENSIONS

// DX11 doesn't support higher MSAA count
#define DX_MAX_MSAA_COUNT 8

#ifndef EXPERIMENTAL_D3D11_RHITHREAD
#define EXPERIMENTAL_D3D11_RHITHREAD 0
#endif

#if EXPERIMENTAL_D3D11_RHITHREAD
#define D3D11_NUM_THREAD_LOCAL_CACHES 2
#else
#define D3D11_NUM_THREAD_LOCAL_CACHES 1
#endif

#ifndef WITH_NV_API
#define WITH_NV_API 0
#endif

#ifndef WITH_AMD_AGS
#define WITH_AMD_AGS 0
#endif

/**
 * The D3D RHI stats.
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Present time"),STAT_D3D11PresentTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CustomPresent time"), STAT_D3D11CustomPresentTime, STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CreateTexture time"),STAT_D3D11CreateTextureTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("LockTexture time"),STAT_D3D11LockTextureTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UnlockTexture time"),STAT_D3D11UnlockTextureTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CopyTexture time"),STAT_D3D11CopyTextureTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CreateBoundShaderState time"),STAT_D3D11CreateBoundShaderStateTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("New bound shader state time"),STAT_D3D11NewBoundShaderStateTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clean uniform buffer pool"),STAT_D3D11CleanUniformBufferTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clear shader resources"),STAT_D3D11ClearShaderResourceTime,STATGROUP_D3D11RHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Uniform buffer pool num free"),STAT_D3D11NumFreeUniformBuffers,STATGROUP_D3D11RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Immutable Uniform buffers"), STAT_D3D11NumImmutableUniformBuffers, STATGROUP_D3D11RHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Bound Shader State"), STAT_D3D11NumBoundShaderState, STATGROUP_D3D11RHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Uniform buffer pool memory"), STAT_D3D11FreeUniformBufferMemory, STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update uniform buffer"),STAT_D3D11UpdateUniformBufferTime,STATGROUP_D3D11RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Allocated"),STAT_D3D11TexturesAllocated,STATGROUP_D3D11RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Released"),STAT_D3D11TexturesReleased,STATGROUP_D3D11RHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Texture object pool memory"),STAT_D3D11TexturePoolMemory,STATGROUP_D3D11RHI, );


DECLARE_CYCLE_STAT_EXTERN(TEXT("RenderTargetCommit"), STAT_D3D11RenderTargetCommits, STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("RenderTargetCommitUAV"), STAT_D3D11RenderTargetCommitsUAV, STATGROUP_D3D11RHI, );

extern TAutoConsoleVariable<int32> GCVarUseSharedKeyedMutex;

extern TAutoConsoleVariable<int32> GD3D11DebugCvar;

struct FD3D11GlobalStats
{
	// in bytes, never change after RHI, needed to scale game features
	static int64 GDedicatedVideoMemory;
	
	// in bytes, never change after RHI, needed to scale game features
	static int64 GDedicatedSystemMemory;
	
	// in bytes, never change after RHI, needed to scale game features
	static int64 GSharedSystemMemory;
	
	// In bytes. Never changed after RHI init. Our estimate of the amount of memory that we can use for graphics resources in total.
	static int64 GTotalGraphicsMemory;
};


// This class has multiple inheritance but really FGPUTiming is a static class
class FD3D11BufferedGPUTiming : public FRenderResource, public FGPUTiming
{
public:
	/**
	 * Constructor.
	 *
	 * @param InD3DRHI			RHI interface
	 * @param InBufferSize		Number of buffered measurements
	 */
	FD3D11BufferedGPUTiming(class FD3D11DynamicRHI* InD3DRHI, int32 BufferSize);

	/**
	 * Start a GPU timing measurement.
	 */
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

	/**
	 * Initializes all D3D resources.
	 */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/**
	 * Releases all D3D resources.
	 */
	virtual void ReleaseRHI() override;

	static void CalibrateTimers(FD3D11DynamicRHI* InD3DRHI);

private:
	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	/** RHI interface */
	FD3D11DynamicRHI*			D3DRHI;
	/** Number of timestamps created in 'StartTimestamps' and 'EndTimestamps'. */
	int32						BufferSize;
	/** Current timing being measured on the CPU. */
	int32						CurrentTimestamp;
	/** Number of measurements in the buffers (0 - BufferSize). */
	int32						NumIssuedTimestamps;
	/** Timestamps for all StartTimings. */
	TRefCountPtr<ID3D11Query>*	StartTimestamps;
	/** Timestamps for all EndTimings. */
	TRefCountPtr<ID3D11Query>*	EndTimestamps;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool						bIsTiming;
};

/** Used to track whether a period was disjoint on the GPU, which means GPU timings are invalid. */
class FD3D11DisjointTimeStampQuery : public FRenderResource
{
public:
	FD3D11DisjointTimeStampQuery(class FD3D11DynamicRHI* InD3DRHI);

	void StartTracking();
	void EndTracking();
	bool IsResultValid();
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT GetResult();

	/**
	 * Initializes all D3D resources.
	 */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/**
	 * Releases all D3D resources.
	 */
	virtual void ReleaseRHI() override;


private:

	TRefCountPtr<ID3D11Query> DisjointQuery;

	FD3D11DynamicRHI* D3DRHI;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FD3D11EventNode : public FGPUProfilerEventNode
{
public:
	FD3D11EventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent, class FD3D11DynamicRHI* InRHI) :
		FGPUProfilerEventNode(InName, InParent),
		Timing(InRHI, 1)
	{
		// Initialize Buffered timestamp queries 
		Timing.InitRHI(FRHICommandListExecutor::GetImmediateCommandList()); // can't do this from the RHI thread
	}

	virtual ~FD3D11EventNode()
	{
		Timing.ReleaseRHI();  // can't do this from the RHI thread
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	virtual float GetTiming() override;


	virtual void StartTiming() override
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override
	{
		Timing.EndTiming();
	}

	FD3D11BufferedGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FD3D11EventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:

	FD3D11EventNodeFrame(class FD3D11DynamicRHI* InRHI) :
		FGPUProfilerEventNodeFrame(),
		RootEventTiming(InRHI, 1),
		DisjointQuery(InRHI)
	{
		FRHICommandListBase& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		RootEventTiming.InitRHI(RHICmdList);
		DisjointQuery.InitRHI(RHICmdList);
	}

	~FD3D11EventNodeFrame()
	{
		RootEventTiming.ReleaseRHI();
		DisjointQuery.ReleaseRHI();
	}

	/** Start this frame of per tracking */
	virtual void StartFrame() override;

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;

	virtual void LogDisjointQuery() override;

	/** Timer tracking inclusive time spent in the root nodes. */
	FD3D11BufferedGPUTiming RootEventTiming;

	/** Disjoint query tracking whether the times reported by DumpEventTree are reliable. */
	FD3D11DisjointTimeStampQuery DisjointQuery;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FD3DGPUProfiler : public FGPUProfiler
{
	/** Used to measure GPU time per frame. */
	FD3D11BufferedGPUTiming FrameTiming;

	class FD3D11DynamicRHI* D3D11RHI;

	/** GPU hitch profile histories */
	TIndirectArray<FD3D11EventNodeFrame> GPUHitchEventNodeFrames;

	FD3DGPUProfiler(class FD3D11DynamicRHI* InD3DRHI);

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override
	{
		FD3D11EventNode* EventNode = new FD3D11EventNode(InName, InParent, D3D11RHI);
		return EventNode;
	}

	virtual void PushEvent(const TCHAR* Name, FColor Color) override;
	virtual void PopEvent() override;

	void BeginFrame(class FD3D11DynamicRHI* InRHI);

	void EndFrame();

	bool CheckGpuHeartbeat(bool bShowActiveStatus) const;

private:
	TMap<uint32, FString> CachedStrings;
	TArray<uint32> PushPopStack;
};

struct FD3D11TransitionData
{
	bool bUAVBarrier;
};

/** Forward declare the context for the AMD AGS utility library. */
struct AGSContext;

struct FD3D11Adapter
{
	/** Null if not supported or FindAdapter() wasn't called. */
	TRefCountPtr<IDXGIAdapter> DXGIAdapter;

	DXGI_ADAPTER_DESC DXGIAdapterDesc;

	/** The maximum D3D11 feature level supported. 0 if not supported or FindAdapter() wasn't called */
	D3D_FEATURE_LEVEL MaxSupportedFeatureLevel;

	/** Whether this is a software adapter */
	bool bSoftwareAdapter;

	/** Whether the GPU is integrated or discrete. */
	bool bIsIntegrated;

	// constructors
	FD3D11Adapter() 
	{
	}

	FD3D11Adapter(TRefCountPtr<IDXGIAdapter> InDXGIAdapter, D3D_FEATURE_LEVEL InMaxSupportedFeatureLevel, bool bInSoftwareAdatper, bool InIsIntegrated)
		: DXGIAdapter(InDXGIAdapter)
		, MaxSupportedFeatureLevel(InMaxSupportedFeatureLevel)
		, bSoftwareAdapter(bInSoftwareAdatper)
		, bIsIntegrated(InIsIntegrated)
	{
		if (DXGIAdapter.IsValid())
		{
			VERIFYD3D11RESULT(DXGIAdapter->GetDesc(&DXGIAdapterDesc));
		}
	}

	bool IsValid() const
	{
		return DXGIAdapter.IsValid();
	}
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/** The interface which is implemented by the dynamically bound RHI. */
class D3D11RHI_API FD3D11DynamicRHI : public ID3D11DynamicRHI, public IRHICommandContextPSOFallback
{
public:
	typedef TMap<FD3D11LockedKey, FD3D11LockedData> FD3D11LockTracker;
	friend class FD3D11Viewport;

	/** Initialization constructor. */
	FD3D11DynamicRHI(IDXGIFactory1* InDXGIFactory1, D3D_FEATURE_LEVEL InFeatureLevel, const FD3D11Adapter& InAdapter);

	/** Destructor */
	virtual ~FD3D11DynamicRHI();

	/** If it hasn't been initialized yet, initializes the D3D device. */
	virtual void InitD3DDevice();

	// FDynamicRHI interface.
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual const TCHAR* GetName() override { return TEXT("D3D11"); }

	// HDR display output
	virtual void EnableHDR();
	virtual void ShutdownHDR();

	virtual void FlushPendingLogs() override;

	static FD3D11DynamicRHI& Get() { return *GetDynamicRHI<FD3D11DynamicRHI>(); }

	template<typename TRHIType>
	static FORCEINLINE typename TD3D11ResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TD3D11ResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}

	static inline FD3D11Texture* ResourceCast(FRHITexture* Texture)
	{
		if (!Texture)
		{
			return nullptr;
		}

		FD3D11Texture* Result = static_cast<FD3D11Texture*>(Texture->GetTextureBaseRHI());
		check(Result);

		return Result;
	}

	template<EShaderFrequency ShaderFrequency>
	void BindUniformBuffer(uint32 BufferIndex, FRHIUniformBuffer* BufferRHI);

	template <typename TRHIShader>
	void ApplyStaticUniformBuffers(TRHIShader* Shader);

	template<EShaderFrequency ShaderFrequency>
	void SetShaderParametersCommon(FD3D11ConstantBuffer* StageConstantBuffer, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters);

	template<EShaderFrequency ShaderFrequency>
	void SetShaderUnbindsCommon(TConstArrayView<FRHIShaderParameterUnbind> InUnbinds);

	/**
	 * Reads a D3D query's data into the provided buffer.
	 * @param Query - The D3D query to read data from.
	 * @param Data - The buffer to read the data into.
	 * @param DataSize - The size of the buffer.
	 * @param QueryType e.g. RQT_Occlusion or RQT_AbsoluteTime
	 * @param bWait - If true, it will wait for the query to finish.
	 * @param bStallRHIThread - if true, stall RHIT before accessing immediate context
	 * @return true if the query finished.
	 */
	bool GetQueryData(ID3D11Query* Query, void* Data, SIZE_T DataSize, ERenderQueryType QueryType, bool bWait, bool bStallRHIThread);

	virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) final override;
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) final override;
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) final override;
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) final override;
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) final override;
	virtual FPixelShaderRHIRef RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FVertexShaderRHIRef RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FGeometryShaderRHIRef RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FComputeShaderRHIRef RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash) final override;
	virtual FStagingBufferRHIRef RHICreateStagingBuffer() final override;
	virtual void RHICopyToStagingBuffer(FRHIBuffer* SourceBuffer, FRHIStagingBuffer* DestinationStagingBuffer, uint32 Offset, uint32 NumBytes) final override;
	virtual void RHIWriteGPUFence(FRHIGPUFence* Fence) final override;
	virtual void* RHILockStagingBuffer(FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI) final override;
    virtual void RHIUnlockStagingBuffer(FRHIStagingBuffer* StagingBuffer) final override;
	virtual FBoundShaderStateRHIRef RHICreateBoundShaderState(FRHIVertexDeclaration* VertexDeclaration, FRHIVertexShader* VertexShader, FRHIPixelShader* PixelShader, FRHIGeometryShader* GeometryShader) final override;
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
	virtual void RHIAsyncCopyTexture2DCopy(FRHITexture2D* NewTexture2DRHI, FRHITexture2D* Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus);
	virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D(FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) final override;
	virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual void* RHILockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, uint64* OutLockedByteCount) final override;
	virtual void RHIUnlockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void* RHILockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIUpdateTexture2D(FRHICommandListBase& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData) final override;
	virtual void RHIUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData) final override;
	virtual void* RHILockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHITexture* Texture, const TCHAR* Name) final override;
	virtual void RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, const TCHAR* Name) final override;
	virtual void RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHIUnorderedAccessView* UnorderedAccessViewRHI, const TCHAR* Name) final override;
	virtual void RHIReadSurfaceData(FRHITexture* Texture,FIntRect Rect,TArray<FColor>& OutData,FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIReadSurfaceData(FRHITexture* TextureRHI, FIntRect InRect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIMapStagingSurface(FRHITexture* Texture, FRHIGPUFence* Fence, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex = 0) final override;
	virtual void RHIUnmapStagingSurface(FRHITexture* Texture, uint32 GPUIndex = 0) final override;
	virtual void RHIReadSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,TArray<FFloat16Color>& OutData,ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex) final override;
	virtual void RHIRead3DSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData) final override;
	virtual FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType) final override;
	virtual bool RHIGetRenderQueryResult(FRHIRenderQuery* RenderQuery, uint64& OutResult, bool bWait, uint32 GPUIndex = INDEX_NONE) final override;
	virtual FTexture2DRHIRef RHIGetViewportBackBuffer(FRHIViewport* Viewport) final override;
	virtual void RHIAliasTextureResources(FTextureRHIRef& DestTexture, FTextureRHIRef& SrcTexture) final override;
	virtual FTextureRHIRef RHICreateAliasedTexture(FTextureRHIRef& SourceTexture) final override;
	virtual void RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* Viewport) final override;
	virtual void RHIAcquireThreadOwnership() final override;
	virtual void RHIReleaseThreadOwnership() final override;
	virtual void RHIFlushResources() final override;
	virtual uint32 RHIGetGPUFrameCycles(uint32 GPUIndex = 0) final override;
	virtual FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
	virtual void RHIResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen) final override;
	virtual void RHIResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
	virtual EColorSpaceAndEOTF RHIGetColorSpace(FRHIViewport* Viewport) final override;
	virtual void RHICheckViewportHDRStatus(FRHIViewport* ViewportRHI) final override;
	virtual void RHITick(float DeltaTime) final override;
	virtual void RHIBlockUntilGPUIdle() final override;
	virtual bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate) final override;
	virtual void RHIGetSupportedResolution(uint32& Width, uint32& Height) final override;
	virtual void* RHIGetNativeDevice() final override;
	virtual void* RHIGetNativeInstance() final override;
	virtual void* RHIGetNativeCommandBuffer() final override;
	virtual class IRHICommandContext* RHIGetDefaultContext() final override;
	virtual IRHIComputeContext* RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask) final override;
	virtual IRHIPlatformCommandList* RHIFinalizeContext(IRHIComputeContext* Context) final override;
	virtual void RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists, bool bFlushResources) final override;

	// SRV / UAV creation functions
	virtual FShaderResourceViewRHIRef  RHICreateShaderResourceView (class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc) final override;

	virtual void RHISetComputeShader(FRHIComputeShader* ComputeShader) final override;
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	virtual void RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIBeginUAVOverlap() final override;
	virtual void RHIEndUAVOverlap() final override;
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	virtual void RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values) final override;
	virtual void RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values) final override;
	virtual void RHICopyTexture(FRHITexture* SourceTexture, FRHITexture* DestTexture, const FRHICopyTextureInfo& CopyInfo) final override;
	virtual void RHICopyBufferRegion(FRHIBuffer* DestBuffer, uint64 DstOffset, FRHIBuffer* SourceBuffer, uint64 SrcOffset, uint64 NumBytes) final override;
	virtual void RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& CreateInfo) final override;
	virtual void RHIReleaseTransition(FRHITransition* Transition) final override;
	virtual void RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions) override final;
	virtual void RHIEndTransitions(TArrayView<const FRHITransition*> Transitions) override final;
	virtual void RHIBeginRenderQuery(FRHIRenderQuery* RenderQuery) final override;
	virtual void RHIEndRenderQuery(FRHIRenderQuery* RenderQuery) final override;
	void RHIBeginOcclusionQueryBatch(uint32 NumQueriesInBatch);
	void RHIEndOcclusionQueryBatch();
	virtual void RHISubmitCommandsHint() final override;
	virtual void RHIBeginDrawingViewport(FRHIViewport* Viewport, FRHITexture* RenderTargetRHI) final override;
	virtual void RHIEndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync) final override;
	using FDynamicRHI::RHIBeginFrame;
	virtual void RHIBeginFrame() override;
	virtual void RHIEndFrame() override;
	virtual void RHIBeginScene() final override;
	virtual void RHIEndScene() final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset) final override;
	virtual void RHISetRasterizerState(FRHIRasterizerState* NewState) final override;
	virtual void RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) final override;
	virtual void RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ) override;
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	virtual void RHISetBoundShaderState(FRHIBoundShaderState* BoundShaderState) final override;
	virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState) final override;
	virtual void RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers) final override;
	virtual void RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override;
	virtual void RHISetShaderParameters(FRHIGraphicsShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override;
	virtual void RHISetShaderUnbinds(FRHIComputeShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds) final override;
	virtual void RHISetShaderUnbinds(FRHIGraphicsShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds) final override;
	virtual void RHISetDepthStencilState(FRHIDepthStencilState* NewState, uint32 StencilRef) final override;
	virtual void RHISetStencilRef(uint32 StencilRef) final override;
	virtual void RHISetBlendState(FRHIBlendState* NewState, const FLinearColor& BlendFactor) final override;
	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) final override;
	void SetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget);
	void InternalSetUAVCS(uint32 BindIndex, FD3D11UnorderedAccessView* UnorderedAccessViewRHI);
	void InternalSetUAVPS(uint32 BindIndex, FD3D11UnorderedAccessView* UnorderedAccessViewRHI);
	void SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo);
	virtual void RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIEnableDepthBoundsTest(bool bEnable) final override
	{
		if (GSupportsDepthBoundsTest && StateCache.bDepthBoundsEnabled != bEnable)
		{
			EnableDepthBoundsTest(bEnable, 0.0f, 1.0f);
		}
	}
	virtual void RHISetDepthBounds(float MinDepth, float MaxDepth) final override
	{
		if (GSupportsDepthBoundsTest && (StateCache.DepthBoundsMin != MinDepth || StateCache.DepthBoundsMax != MaxDepth))
		{
			EnableDepthBoundsTest(true, MinDepth, MaxDepth);
		}
	}
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override;
	virtual void RHIPopEvent() final override;

	virtual void RHIPerFrameRHIFlushComplete() final override;
	virtual void RHIPollRenderQueryResults() final override;

	// *_RenderThread functions. Command lists call these functions on RT. You can implement your own behavior inside these functions.
	// For example, deferring the actual creation to RHI thread by sending an RHI command.
	// For D3D11, these functions mainly just remove RHIT stalls because ID3D11Device is thread safe.
	virtual FTexture2DRHIRef AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) final override;
	virtual ETextureReallocationStatus FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual ETextureReallocationStatus CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted) final override;
	virtual void* LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true, uint64* OutLockedByteCount = nullptr) final override;
	virtual void UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true) final override;
	virtual void RHIEndUpdateTexture3D(FRHICommandListBase& RHICmdList, FUpdateTexture3DData& UpdateData) final override;
	virtual void* RHILockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;

	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName) final override;
	virtual void RHIEndRenderPass() final override;

	void ResolveTexture(UE::RHICore::FResolveTextureInfo Info);

	virtual void RHICalibrateTimers() override;

	// ID3D11DynamicRHI interface
	virtual ID3D11Device*         RHIGetDevice() const final override;
	virtual ID3D11DeviceContext*  RHIGetDeviceContext() const final override;
	virtual IDXGIAdapter*         RHIGetAdapter() const final override;
	virtual IDXGISwapChain*       RHIGetSwapChain(FRHIViewport* InViewport) const final override;
	virtual DXGI_FORMAT           RHIGetSwapChainFormat(EPixelFormat InFormat) const final override;
	virtual FTexture2DRHIRef      RHICreateTexture2DFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* Resource) final override;
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArrayFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding&, ID3D11Texture2D* Resource) final override;
	virtual FTextureCubeRHIRef    RHICreateTextureCubeFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* Resource) final override;
	virtual ID3D11Buffer*         RHIGetResource(FRHIBuffer* InBuffer) const final override;
	virtual ID3D11Resource*       RHIGetResource(FRHITexture* InTexture) const final override;
	virtual int64                 RHIGetResourceMemorySize(FRHITexture* InTexture) const final override;
	virtual ID3D11RenderTargetView* RHIGetRenderTargetView(FRHITexture* InTexture, int32 InMipIndex = 0, int32 InArraySliceIndex = -1) const final override;
	virtual ID3D11ShaderResourceView* RHIGetShaderResourceView(FRHITexture* InTexture) const final override;
	virtual void                  RHIRegisterWork(uint32 NumPrimitives) final override;
	virtual void                  RHIVerifyResult(ID3D11Device* Device, HRESULT Result, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line) const final override;

	virtual void				  RHIGetDisplaysInformation(FDisplayInformationArray& OutDisplayInformation) final override;

	// Accessors.
	ID3D11Device* GetDevice() const
	{
		return Direct3DDevice;
	}
	FD3D11DeviceContext* GetDeviceContext() const
	{
		return Direct3DDeviceIMContext;
	}

#if NV_AFTERMATH
	GFSDK_Aftermath_ContextHandle GetNVAftermathContext()
	{
		return NVAftermathIMContextHandle;
	}
#endif

	IDXGIFactory1* GetFactory() const
	{
		return DXGIFactory1;
	}

	bool CheckGpuHeartbeat() const override
	{
		return GPUProfilingData.CheckGpuHeartbeat(false);
	}

	void AddLockedData(const FD3D11LockedKey& Key, const FD3D11LockedData& LockedData)
	{
		FScopeLock Lock(&LockTrackerCS);
		LockTracker.Add(Key, LockedData);
	}

	bool RemoveLockedData(const FD3D11LockedKey& Key, FD3D11LockedData& OutLockedData)
	{
		FScopeLock Lock(&LockTrackerCS);
		return LockTracker.RemoveAndCopyValue(Key, OutLockedData);
	}

	bool IsQuadBufferStereoEnabled();
	void DisableQuadBufferStereo();

	void BeginRecursiveCommand()
	{
		// Nothing to do
	}

private:
	void EnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth);

	static void ClearUAV(TRHICommandList_RecursiveHazardous<FD3D11DynamicRHI>& RHICmdList, FD3D11UnorderedAccessView* UAV, const void* ClearValues, bool bFloat);

	enum class EForceFullScreenClear
	{
		EDoNotForce,
		EForce
	};

	virtual void RHIClearMRTImpl(const bool* bClearColorArray, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

	template <EShaderFrequency ShaderFrequency>
	void ClearShaderResourceViews(FD3D11ViewableResource* Resource);

	template <EShaderFrequency ShaderFrequency>
	void ClearAllShaderResourcesForFrequency();

	template <EShaderFrequency ShaderFrequency>
	void InternalSetShaderResourceView(FD3D11ViewableResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex);

	void TrackResourceBoundAsVB(FD3D11ViewableResource* Resource, int32 StreamIndex);
	void TrackResourceBoundAsIB(FD3D11ViewableResource* Resource);

	void SetCurrentComputeShader(FRHIComputeShader* ComputeShader)
	{
		CurrentComputeShader = ComputeShader;
	}
	
	const FComputeShaderRHIRef& GetCurrentComputeShader() const
	{
		return CurrentComputeShader;
	}

public:

	template <EShaderFrequency ShaderFrequency>
	void SetShaderResourceView(FD3D11ViewableResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex)
	{
		InternalSetShaderResourceView<ShaderFrequency>(Resource, SRV, ResourceIndex);
	}

	void ClearState();
	void ConditionalClearShaderResource(FD3D11ViewableResource* Resource, bool bCheckBoundInputAssembler);
	void ClearAllShaderResources();

	uint32 GetHDRDetectedDisplayIndex() const
	{
		return HDRDetectedDisplayIndex;
	}

	void SetHDRDetectedDisplayIndices(const uint32 DisplayIndex, const uint32 IHVIndex)
	{
		HDRDetectedDisplayIndex = DisplayIndex;
		HDRDetectedDisplayIHVIndex = IHVIndex;
	}

	EPixelFormat GetDisplayFormat(EPixelFormat InPixelFormat) const;

	FD3D11StateCache& GetStateCache() { return StateCache; }

protected:
	/** The global D3D interface. */
	TRefCountPtr<IDXGIFactory1> DXGIFactory1;

	// Whether HDR is available from the particular DXGI factories available
	bool bDXGISupportsHDR;

	/** The global D3D device's immediate context */
	TRefCountPtr<FD3D11DeviceContext> Direct3DDeviceIMContext;

#if NV_AFTERMATH
	GFSDK_Aftermath_ContextHandle NVAftermathIMContextHandle;
#endif

	/** The global D3D device's immediate context */
	TRefCountPtr<FD3D11Device> Direct3DDevice;

	FD3D11StateCache StateCache;

	/** Tracks outstanding locks on each thread */
	FD3D11LockTracker LockTracker;
	FCriticalSection LockTrackerCS;

	/** A list of all viewport RHIs that have been created. */
	TArray<FD3D11Viewport*> Viewports;

	/** The viewport which is currently being drawn. */
	TRefCountPtr<FD3D11Viewport> DrawingViewport;

	/** The feature level of the device. */
	D3D_FEATURE_LEVEL FeatureLevel;

	/**
	 * The context for the AMD AGS utility library.
	 * AGSContext does not implement AddRef/Release.
	 * Just use a bare pointer.
	 */
	AGSContext* AmdAgsContext;

#if INTEL_EXTENSIONS
	// Context and functions for Intel extension framework utility library
	INTCExtensionContext* IntelExtensionContext;
	bool bIntelSupportsUAVOverlap;
#endif // INTEL_EXTENSIONS

	// set by UpdateMSAASettings(), get by GetMSAAQuality()
	// [SampleCount] = Quality, 0xffffffff if not supported
	uint32 AvailableMSAAQualities[DX_MAX_MSAA_COUNT + 1];

	/** A buffer in system memory containing all zeroes of the specified size. */
	void* ZeroBuffer;
	uint32 ZeroBufferSize;

	// Tracks the currently set state blocks.
	bool bCurrentDepthStencilStateIsReadOnly;

	// Current PSO Primitive Type
	EPrimitiveType PrimitiveType;

	TRefCountPtr<ID3D11RenderTargetView> CurrentRenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	TRefCountPtr<FD3D11UnorderedAccessView> CurrentUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT];
	ID3D11UnorderedAccessView* UAVBound[D3D11_PS_CS_UAV_REGISTER_COUNT];
	uint32 UAVBindFirst;
	uint32 UAVBindCount;
	uint32 UAVSChanged;
	uint32 CurrentRTVOverlapMask;
	uint32 CurrentUAVMask;

	TRefCountPtr<ID3D11DepthStencilView> CurrentDepthStencilTarget;
	TRefCountPtr<FD3D11Texture> CurrentDepthTexture;
	FD3D11ViewableResource* CurrentResourcesBoundAsSRVs[SF_NumStandardFrequencies][D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	FD3D11ViewableResource* CurrentResourcesBoundAsVBs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	FD3D11ViewableResource* CurrentResourceBoundAsIB;
	int32 MaxBoundShaderResourcesIndex[SF_NumStandardFrequencies];
	int32 MaxBoundVertexBufferIndex;
	uint32 NumSimultaneousRenderTargets;
	uint32 NumUAVs;

	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;

	/** Internal frame counter that just counts calls to Present */
	uint32 PresentCounter;

	uint32 RequestedOcclusionQueriesInBatch = 0;
	uint32 ActualOcclusionQueriesInBatch = 0;

	/**
	 * Internal counter used for resource table caching.
	 * INDEX_NONE means caching is not allowed.
	 */
	uint32 ResourceTableFrameCounter;

	/** D3D11 defines a maximum of 14 constant buffers per shader stage. */
	enum { MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE = 14 };

	/** Track the currently bound uniform buffers. */
	FRHIUniformBuffer* BoundUniformBuffers[SF_NumStandardFrequencies][MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE];

	/** Bit array to track which uniform buffers have changed since the last draw call. */
	uint16 DirtyUniformBuffers[SF_NumStandardFrequencies];

	TArray<FRHIUniformBuffer*> StaticUniformBuffers;

	/** Tracks the current depth stencil access type. */
	FExclusiveDepthStencil CurrentDSVAccessType;

	/** When a new shader is set, we discard all old constants set for the previous shader. */
	bool bDiscardSharedConstants;

	/** A list of all D3D constant buffers RHIs that have been created. */
	TRefCountPtr<FD3D11ConstantBuffer> VSConstantBuffer;
	TRefCountPtr<FD3D11ConstantBuffer> PSConstantBuffer;
	TRefCountPtr<FD3D11ConstantBuffer> GSConstantBuffer;
	TRefCountPtr<FD3D11ConstantBuffer> CSConstantBuffer;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<10000> > BoundShaderStateHistory;
	FComputeShaderRHIRef CurrentComputeShader;

	/** If HDR display detected, we store the output device. */
	uint32 HDRDetectedDisplayIndex;
	uint32 HDRDetectedDisplayIHVIndex;

	FDisplayInformationArray DisplayList;

	HANDLE ExceptionHandlerHandle = INVALID_HANDLE_VALUE;

	bool bRenderDoc = false;

public:
	void RegisterGPUWork(uint32 NumPrimitives = 0, uint32 NumVertices = 0)
	{
		GPUProfilingData.RegisterGPUWork(NumPrimitives, NumVertices);
	}
	void RegisterGPUDispatch(FIntVector GroupCount)
	{
		GPUProfilingData.RegisterGPUDispatch(GroupCount);
	}

	inline const FD3D11Adapter& GetAdapter() const { return Adapter; }

protected:
	FD3DGPUProfiler GPUProfilingData;

	FD3D11Adapter Adapter;

	// If this is false, disable any IHV optimization/libs
	bool bAllowVendorDevice;

	FD3D11Texture* CreateD3D11Texture2D(FRHITextureCreateDesc const& CreateDesc, TConstArrayView<D3D11_SUBRESOURCE_DATA> InitialData = {});
	FD3D11Texture* CreateD3D11Texture3D(FRHITextureCreateDesc const& CreateDesc);

	FD3D11Texture* CreateTextureFromResource(bool bTextureArray, bool bCubeTexture, EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* TextureResource);

	/** Initializes the constant buffers.  Called once at RHI initialization time. */
	void InitConstantBuffers();

	/** needs to be called before each draw call */
	virtual void CommitNonComputeShaderConstants();

	/** needs to be called before each dispatch call */
	virtual void CommitComputeShaderConstants();

	template <class ShaderType> void SetResourcesFromTables(const ShaderType* RESTRICT);

	void CommitGraphicsResourceTables();
	void CommitComputeResourceTables(FD3D11ComputeShader* ComputeShader);

	void ValidateExclusiveDepthStencilAccess(FExclusiveDepthStencil Src) const;

	/** 
	 * Gets the best supported MSAA settings from the provided MSAA count to check against. 
	 * 
	 * @param PlatformFormat		The format of the texture being created 
	 * @param MSAACount				The MSAA count to check against. 
	 * @param OutBestMSAACount		The best MSAA count that is suppored.  Could be smaller than MSAACount if it is not supported 
	 * @param OutMSAAQualityLevels	The number MSAA quality levels for the best msaa count supported
	 */
	void GetBestSupportedMSAASetting( DXGI_FORMAT PlatformFormat, uint32 MSAACount, uint32& OutBestMSAACount, uint32& OutMSAAQualityLevels );

	// shared code for different D3D11 devices (e.g. PC DirectX11 and XboxOne) called
	// after device creation and GRHISupportsAsyncTextureCreation was set and before resource init
	void SetupAfterDeviceCreation();

	// called by SetupAfterDeviceCreation() when the device gets initialized
	void UpdateMSAASettings();

	// @return 0xffffffff if not not supported
	uint32 GetMaxMSAAQuality(uint32 SampleCount);

	void CommitRenderTargetsAndUAVs();
	void CommitRenderTargets(bool bClearUAVS);
	void CommitUAVs();

	/**
	 * Cleanup the D3D device.
	 * This function must be called from the main game thread.
	 */
	virtual void CleanupD3DDevice();

	void ReleasePooledUniformBuffers();
	void ReleaseCachedQueries();

	template<typename TPixelShader>
	static void ResolveTextureUsingShader(
		FD3D11DynamicRHI* const This,
		FD3D11Texture* const SourceTexture,
		FD3D11Texture* const DestTexture,
		ID3D11RenderTargetView* const DestTextureRTV,
		ID3D11DepthStencilView* const DestTextureDSV,
		D3D11_TEXTURE2D_DESC const& ResolveTargetDesc,
		FResolveRect const& SourceRect,
		FResolveRect const& DestRect,
		typename TPixelShader::FParameter const PixelShaderParameter
		);

	/**
	* Returns a pointer to a texture resource that can be used for CPU reads.
	* Note: the returned resource could be the original texture or a new temporary texture.
	* @param TextureRHI - Source texture to create a staging texture from.
	* @param InRect - rectangle to 'stage'.
	* @param StagingRectOUT - parameter is filled with the rectangle to read from the returned texture.
	* @return The CPU readable Texture object.
	*/
	TRefCountPtr<ID3D11Texture2D> GetStagingTexture(FRHITexture* TextureRHI,FIntRect InRect, FIntRect& OutRect, FReadSurfaceDataFlags InFlags);

	void ReadSurfaceDataNoMSAARaw(FRHITexture* TextureRHI,FIntRect Rect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	void ReadSurfaceDataMSAARaw(FRHITexture* TextureRHI, FIntRect Rect, TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

#if NV_AFTERMATH
	void StartNVAftermath();

	void StopNVAftermath();
#endif

#if INTEL_EXTENSIONS
	void StartIntelExtensions();
	void StopIntelExtensions();
#endif // INTEL_EXTENSIONS

	bool bUAVOverlapEnabled = false;
	void EnableUAVOverlap();
	void DisableUAVOverlap();

	bool SetupDisplayHDRMetaData();

	friend struct FD3DGPUProfiler;

};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

/** Implements the D3D11RHI module as a dynamic RHI providing module. */
class FD3D11DynamicRHIModule : public IDynamicRHIModule
{
public:
	// IModuleInterface	
	virtual bool SupportsDynamicReloading() override { return false; }
	virtual void StartupModule() override;

	// IDynamicRHIModule
	virtual bool IsSupported() override;
	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) override;

private:
	FD3D11Adapter ChosenAdapter;

	// set MaxSupportedFeatureLevel and ChosenAdapter
	void FindAdapter();
};

// 1d, 31 bit (uses the sign bit for internal use), O(n) where n is the amount of elements stored
// does not enforce any alignment
// unoccupied regions get compacted but occupied don't get compacted
class FRangeAllocator
{
public:

	struct FRange
	{
		// not valid
		FRange()
			: Start(0)
			, Size(0)
		{
			check(!IsValid());
		}

		void SetOccupied(int32 InStart, int32 InSize)
		{
			check(InStart >= 0);
			check(InSize > 0);

			Start = InStart;
			Size = InSize;
			check(IsOccupied());
		}

		void SetUnOccupied(int32 InStart, int32 InSize)
		{
			check(InStart >= 0);
			check(InSize > 0);

			Start = InStart;
			Size = -InSize;
			check(!IsOccupied());
		}

		bool IsValid() { return Size != 0; }

		bool IsOccupied() const { return Size > 0; }
		uint32 ComputeSize() const { return (Size > 0) ? Size : -Size; }

		// @apram InSize can be <0 to remove from the size
		void ExtendUnoccupied(int32 InSize) { check(!IsOccupied()); Size -= InSize; }

		void MakeOccupied(int32 InSize) { check(InSize > 0); check(!IsOccupied()); Size = InSize; }
		void MakeUnOccupied() { check(IsOccupied()); Size = -Size; }

		bool operator==(const FRange& rhs) const { return Start == rhs.Start && Size == rhs.Size; }

		int32 GetStart() { return Start; }
		int32 GetEnd() { return Start + ComputeSize(); }

	private:
		// in bytes
		int32 Start;
		// in bytes, 0:not valid, <0:unoccupied, >0:occupied
		int32 Size;
	};
public:

	// constructor
	FRangeAllocator(uint32 TotalSize)
	{
		FRange NewRange;

		NewRange.SetUnOccupied(0, TotalSize);

		Entries.Add(NewRange);
	}

	// specified range must be non occupied
	void OccupyRange(FRange InRange)
	{
		check(InRange.IsValid());
		check(InRange.IsOccupied());

		for(uint32 i = 0, Num = Entries.Num(); i < Num; ++i)
		{
			FRange& ref = Entries[i];

			if(!ref.IsOccupied())
			{
				int32 OverlapSize = ref.GetEnd() - InRange.GetStart();

				if(OverlapSize > 0)
				{
					int32 FrontCutSize = InRange.GetStart() - ref.GetStart();

					// there is some front part we cut off
					if(FrontCutSize > 0)
					{
						FRange NewFrontRange;

						NewFrontRange.SetUnOccupied(InRange.GetStart(), ref.ComputeSize() - FrontCutSize);

						ref.SetUnOccupied(ref.GetStart(), FrontCutSize);

						++i;

						// remaining is added behind the found element
						Entries.Insert(NewFrontRange, i);

						// don't access ref or Num any more - Entries[] might be reallocated
					}

					check(Entries[i].GetStart() == InRange.GetStart());

					int32 BackCutSize = Entries[i].ComputeSize() - InRange.ComputeSize();

					// otherwise the range was already occupied or not enough space was left (internal error)
					check(BackCutSize >= 0);

					// there is some back part we cut off
					if(BackCutSize > 0)
					{
						FRange NewBackRange;

						NewBackRange.SetUnOccupied(Entries[i].GetStart() + InRange.ComputeSize(), BackCutSize);

						Entries.Insert(NewBackRange, i + 1);
					}

					Entries[i] = InRange;
					return;
				}
			}
		}
	}

	// @param InSize >0
	FRange AllocRange(uint32 InSize)//, uint32 Alignment)
	{
		check(InSize > 0);

		for(uint32 i = 0, Num = Entries.Num(); i < Num; ++i)
		{
			FRange& ref = Entries[i];

			if(!ref.IsOccupied())
			{
				uint32 RefSize = ref.ComputeSize();

				// take the first fitting one - later we could optimize for minimal fragmentation
				if(RefSize >= InSize)
				{
					ref.MakeOccupied(InSize);

					FRange Ret = ref;

					if(RefSize > InSize)
					{
						FRange NewRange;

						NewRange.SetUnOccupied(ref.GetEnd(), RefSize - InSize);

						// remaining is added behind the found element
						Entries.Insert(NewRange, i + 1);
					}
					return Ret;
				}
			}
		}

		// nothing found
		return FRange();
	}

	// @param In needs to be what was returned by AllocRange()
	void ReleaseRange(FRange In)
	{
		int32 Index = Entries.Find(In);

		check(Index != INDEX_NONE);

		FRange& refIndex = Entries[Index];

		refIndex.MakeUnOccupied();

		Compacten(Index);
	}

	// for debugging
	uint32 GetNumEntries() const { return Entries.Num(); }

	// for debugging
	uint32 ComputeUnoccupiedSize() const
	{
		uint32 Ret = 0;

		for(uint32 i = 0, Num = Entries.Num(); i < Num; ++i)
		{
			const FRange& ref = Entries[i];

			if(!ref.IsOccupied())
			{
				uint32 RefSize = ref.ComputeSize();

				Ret += RefSize;
			}
		}

		return Ret;
	}

private:
	// compact unoccupied ranges
	void Compacten(uint32 StartIndex)
	{
		check(!Entries[StartIndex].IsOccupied());

		if(StartIndex && !Entries[StartIndex-1].IsOccupied())
		{
			// Seems we can combine with the element before,
			// searching further is not needed as we assume the buffer was compact before the last change.
			--StartIndex;
		}

		uint32 ElementsToRemove = 0;
		uint32 SizeGained = 0;

		for(uint32 i = StartIndex + 1, Num = Entries.Num(); i < Num; ++i)
		{
			FRange& ref = Entries[i];

			if(!ref.IsOccupied())
			{
				++ElementsToRemove;
				SizeGained += ref.ComputeSize();
			}
			else
			{
				break;
			}
		}

		if(ElementsToRemove)
		{
			Entries.RemoveAt(StartIndex + 1, ElementsToRemove, EAllowShrinking::No);
			Entries[StartIndex].ExtendUnoccupied(SizeGained);
		}
	}

public:
	static void Test()
	{
		// testing code
#if !UE_BUILD_SHIPPING
		{
			// create
			FRangeAllocator A(10);
			check(A.GetNumEntries() == 1);
			check(A.ComputeUnoccupiedSize() == 10);

			// successfully alloc
			FRangeAllocator::FRange a = A.AllocRange(3);
			check(a.GetStart() == 0);
			check(a.GetEnd() == 3);
			check(a.IsOccupied());
			check(A.GetNumEntries() == 2);
			check(A.ComputeUnoccupiedSize() == 7);

			// successfully alloc
			FRangeAllocator::FRange b = A.AllocRange(4);
			check(b.GetStart() == 3);
			check(b.GetEnd() == 7);
			check(b.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 3);

			// failed alloc
			FRangeAllocator::FRange c = A.AllocRange(4);
			check(!c.IsValid());
			check(!c.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 3);

			// successfully alloc
			FRangeAllocator::FRange d = A.AllocRange(3);
			check(d.GetStart() == 7);
			check(d.GetEnd() == 10);
			check(d.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 0);

			A.ReleaseRange(b);
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 4);

			A.ReleaseRange(a);
			check(A.GetNumEntries() == 2);
			check(A.ComputeUnoccupiedSize() == 7);

			A.ReleaseRange(d);
			check(A.GetNumEntries() == 1);
			check(A.ComputeUnoccupiedSize() == 10);

			// we are back to a clean start

			FRangeAllocator::FRange e = A.AllocRange(10);
			check(e.GetStart() == 0);
			check(e.GetEnd() == 10);
			check(e.IsOccupied());
			check(A.GetNumEntries() == 1);
			check(A.ComputeUnoccupiedSize() == 0);

			A.ReleaseRange(e);
			check(A.GetNumEntries() == 1);
			check(A.ComputeUnoccupiedSize() == 10);

			// we are back to a clean start

			// create define range we want to block out
			FRangeAllocator::FRange f;
			f.SetOccupied(2, 4);
			A.OccupyRange(f);
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 6);

			FRangeAllocator::FRange g = A.AllocRange(2);
			check(g.GetStart() == 0);
			check(g.GetEnd() == 2);
			check(g.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 4);

			FRangeAllocator::FRange h = A.AllocRange(4);
			check(h.GetStart() == 6);
			check(h.GetEnd() == 10);
			check(h.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 0);
		}
#endif // !UE_BUILD_SHIPPING
	}

private:

	// ordered from small to large (for efficient compactening)
	TArray<FRange> Entries;
};

extern D3D11RHI_API FD3D11DynamicRHI*	GD3D11RHI;
