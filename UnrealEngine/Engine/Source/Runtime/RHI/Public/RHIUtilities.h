// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "HAL/IConsoleManager.h"
#include "RHIDefinitions.h"
#include "RHICommandList.h"
#include "Tasks/Task.h"

static inline EPixelFormat GetBlockCompressedFormatUAVAliasFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_DXT1:
	case PF_BC4:
		return PF_R32G32_UINT;

	case PF_DXT3:
	case PF_DXT5:
	case PF_BC5:
	case PF_BC6H:
	case PF_BC7:
		return PF_R32G32B32A32_UINT;
	}

	return Format;
}

static bool IsUnormFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_R5G6B5_UNORM:
	case PF_R16G16B16A16_UNORM:
	case PF_B5G5R5A1_UNORM:
		return true;

	default:
		break;
	}
	return false;
}

static bool IsSnormFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_R8G8B8A8_SNORM:
	case PF_R16G16B16A16_SNORM:
	case PF_G16R16_SNORM:
		return true;

	default:
		break;
	}
	return false;
}

static bool IsUintFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_R32_UINT:
	case PF_R16_UINT:
	case PF_R16G16B16A16_UINT:
	case PF_R32G32B32A32_UINT:
	case PF_R16G16_UINT:
	case PF_R8_UINT:
	case PF_R8G8B8A8_UINT:
	case PF_R32G32_UINT:
		return true;

	default:
		break;
	}
	return false;
}

static bool IsSintFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_R32_SINT:
	case PF_R16_SINT:
	case PF_R16G16B16A16_SINT:
		return true;

	default:
		break;
	}
	return false;
}

/** Get the best default resource state for the given texture creation flags */
extern RHI_API ERHIAccess RHIGetDefaultResourceState(ETextureCreateFlags InUsage, bool bInHasInitialData);

/** Get the best default resource state for the given buffer creation flags */
extern RHI_API ERHIAccess RHIGetDefaultResourceState(EBufferUsageFlags InUsage, bool bInHasInitialData);

/** Encapsulates a GPU read/write texture 2D with its UAV and SRV. */
struct FTextureRWBuffer
{
	FTextureRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes = 0;

	FTextureRWBuffer() = default;

	~FTextureRWBuffer()
	{
		Release();
	}

	static constexpr ETextureCreateFlags DefaultTextureInitFlag = TexCreate_ShaderResource | TexCreate_UAV;

	void Initialize2D(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 SizeX, uint32 SizeY, EPixelFormat Format, ETextureCreateFlags Flags = DefaultTextureInitFlag)
	{
		NumBytes = SizeX * SizeY * BytesPerElement;

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(InDebugName, SizeX, SizeY, Format)
			.SetFlags(Flags);

		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

		Buffer = RHICreateTexture(Desc);
		UAV = RHICmdList.CreateUnorderedAccessView(Buffer, 0);
		SRV = RHICmdList.CreateShaderResourceView(Buffer, 0);
	}

	void Initialize3D(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 SizeX, uint32 SizeY, uint32 SizeZ, EPixelFormat Format, ETextureCreateFlags Flags = DefaultTextureInitFlag)
	{
		NumBytes = SizeX * SizeY * SizeZ * BytesPerElement;

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create3D(InDebugName, SizeX, SizeY, SizeZ, Format)
			.SetFlags(Flags);

		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

		Buffer = RHICreateTexture(Desc);
		UAV = RHICmdList.CreateUnorderedAccessView(Buffer, 0);
		SRV = RHICmdList.CreateShaderResourceView(Buffer, 0);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read/write buffer with its UAV and SRV. */
struct FRWBuffer
{
	FBufferRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;
	FName ClassName = NAME_None;	// The owner class of Buffer used for Insight asset metadata tracing, set it before calling Initialize()
	FName OwnerName = NAME_None;	// The owner name used for Insight asset metadata tracing, set it before calling Initialize()

	FRWBuffer()
		: NumBytes(0)
	{}

	FRWBuffer(FRWBuffer&& Other)
		: Buffer(MoveTemp(Other.Buffer))
		, UAV(MoveTemp(Other.UAV))
		, SRV(MoveTemp(Other.SRV))
		, NumBytes(Other.NumBytes)
	{
		Other.NumBytes = 0;
	}

	FRWBuffer(const FRWBuffer& Other)
		: Buffer(Other.Buffer)
		, UAV(Other.UAV)
		, SRV(Other.SRV)
		, NumBytes(Other.NumBytes)
	{
	}

	FRWBuffer& operator=(FRWBuffer&& Other)
	{
		Buffer = MoveTemp(Other.Buffer);
		UAV = MoveTemp(Other.UAV);
		SRV = MoveTemp(Other.SRV);
		NumBytes = Other.NumBytes;
		Other.NumBytes = 0;

		return *this;
	}

	FRWBuffer& operator=(const FRWBuffer& Other)
	{
		Buffer = Other.Buffer;
		UAV = Other.UAV;
		SRV = Other.SRV;
		NumBytes = Other.NumBytes;

		return *this;
	}

	~FRWBuffer()
	{
		Release();
	}

	// @param AdditionalUsage passed down to RHICreateVertexBuffer(), get combined with "BUF_UnorderedAccess | BUF_ShaderResource" e.g. BUF_Static
	void Initialize(FRHICommandListBase& RHICmdList, const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, ERHIAccess InResourceState, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface *InResourceArray = nullptr)
	{
		// Provide a debug name if using Fast VRAM so the allocators diagnostics will work
		ensure(!(EnumHasAnyFlags(AdditionalUsage, BUF_FastVRAM) && !InDebugName));
		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo(InDebugName);
		CreateInfo.ClassName = ClassName;
		CreateInfo.OwnerName = OwnerName;
		CreateInfo.ResourceArray = InResourceArray;
		Buffer = RHICmdList.CreateBuffer(NumBytes, BUF_VertexBuffer | BUF_UnorderedAccess | BUF_ShaderResource | AdditionalUsage, 0, InResourceState, CreateInfo);
		UAV = RHICmdList.CreateUnorderedAccessView(Buffer, UE_PIXELFORMAT_TO_UINT8(Format));
		SRV = RHICmdList.CreateShaderResourceView(Buffer, BytesPerElement, UE_PIXELFORMAT_TO_UINT8(Format));
	}

	void Initialize(FRHICommandListBase& RHICmdList, const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface* InResourceArray = nullptr)
	{
		Initialize(RHICmdList, InDebugName, BytesPerElement, NumElements, Format, ERHIAccess::UAVCompute, AdditionalUsage, InResourceArray);
	}

	UE_DEPRECATED(5.3, "Initialize now requires a command list.")
	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface* InResourceArray = nullptr)
	{
		check(IsInRenderingThread());
		Initialize(FRHICommandListExecutor::GetImmediateCommandList(), InDebugName, BytesPerElement, NumElements, Format, ERHIAccess::UAVCompute, AdditionalUsage, InResourceArray);
	}

	UE_DEPRECATED(5.3, "Initialize now requires a command list.")
	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, ERHIAccess InResourceState, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface* InResourceArray = nullptr)
	{
		check(IsInRenderingThread());
		Initialize(FRHICommandListExecutor::GetImmediateCommandList(), InDebugName, BytesPerElement, NumElements, Format, InResourceState, AdditionalUsage, InResourceArray);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read only texture 2D with its SRV. */
struct FTextureReadBuffer2D
{
	FTexture2DRHIRef Buffer;	
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FTextureReadBuffer2D()
		: NumBytes(0)
	{}

	~FTextureReadBuffer2D()
	{
		Release();
	}

	const static ETextureCreateFlags DefaultTextureInitFlag = ETextureCreateFlags::ShaderResource;
	void Initialize(const TCHAR* InDebugName, const uint32 BytesPerElement, const uint32 SizeX, const uint32 SizeY, const EPixelFormat Format, ETextureCreateFlags Flags = DefaultTextureInitFlag, FResourceBulkDataInterface* InBulkData = nullptr)
	{
		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

		NumBytes = SizeX * SizeY * BytesPerElement;

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(InDebugName, SizeX, SizeY, Format)
			.SetFlags(Flags)
			.SetBulkData(InBulkData);

		Buffer = RHICreateTexture(Desc);
		
		SRV = RHICmdList.CreateShaderResourceView(Buffer, 0);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();		
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read buffer with its SRV. */
struct FReadBuffer
{
	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FReadBuffer(): NumBytes(0) {}

	void Initialize(FRHICommandListBase& RHICmdList, const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface* InResourceArray = nullptr)
	{
		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo(InDebugName);
		CreateInfo.ResourceArray = InResourceArray;
		Buffer = RHICmdList.CreateVertexBuffer(NumBytes, BUF_ShaderResource | AdditionalUsage, ERHIAccess::SRVMask, CreateInfo);
		SRV = RHICmdList.CreateShaderResourceView(Buffer, BytesPerElement, UE_PIXELFORMAT_TO_UINT8(Format));
	}

	UE_DEPRECATED(5.3, "Initialize now requires a command list.")
	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface* InResourceArray = nullptr)
	{
		Initialize(FRHICommandListImmediate::Get(), InDebugName, BytesPerElement, NumElements, Format, AdditionalUsage, InResourceArray);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read/write structured buffer with its UAV and SRV. */
struct FRWBufferStructured
{
	FBufferRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FRWBufferStructured(): NumBytes(0) {}

	~FRWBufferStructured()
	{
		Release();
	}

	void Initialize(FRHICommandListBase& RHICmdList, const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EBufferUsageFlags AdditionalUsage = BUF_None, bool bUseUavCounter = false, bool bAppendBuffer = false, ERHIAccess InitialState = ERHIAccess::UAVMask)
	{
		// Provide a debug name if using Fast VRAM so the allocators diagnostics will work
		ensure(!(EnumHasAnyFlags(AdditionalUsage, BUF_FastVRAM) && !InDebugName));

		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo(InDebugName);
		Buffer = RHICmdList.CreateStructuredBuffer(BytesPerElement, NumBytes, BUF_UnorderedAccess | BUF_ShaderResource | AdditionalUsage, InitialState, CreateInfo);
		UAV = RHICmdList.CreateUnorderedAccessView(Buffer, bUseUavCounter, bAppendBuffer);
		SRV = RHICmdList.CreateShaderResourceView(Buffer);
	}

	UE_DEPRECATED(5.3, "Initialize now requires a command list.")
	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EBufferUsageFlags AdditionalUsage = BUF_None, bool bUseUavCounter = false, bool bAppendBuffer = false, ERHIAccess InitialState = ERHIAccess::UAVMask)
	{
		Initialize(FRHICommandListImmediate::Get(), InDebugName, BytesPerElement, NumElements, AdditionalUsage, bUseUavCounter, bAppendBuffer, InitialState);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}
};

struct FByteAddressBuffer
{
	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FByteAddressBuffer(): NumBytes(0) {}

	void Initialize(FRHICommandListBase& RHICmdList, const TCHAR* InDebugName, uint32 InNumBytes, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		NumBytes = InNumBytes;
		check( NumBytes % 4 == 0 );
		FRHIResourceCreateInfo CreateInfo(InDebugName);
		Buffer = RHICmdList.CreateStructuredBuffer(4, NumBytes, BUF_ShaderResource | BUF_ByteAddressBuffer | AdditionalUsage, ERHIAccess::SRVMask, CreateInfo);
		SRV = RHICmdList.CreateShaderResourceView(Buffer);
	}

	UE_DEPRECATED(5.3, "Initialize now requires a command list.")
	void Initialize(const TCHAR* InDebugName, uint32 InNumBytes, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		Initialize(FRHICommandListImmediate::Get(), InDebugName, InNumBytes, AdditionalUsage);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read/write ByteAddress buffer with its UAV and SRV. */
struct FRWByteAddressBuffer : public FByteAddressBuffer
{
	FUnorderedAccessViewRHIRef UAV;

	void Initialize(FRHICommandListBase& RHICmdList, const TCHAR* DebugName, uint32 InNumBytes, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		FByteAddressBuffer::Initialize(RHICmdList, DebugName, InNumBytes, BUF_UnorderedAccess | AdditionalUsage);
		UAV = RHICmdList.CreateUnorderedAccessView(Buffer, false, false);
	}

	UE_DEPRECATED(5.3, "Initialize now requires a command list.")
	void Initialize(const TCHAR* DebugName, uint32 InNumBytes, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		Initialize(FRHICommandListImmediate::Get(), DebugName, InNumBytes, AdditionalUsage);
	}

	void Release()
	{
		FByteAddressBuffer::Release();
		UAV.SafeRelease();
	}
};

struct FDynamicReadBuffer : public FReadBuffer
{
	/** Pointer to the vertex buffer mapped in main memory. */
	uint8* MappedBuffer;

	/** Default constructor. */
	FDynamicReadBuffer()
		: MappedBuffer(nullptr)
	{
	}

	virtual ~FDynamicReadBuffer()
	{
		Release();
	}

	void Initialize(FRHICommandListBase& RHICmdList, const TCHAR* DebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		ensure(
			EnumHasAnyFlags(AdditionalUsage, BUF_Dynamic | BUF_Volatile | BUF_Static) &&					// buffer should be Dynamic or Volatile or Static
			EnumHasAnyFlags(AdditionalUsage, BUF_Dynamic) != EnumHasAnyFlags(AdditionalUsage, BUF_Volatile) // buffer should not be both
			);

		FReadBuffer::Initialize(RHICmdList, DebugName, BytesPerElement, NumElements, Format, AdditionalUsage);
	}

	UE_DEPRECATED(5.3, "Initialize now requires a command list.")
	void Initialize(const TCHAR* DebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		Initialize(FRHICommandListImmediate::Get(), DebugName, BytesPerElement, NumElements, Format, AdditionalUsage);
	}

	/**
	* Locks the vertex buffer so it may be written to.
	*/
	void Lock(FRHICommandListBase& RHICmdList)
	{
		check(MappedBuffer == nullptr);
		check(IsValidRef(Buffer));
		MappedBuffer = (uint8*)RHICmdList.LockBuffer(Buffer, 0, NumBytes, RLM_WriteOnly);
	}

	UE_DEPRECATED(5.3, "Lock now requires a command list.")
	void Lock()
	{
		Lock(FRHICommandListImmediate::Get());
	}

	/**
	* Unocks the buffer so the GPU may read from it.
	*/
	void Unlock(FRHICommandListBase& RHICmdList)
	{
		check(MappedBuffer);
		check(IsValidRef(Buffer));
		RHICmdList.UnlockBuffer(Buffer);
		MappedBuffer = nullptr;
	}

	UE_DEPRECATED(5.3, "Unlock now requires a command list.")
	void Unlock()
	{
		Unlock(FRHICommandListImmediate::Get());
	}
};

/**
 * Convert the ESimpleRenderTargetMode into usable values 
 * @todo: Can we easily put this into a .cpp somewhere?
 */
RHI_API void DecodeRenderTargetMode(ESimpleRenderTargetMode Mode, ERenderTargetLoadAction& ColorLoadAction, ERenderTargetStoreAction& ColorStoreAction, ERenderTargetLoadAction& DepthLoadAction, ERenderTargetStoreAction& DepthStoreAction, ERenderTargetLoadAction& StencilLoadAction, ERenderTargetStoreAction& StencilStoreAction, FExclusiveDepthStencil DepthStencilUsage);

inline void TransitionRenderPassTargets(FRHICommandList& RHICmdList, const FRHIRenderPassInfo& RPInfo)
{
	FRHITransitionInfo Transitions[MaxSimultaneousRenderTargets];
	int32 TransitionIndex = 0;
	uint32 NumColorRenderTargets = RPInfo.GetNumColorRenderTargets();
	for (uint32 Index = 0; Index < NumColorRenderTargets; Index++)
	{
		const FRHIRenderPassInfo::FColorEntry& ColorRenderTarget = RPInfo.ColorRenderTargets[Index];
		if (ColorRenderTarget.RenderTarget != nullptr)
		{
			Transitions[TransitionIndex] = FRHITransitionInfo(ColorRenderTarget.RenderTarget, ERHIAccess::Unknown, ERHIAccess::RTV);
			TransitionIndex++;
		}
	}

	const FRHIRenderPassInfo::FDepthStencilEntry& DepthStencilTarget = RPInfo.DepthStencilRenderTarget;
	if (DepthStencilTarget.DepthStencilTarget != nullptr && (RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsAnyWrite()))
	{
		RHICmdList.Transition(FRHITransitionInfo(DepthStencilTarget.DepthStencilTarget, ERHIAccess::Unknown, ERHIAccess::DSVRead | ERHIAccess::DSVWrite));
	}

	RHICmdList.Transition(MakeArrayView(Transitions, TransitionIndex));
}

/** Performs a clear render pass on an RHI texture. The texture is expected to be in the RTV state. */
inline void ClearRenderTarget(FRHICommandList& RHICmdList, FRHITexture* Texture, uint32 MipIndex = 0, uint32 ArraySlice = 0)
{
	check(Texture);
	const FIntPoint Extent = Texture->GetSizeXY();
	FRHIRenderPassInfo Info(Texture, ERenderTargetActions::Clear_Store);
	Info.ColorRenderTargets[0].MipIndex = (uint8)MipIndex;
	Info.ColorRenderTargets[0].ArraySlice = (int32)ArraySlice;
	RHICmdList.BeginRenderPass(Info, TEXT("ClearRenderTarget"));
	RHICmdList.EndRenderPass();
}

inline void TransitionAndCopyTexture(FRHICommandList& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FRHICopyTextureInfo& Info)
{
	check(SrcTexture && DstTexture);
	check(SrcTexture->GetNumSamples() == DstTexture->GetNumSamples());

	if (SrcTexture == DstTexture)
	{
		RHICmdList.Transition({
			FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask)
		});
		return;
	}

	RHICmdList.Transition({
		FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc),
		FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest)
	});

	RHICmdList.CopyTexture(SrcTexture, DstTexture, Info);

	RHICmdList.Transition({
		FRHITransitionInfo(SrcTexture, ERHIAccess::CopySrc,  ERHIAccess::SRVMask),
		FRHITransitionInfo(DstTexture, ERHIAccess::CopyDest, ERHIAccess::SRVMask)
	});
}

/**
 * Computes the vertex count for a given number of primitives of the specified type.
 * @param NumPrimitives The number of primitives.
 * @param PrimitiveType The type of primitives.
 * @returns The number of vertices.
 */
inline uint32 GetVertexCountForPrimitiveCount(uint32 NumPrimitives, uint32 PrimitiveType)
{
	static_assert(PT_Num == 6, "This function needs to be updated");
	uint32 Factor = (PrimitiveType == PT_TriangleList) ? 3 : (PrimitiveType == PT_LineList) ? 2 : (PrimitiveType == PT_RectList) ? 3 : 1;
	uint32 Offset = (PrimitiveType == PT_TriangleStrip) ? 2 : 0;

	return NumPrimitives * Factor + Offset;

}

inline uint32 ComputeAnisotropyRT(int32 InitializerMaxAnisotropy)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaxAnisotropy"));
	int32 CVarValue = CVar->GetValueOnAnyThread(); // this is sometimes called from main thread during initialization of static RHI states

	return FMath::Clamp(InitializerMaxAnisotropy > 0 ? InitializerMaxAnisotropy : CVarValue, 1, 16);
}

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
#define ENABLE_TRANSITION_DUMP 0
#else
#define ENABLE_TRANSITION_DUMP 1
#endif

class FDumpTransitionsHelper
{
public:
	RHI_API static void DumpResourceTransition(const FName& ResourceName, const ERHIAccess TransitionType);
	
private:
	static void DumpTransitionForResourceHandler();

	static TAutoConsoleVariable<FString> CVarDumpTransitionsForResource;
	static FAutoConsoleVariableSink CVarDumpTransitionsForResourceSink;
	static FName DumpTransitionForResource;
};

#if ENABLE_TRANSITION_DUMP
#define DUMP_TRANSITION(ResourceName, TransitionType) FDumpTransitionsHelper::DumpResourceTransition(ResourceName, TransitionType);
#else
#define DUMP_TRANSITION(ResourceName, TransitionType)
#endif

extern RHI_API void SetDepthBoundsTest(FRHICommandList& RHICmdList, float WorldSpaceDepthNear, float WorldSpaceDepthFar, const FMatrix& ProjectionMatrix);

/** Returns the value of the rhi.SyncInterval CVar. */
extern RHI_API uint32 RHIGetSyncInterval();

/** Returns the value of the rhi.SyncSlackMS CVar or length of a full frame interval if the frame offset system is disabled. */
extern RHI_API float RHIGetSyncSlackMS();

/** Returns the top and bottom vsync present thresholds (the values of rhi.PresentThreshold.Top and rhi.PresentThreshold.Bottom) */
extern RHI_API void RHIGetPresentThresholds(float& OutTopPercent, float& OutBottomPercent);

/** Returns the value of the rhi.SyncAllowVariable CVar. */
extern RHI_API bool RHIGetSyncAllowVariable();

/** Signals the completion of the specified task graph event when the given frame has flipped. */
UE_DEPRECATED(5.4, "RHICompleteGraphEventOnFlip is replaced with RHITriggerTaskEventOnFlip")
inline void RHICompleteGraphEventOnFlip(uint64 PresentIndex, FGraphEventRef Event) { checkNoEntry(); }

extern RHI_API void RHITriggerTaskEventOnFlip(uint64 PresentIndex, const UE::Tasks::FTaskEvent& TaskEvent);

/** Sets the FrameIndex and InputTime for the current frame. */
extern RHI_API void RHISetFrameDebugInfo(uint64 PresentIndex, uint64 FrameIndex, uint64 InputTime);

extern RHI_API void RHIInitializeFlipTracking();
extern RHI_API void RHIShutdownFlipTracking();

/** Sets the FrameIndex and InputTime for the current frame. */
extern RHI_API float RHIGetFrameTime();

extern RHI_API void RHICalculateFrameTime();

/** Returns the VendorID of the preferred vendor or -1 if none were specified. */
extern RHI_API EGpuVendorId RHIGetPreferredAdapterVendor();

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "RHILockTracker.h"
#include "PixelFormat.h"
#endif
