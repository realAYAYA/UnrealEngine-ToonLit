// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanState.h"
#include "VulkanUtil.h"
#include "BoundShaderStateCache.h"
#include "VulkanShaderResources.h"
#include "VulkanState.h"
#include "VulkanMemory.h"
#include "Misc/ScopeRWLock.h"

class FVulkanDevice;
class FVulkanQueue;
class FVulkanCmdBuffer;
class FVulkanBufferCPU;
class FVulkanTexture;
struct FVulkanBufferView;
class FVulkanResourceMultiBuffer;
class FVulkanLayout;
class FVulkanOcclusionQuery;
class FVulkanShaderResourceView;
class FVulkanCommandBufferManager;
struct FRHITransientHeapAllocation;

namespace VulkanRHI
{
	class FDeviceMemoryAllocation;
	class FOldResourceAllocation;
	struct FPendingBufferLock;
	class FVulkanViewBase;
}

enum
{
	NUM_OCCLUSION_QUERIES_PER_POOL = 4096,

	NUM_TIMESTAMP_QUERIES_PER_POOL = 1024,
};

// Mirror GPixelFormats with format information for buffers
extern VkFormat GVulkanBufferFormat[PF_MAX];

// Converts the internal texture dimension to Vulkan view type
inline VkImageViewType UETextureDimensionToVkImageViewType(ETextureDimension Dimension)
{
	switch (Dimension)
	{
	case ETextureDimension::Texture2D: return VK_IMAGE_VIEW_TYPE_2D;
	case ETextureDimension::Texture2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	case ETextureDimension::Texture3D: return VK_IMAGE_VIEW_TYPE_3D;
	case ETextureDimension::TextureCube: return VK_IMAGE_VIEW_TYPE_CUBE;
	case ETextureDimension::TextureCubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	default: checkNoEntry(); return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	}
}

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FVulkanVertexDeclaration : public FRHIVertexDeclaration
{
public:
	FVertexDeclarationElementList Elements;

	FVulkanVertexDeclaration(const FVertexDeclarationElementList& InElements);

	virtual bool GetInitializer(FVertexDeclarationElementList& Out) final override
	{
		Out = Elements;
		return true;
	}

	static void EmptyCache();
};

struct FGfxPipelineDesc;

class FVulkanShaderModule : public FThreadSafeRefCountedObject
{
	static FVulkanDevice* Device;
	VkShaderModule ActualShaderModule;
public:
	FVulkanShaderModule(FVulkanDevice* DeviceIn, VkShaderModule ShaderModuleIn) : ActualShaderModule(ShaderModuleIn) 
	{
		check(DeviceIn && (Device == DeviceIn || !Device));
		Device = DeviceIn;
	}
	virtual ~FVulkanShaderModule();
	VkShaderModule& GetVkShaderModule() { return ActualShaderModule; }
};

class FVulkanShader : public IRefCountedObject
{
	static FCriticalSection VulkanShaderModulesMapCS;

public:
	FVulkanShader(FVulkanDevice* InDevice, EShaderFrequency InFrequency, VkShaderStageFlagBits InStageFlag)
		: ShaderKey(0)
		, StageFlag(InStageFlag)
		, Frequency(InFrequency)
		, Device(InDevice)
	{
	}

	virtual ~FVulkanShader();

	void PurgeShaderModules();

	void Setup(TArrayView<const uint8> InShaderHeaderAndCode, uint64 InShaderKey);

	TRefCountPtr<FVulkanShaderModule> GetOrCreateHandle(const FVulkanLayout* Layout, uint32 LayoutHash)
	{
		FScopeLock Lock(&VulkanShaderModulesMapCS);
		TRefCountPtr<FVulkanShaderModule>* Found = ShaderModules.Find(LayoutHash);
		if (Found)
		{
			return *Found;
		}

		return CreateHandle(Layout, LayoutHash);
	}
	
	TRefCountPtr<FVulkanShaderModule> GetOrCreateHandle(const FGfxPipelineDesc& Desc, const FVulkanLayout* Layout, uint32 LayoutHash)
	{
		FScopeLock Lock(&VulkanShaderModulesMapCS);
		if (NeedsSpirvInputAttachmentPatching(Desc))
		{
			LayoutHash = HashCombine(LayoutHash, 1);
		}
		
		TRefCountPtr<FVulkanShaderModule>* Found = ShaderModules.Find(LayoutHash);
		if (Found)
		{
			return *Found;
		}

		return CreateHandle(Desc, Layout, LayoutHash);
	}

	inline const FString& GetDebugName() const
	{
		return CodeHeader.DebugName;
	}

	// Name should be pointing to "main_"
	void GetEntryPoint(ANSICHAR* Name, int32 NameLength)
	{
		FCStringAnsi::Snprintf(Name, NameLength, "main_%0.8x_%0.8x", SpirvContainer.GetSizeBytes(), CodeHeader.SpirvCRC);
	}

	FORCEINLINE const FVulkanShaderHeader& GetCodeHeader() const
	{
		return CodeHeader;
	}

	inline uint64 GetShaderKey() const
	{
		return ShaderKey;
	}

	// This provides a view of the raw spirv bytecode.
	// If it is stored compressed then the result of GetSpirvCode will contain the decompressed spirv.
	class FSpirvCode
	{
		friend class FVulkanShader;
		explicit FSpirvCode(TArray<uint32>&& UncompressedCodeIn) : UncompressedCode(MoveTemp(UncompressedCodeIn))
		{
			CodeView = UncompressedCode;
		}
		explicit FSpirvCode(TArrayView<uint32> UncompressedCodeView) : CodeView(UncompressedCodeView)	{	}
		TArrayView<uint32> CodeView;
		TArray<uint32> UncompressedCode;
	public:
		TArrayView<uint32> GetCodeView() {return CodeView;}
	};
	FSpirvCode GetSpirvCode();
	FSpirvCode GetPatchedSpirvCode(const FGfxPipelineDesc& Desc, const FVulkanLayout* Layout);
protected:
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	FString							DebugEntryPoint;
#endif
	uint64							ShaderKey;

	/** External bindings for this shader. */
	FVulkanShaderHeader				CodeHeader;
	TMap<uint32, TRefCountPtr<FVulkanShaderModule>>	ShaderModules;
	const VkShaderStageFlagBits		StageFlag;
	EShaderFrequency				Frequency;

	TArray<FUniformBufferStaticSlot> StaticSlots;

private:
	class FSpirvContainer
	{
		friend class FVulkanShader;
		TArray<uint8>	SpirvCode;
		int32 UncompressedSizeBytes = -1;
	public:
		bool IsCompressed() const {	return UncompressedSizeBytes != -1;	}
		int32 GetSizeBytes() const { return UncompressedSizeBytes >= 0 ? UncompressedSizeBytes : SpirvCode.Num(); }
		friend FArchive& operator<<(FArchive& Ar, class FVulkanShader::FSpirvContainer& SpirvContainer);
	} SpirvContainer;

	friend FArchive& operator<<(FArchive& Ar, class FVulkanShader::FSpirvContainer& SpirvContainer);
	static FSpirvCode PatchSpirvInputAttachments(FSpirvCode& SpirvCode);

protected:

	FVulkanDevice*					Device;

	TRefCountPtr<FVulkanShaderModule> CreateHandle(const FVulkanLayout* Layout, uint32 LayoutHash);
	TRefCountPtr<FVulkanShaderModule> CreateHandle(const FGfxPipelineDesc& Desc, const FVulkanLayout* Layout, uint32 LayoutHash);

	bool NeedsSpirvInputAttachmentPatching(const FGfxPipelineDesc& Desc) const;

	friend class FVulkanCommandListContext;
	friend class FVulkanPipelineStateCacheManager;
	friend class FVulkanComputeShaderState;
	friend class FVulkanComputePipeline;
	friend class FVulkanShaderFactory;
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template<typename BaseResourceType, EShaderFrequency ShaderType, VkShaderStageFlagBits StageFlagBits>
class TVulkanBaseShader : public BaseResourceType, public FVulkanShader
{
private:
	TVulkanBaseShader(FVulkanDevice* InDevice) :
		FVulkanShader(InDevice, ShaderType, StageFlagBits)
	{
	}
	friend class FVulkanShaderFactory;
public:
	enum { StaticFrequency = ShaderType };

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}
};

typedef TVulkanBaseShader<FRHIVertexShader, SF_Vertex, VK_SHADER_STAGE_VERTEX_BIT>					FVulkanVertexShader;
typedef TVulkanBaseShader<FRHIPixelShader, SF_Pixel, VK_SHADER_STAGE_FRAGMENT_BIT>					FVulkanPixelShader;
typedef TVulkanBaseShader<FRHIComputeShader, SF_Compute, VK_SHADER_STAGE_COMPUTE_BIT>				FVulkanComputeShader;
typedef TVulkanBaseShader<FRHIGeometryShader, SF_Geometry, VK_SHADER_STAGE_GEOMETRY_BIT>			FVulkanGeometryShader;

#if VULKAN_RHI_RAYTRACING
typedef TVulkanBaseShader<FRHIRayGenShader, SF_RayGen, VK_SHADER_STAGE_RAYGEN_BIT_KHR>					FVulkanRayGenShader;
typedef TVulkanBaseShader<FRHIRayMissShader, SF_RayMiss, VK_SHADER_STAGE_MISS_BIT_KHR>					FVulkanRayMissShader;
typedef TVulkanBaseShader<FRHIRayCallableShader, SF_RayCallable, VK_SHADER_STAGE_CALLABLE_BIT_KHR>		FVulkanRayCallableShader;
typedef TVulkanBaseShader<FRHIRayHitGroupShader, SF_RayHitGroup, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR>	FVulkanRayHitGroupShader; // vkrt todo: How to handle VK_SHADER_STAGE_ANY_HIT_BIT_KHR?
#endif // VULKAN_RHI_RAYTRACING

class FVulkanShaderFactory
{
public:
	~FVulkanShaderFactory();

	template <typename ShaderType>
	ShaderType* CreateShader(TArrayView<const uint8> Code, FVulkanDevice* Device);

	template <typename ShaderType>
	ShaderType* LookupShader(uint64 ShaderKey) const
	{
		if (ShaderKey)
		{
			FRWScopeLock ScopedLock(Lock, SLT_ReadOnly);
			FVulkanShader* const * FoundShaderPtr = ShaderMap[ShaderType::StaticFrequency].Find(ShaderKey);
			if (FoundShaderPtr)
			{
				return static_cast<ShaderType*>(*FoundShaderPtr);
			}
		}
		return nullptr;
	}

	void LookupShaders(const uint64 InShaderKeys[ShaderStage::NumStages], FVulkanShader* OutShaders[ShaderStage::NumStages]) const;

	void OnDeleteShader(const FVulkanShader& Shader);

private:
	mutable FRWLock Lock;
	TMap<uint64, FVulkanShader*> ShaderMap[SF_NumFrequencies];
};

class FVulkanBoundShaderState : public FRHIBoundShaderState
{
public:
	FVulkanBoundShaderState(
		FRHIVertexDeclaration* InVertexDeclarationRHI,
		FRHIVertexShader* InVertexShaderRHI,
		FRHIPixelShader* InPixelShaderRHI,
		FRHIGeometryShader* InGeometryShaderRHI
	);

	virtual ~FVulkanBoundShaderState();

	FORCEINLINE FVulkanVertexShader*   GetVertexShader() const { return (FVulkanVertexShader*)CacheLink.GetVertexShader(); }
	FORCEINLINE FVulkanPixelShader*    GetPixelShader() const { return (FVulkanPixelShader*)CacheLink.GetPixelShader(); }
	FORCEINLINE FVulkanGeometryShader* GetGeometryShader() const { return (FVulkanGeometryShader*)CacheLink.GetGeometryShader(); }

	const FVulkanShader* GetShader(ShaderStage::EStage Stage) const
	{
		switch (Stage)
		{
		case ShaderStage::Vertex:		return GetVertexShader();
		case ShaderStage::Pixel:		return GetPixelShader();
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		case ShaderStage::Geometry:	return GetGeometryShader();
#endif
		default: break;
		}
		checkf(0, TEXT("Invalid Shader Frequency %d"), (int32)Stage);
		return nullptr;
	}

private:
	FCachedBoundShaderStateLink_Threadsafe CacheLink;
};

struct FVulkanCpuReadbackBuffer
{
	VkBuffer Buffer;
	uint32 MipOffsets[MAX_TEXTURE_MIP_COUNT];
	uint32 MipSize[MAX_TEXTURE_MIP_COUNT];
};


enum class EImageOwnerType : uint8
{
	None,
	LocalOwner,
	ExternalOwner,
	Aliased
};


struct FVulkanTextureView
{
	FVulkanTextureView()
		: View(VK_NULL_HANDLE)
		, Image(VK_NULL_HANDLE)
		, ViewId(0)
	{
	}

	void Create(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle = false, VkImageUsageFlags ImageUsageFlags = 0);
	void Destroy(FVulkanDevice& Device);

	VkImageView View;
	VkImage Image;
	uint32 ViewId;
};


class FVulkanTexture : public FRHITexture, public FVulkanEvictable
{
public:
	// Regular constructor.
	FVulkanTexture(FVulkanDevice& InDevice, const FRHITextureCreateDesc& InCreateDesc, const FRHITransientHeapAllocation* InTransientHeapAllocation);

	// Construct from external resource.
	// FIXME: HUGE HACK: the bUnused argument is there to disambiguate this overload from the one above when passing nullptr, since nullptr is a valid VkImage. Get rid of this code smell when unifying FVulkanSurface and FVulkanTexture.
	FVulkanTexture(FVulkanDevice& InDevice, const FRHITextureCreateDesc& InCreateDesc, VkImage InImage, bool bUnused);

	// Aliasing constructor.
	FVulkanTexture(FVulkanDevice& InDevice, const FRHITextureCreateDesc& InCreateDesc, FTextureRHIRef& SrcTextureRHI);

	virtual ~FVulkanTexture();

	void AliasTextureResources(FTextureRHIRef& SrcTextureRHI);

	// View with all mips/layers
	FVulkanTextureView DefaultView;
	// View with all mips/layers, but if it's a Depth/Stencil, only the Depth view
	FVulkanTextureView* PartialView;

	FTextureRHIRef AliasedTexture;

	virtual void OnLayoutTransition(FVulkanCommandListContext& Context, VkImageLayout NewLayout) {}

	template<typename T>
	void DumpMemory(T Callback)
	{
		const FIntVector SizeXYZ = GetSizeXYZ();
		Callback(TEXT("FVulkanTexture"), GetName(), this, static_cast<FRHIResource*>(this), SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, StorageFormat);
	}

	// FVulkanEvictable interface.
	bool CanMove() const override { return false; }
	bool CanEvict() const override { return false; }
	void Evict(FVulkanDevice& Device, FVulkanCommandListContext& Context) override; ///evict to system memory
	void Move(FVulkanDevice& Device, FVulkanCommandListContext& Context, VulkanRHI::FVulkanAllocation& NewAllocation) override; //move to a full new allocation
	FVulkanTexture* GetEvictableTexture() override { return this; }
	
	void AttachView(VulkanRHI::FVulkanViewBase* View);
	void DetachView(VulkanRHI::FVulkanViewBase* View);

	bool GetTextureResourceInfo(FRHIResourceInfo& OutResourceInfo) const;

	void* GetNativeResource() const override final { return (void*)Image; }
	void* GetTextureBaseRHI() override final { return this; }

	inline static FVulkanTexture* Cast(FRHITexture* Texture)
	{
		check(Texture);
		return (FVulkanTexture*)Texture->GetTextureBaseRHI();
	}

public:
	struct FImageCreateInfo
	{
		VkImageCreateInfo ImageCreateInfo;
		//only used when HasImageFormatListKHR is supported. Otherise VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT is used.
		VkImageFormatListCreateInfoKHR ImageFormatListCreateInfo;
#if VULKAN_SUPPORTS_EXTERNAL_MEMORY
		//used when TexCreate_External is given
		VkExternalMemoryImageCreateInfoKHR ExternalMemImageCreateInfo;
#endif // VULKAN_SUPPORTS_EXTERNAL_MEMORY
		VkFormat FormatsUsed[2];
	};

	// Seperate method for creating VkImageCreateInfo
	static void GenerateImageCreateInfo(
		FImageCreateInfo& OutImageCreateInfo,
		FVulkanDevice& InDevice,
		const FRHITextureDesc& InDesc,
		VkFormat* OutStorageFormat = nullptr,
		VkFormat* OutViewFormat = nullptr,
		bool bForceLinearTexture = false);

	void DestroySurface();
	void InvalidateMappedMemory();
	void* GetMappedPointer();

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize() const
	{
		return MemoryRequirements.size;
	}

	/**
	 * Returns one of the texture's mip-maps stride.
	 */
	void GetMipStride(uint32 MipIndex, uint32& Stride);

	/*
	 * Returns the memory offset to the texture's mip-map.
	 */
	void GetMipOffset(uint32 MipIndex, uint32& Offset);

	/**
	* Returns how much memory a single mip uses.
	*/
	void GetMipSize(uint32 MipIndex, uint32& MipBytes);

	inline VkImageViewType GetViewType() const
	{
		return UETextureDimensionToVkImageViewType(GetDesc().Dimension);
	}

	inline VkImageTiling GetTiling() const { return Tiling; }

	inline uint32 GetNumberOfArrayLevels() const
	{
		switch (GetViewType())
		{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_3D:
			return 1;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			return GetDesc().ArraySize;
		case VK_IMAGE_VIEW_TYPE_CUBE:
			return 6;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			return 6 * GetDesc().ArraySize;
		default:
			ErrorInvalidViewType();
			return 1;
		}
	}
	VULKANRHI_API void ErrorInvalidViewType() const;

	// Full includes Depth+Stencil
	inline VkImageAspectFlags GetFullAspectMask() const
	{
		return FullAspectMask;
	}

	// Only Depth or Stencil
	inline VkImageAspectFlags GetPartialAspectMask() const
	{
		return PartialAspectMask;
	}

	inline bool IsDepthOrStencilAspect() const
	{
		return (FullAspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
	}

	inline bool IsImageOwner() const
	{
		return (ImageOwnerType == EImageOwnerType::LocalOwner);
	}

	inline bool SupportsSampling() const
	{
		return EnumHasAllFlags(GPixelFormats[GetDesc().Format].Capabilities, EPixelFormatCapabilities::TextureSample);
	}

	VULKANRHI_API VkDeviceMemory GetAllocationHandle() const;
	VULKANRHI_API uint64 GetAllocationOffset() const;

	static void InternalLockWrite(FVulkanCommandListContext& Context, FVulkanTexture* Surface, const VkBufferImageCopy& Region, VulkanRHI::FStagingBuffer* StagingBuffer);

	const FVulkanCpuReadbackBuffer* GetCpuReadbackBuffer() const { return CpuReadbackBuffer; }

	FVulkanDevice* Device;
	VkImage Image;
	VkFormat StorageFormat;  // Removes SRGB if requested, used to upload data
	VkFormat ViewFormat;  // Format for SRVs, render targets
	VkMemoryPropertyFlags MemProps;
	VkMemoryRequirements MemoryRequirements;

private:
	VulkanRHI::FVulkanViewBase* FirstView = nullptr;

	void InvalidateViews(FVulkanDevice& Device);
	void DestroyViews();
	void SetInitialImageState(FVulkanCommandListContext& Context, VkImageLayout InitialLayout, bool bClear, const FClearValueBinding& ClearValueBinding);
	void InternalMoveSurface(FVulkanDevice& InDevice, FVulkanCommandListContext& Context, VulkanRHI::FVulkanAllocation& DestAllocation);

	VkImageTiling Tiling;
	VulkanRHI::FVulkanAllocation Allocation;
	VkImageAspectFlags FullAspectMask;
	VkImageAspectFlags PartialAspectMask;
	FVulkanCpuReadbackBuffer* CpuReadbackBuffer;

	friend struct FRHICommandSetInitialImageState;

protected:
	EImageOwnerType ImageOwnerType;
};

class FVulkanQueryPool : public VulkanRHI::FDeviceChild
{
public:
	FVulkanQueryPool(FVulkanDevice* InDevice, FVulkanCommandBufferManager* CommandBufferManager, uint32 InMaxQueries, VkQueryType InQueryType, bool bInShouldAddReset = true);
	virtual ~FVulkanQueryPool();

	inline uint32 GetMaxQueries() const
	{
		return MaxQueries;
	}

	inline VkQueryPool GetHandle() const
	{
		return QueryPool;
	}

	inline uint64 GetResultValue(uint32 Index) const
	{
		return QueryOutput[Index];
	}

protected:
	VkQueryPool QueryPool;
	VkEvent ResetEvent;
	const uint32 MaxQueries;
	const VkQueryType QueryType;
	TArray<uint64> QueryOutput;
};

class FVulkanOcclusionQueryPool : public FVulkanQueryPool
{
public:
	FVulkanOcclusionQueryPool(FVulkanDevice* InDevice, FVulkanCommandBufferManager* CommandBufferManager, uint32 InMaxQueries)
		: FVulkanQueryPool(InDevice, CommandBufferManager, InMaxQueries, VK_QUERY_TYPE_OCCLUSION)
	{
		AcquiredIndices.AddZeroed(Align(InMaxQueries, 64) / 64);
		AllocatedQueries.AddZeroed(InMaxQueries);
	}

	inline uint32 AcquireIndex(FVulkanOcclusionQuery* Query)
	{
		check(NumUsedQueries < MaxQueries);
		const uint32 Index = NumUsedQueries;
		const uint32 Word = Index / 64;
		const uint32 Bit = Index % 64;
		const uint64 Mask = (uint64)1 << (uint64)Bit;
		const uint64& WordValue = AcquiredIndices[Word];
		AcquiredIndices[Word] = WordValue | Mask;
		++NumUsedQueries;
		ensure(AllocatedQueries[Index] == nullptr);
		AllocatedQueries[Index] = Query;
		return Index;
	}

	inline void ReleaseIndex(uint32 Index)
	{
		check(Index < NumUsedQueries);
		const uint32 Word = Index / 64;
		const uint32 Bit = Index % 64;
		const uint64 Mask = (uint64)1 << (uint64)Bit;
		const uint64& WordValue = AcquiredIndices[Word];
		ensure((WordValue & Mask) == Mask);
		AcquiredIndices[Word] = WordValue & (~Mask);
		AllocatedQueries[Index] = nullptr;
	}

	inline void EndBatch(FVulkanCmdBuffer* InCmdBuffer)
	{
		ensure(State == EState::RHIT_PostBeginBatch);
		State = EState::RHIT_PostEndBatch;
		SetFence(InCmdBuffer);
	}

	bool CanBeReused();

	inline bool TryGetResults(bool bWait)
	{
		if (State == RT_PostGetResults)
		{
			return true;
		}

		if (State == RHIT_PostEndBatch)
		{
			return InternalTryGetResults(bWait);
		}

		return false;
	}

	void Reset(FVulkanCmdBuffer* InCmdBuffer, uint32 InFrameNumber);

	bool IsStalePool() const;

	void FlushAllocatedQueries();

	enum EState
	{
		Undefined,
		RHIT_PostBeginBatch,
		RHIT_PostEndBatch,
		RT_PostGetResults,
	};
	EState State = Undefined;
	
	// frame number when pool was placed into free list
	uint32 FreedFrameNumber = UINT32_MAX;
protected:
	uint32 NumUsedQueries = 0;
	TArray<FVulkanOcclusionQuery*> AllocatedQueries;
	TArray<uint64> AcquiredIndices;
	bool InternalTryGetResults(bool bWait);
	void SetFence(FVulkanCmdBuffer* InCmdBuffer);

	FVulkanCmdBuffer* CmdBuffer = nullptr;
	uint64 FenceCounter = UINT64_MAX;
	uint32 FrameNumber = UINT32_MAX;
};

class FVulkanTimingQueryPool : public FVulkanQueryPool
{
public:
	FVulkanTimingQueryPool(FVulkanDevice* InDevice, FVulkanCommandBufferManager* CommandBufferManager, uint32 InBufferSize)
		: FVulkanQueryPool(InDevice, CommandBufferManager, InBufferSize * 2, VK_QUERY_TYPE_TIMESTAMP, false)
		, BufferSize(InBufferSize)
	{
		TimestampListHandles.AddZeroed(InBufferSize * 2);
	}

	uint32 CurrentTimestamp = 0;
	uint32 NumIssuedTimestamps = 0;
	const uint32 BufferSize;

	struct FCmdBufferFence
	{
		FVulkanCmdBuffer* CmdBuffer;
		uint64 FenceCounter;
		uint64 FrameCount = UINT64_MAX;
	};
	TArray<FCmdBufferFence> TimestampListHandles;

	VulkanRHI::FStagingBuffer* ResultsBuffer = nullptr;
};

class FVulkanRenderQuery : public FRHIRenderQuery
{
public:
	FVulkanRenderQuery(ERenderQueryType InType)
		: QueryType(InType)
	{
	}

	virtual ~FVulkanRenderQuery() {}

	const ERenderQueryType QueryType;
	uint64 Result = 0;

	uint32 IndexInPool = UINT32_MAX;
};

class FVulkanOcclusionQuery : public FVulkanRenderQuery
{
public:
	FVulkanOcclusionQuery();
	virtual ~FVulkanOcclusionQuery();

	enum class EState
	{
		Undefined,
		RHI_PostBegin,
		RHI_PostEnd,
		RT_GotResults,
		FlushedFromPoolHadResults,
	};

	FVulkanOcclusionQueryPool* Pool = nullptr;

	void ReleaseFromPool();

	EState State = EState::Undefined;
};

class FVulkanTimingQuery : public FVulkanRenderQuery
{
public:
	FVulkanTimingQuery();
	virtual ~FVulkanTimingQuery();

	FVulkanTimingQueryPool* Pool = nullptr;
};

struct FVulkanBufferView : public FRHIResource, public VulkanRHI::FDeviceChild
{
	FVulkanBufferView(FVulkanDevice* InDevice)
		: FRHIResource(RRT_None)
		, VulkanRHI::FDeviceChild(InDevice)
		, View(VK_NULL_HANDLE)
		, ViewId(0)
		, Flags(0)
		, Offset(0)
		, Size(0)
		, bVolatile(false)
	{
	}

	virtual ~FVulkanBufferView()
	{
		Destroy();
	}

	void Create(FVulkanResourceMultiBuffer* Buffer, EPixelFormat Format, uint32 InOffset, uint32 InSize);
	void Create(VkFormat Format, FVulkanResourceMultiBuffer* Buffer, uint32 InOffset, uint32 InSize);
	void Destroy();

	VkBufferView View;
	uint32 ViewId;
	VkFlags Flags;
	uint32 Offset;
	uint32 Size;
	// Whether source buffer is volatile
	bool bVolatile;
};

struct FVulkanRingBuffer : public VulkanRHI::FDeviceChild
{
public:
	FVulkanRingBuffer(FVulkanDevice* InDevice, uint64 TotalSize, VkFlags Usage, VkMemoryPropertyFlags MemPropertyFlags);
	virtual ~FVulkanRingBuffer();

	// Allocate some space in the ring buffer
	inline uint64 AllocateMemory(uint64 Size, uint32 Alignment, FVulkanCmdBuffer* InCmdBuffer)
	{
		Alignment = FMath::Max(Alignment, MinAlignment);
		uint64 AllocationOffset = Align<uint64>(BufferOffset, Alignment);
		if (AllocationOffset + Size <= BufferSize)
		{
			BufferOffset = AllocationOffset + Size;
			return AllocationOffset;
		}

		return WrapAroundAllocateMemory(Size, Alignment, InCmdBuffer);
	}

	inline uint32 GetBufferOffset() const
	{
		return Allocation.Offset;
	}

	inline VkBuffer GetHandle() const
	{
		return Allocation.GetBufferHandle();
	}

	inline void* GetMappedPointer()
	{
		return Allocation.GetMappedPointer(Device);
	}

	VulkanRHI::FVulkanAllocation& GetAllocation()
	{
		return Allocation;
	}

	const VulkanRHI::FVulkanAllocation& GetAllocation() const
	{
		return Allocation;
	}


protected:
	uint64 BufferSize;
	uint64 BufferOffset;
	uint32 MinAlignment;
	VulkanRHI::FVulkanAllocation Allocation;

	// Fence for wrapping around
	FVulkanCmdBuffer* FenceCmdBuffer = nullptr;
	uint64 FenceCounter = 0;

	uint64 WrapAroundAllocateMemory(uint64 Size, uint32 Alignment, FVulkanCmdBuffer* InCmdBuffer);
};

struct FVulkanUniformBufferUploader : public VulkanRHI::FDeviceChild
{
public:
	FVulkanUniformBufferUploader(FVulkanDevice* InDevice);
	~FVulkanUniformBufferUploader();

	uint8* GetCPUMappedPointer()
	{
		return (uint8*)CPUBuffer->GetMappedPointer();
	}

	uint64 AllocateMemory(uint64 Size, uint32 Alignment, FVulkanCmdBuffer* InCmdBuffer)
	{
		return CPUBuffer->AllocateMemory(Size, Alignment, InCmdBuffer);
	}

	const VulkanRHI::FVulkanAllocation& GetCPUBufferAllocation() const
	{
		return CPUBuffer->GetAllocation();
	}

	VkBuffer GetCPUBufferHandle() const
	{
		return CPUBuffer->GetHandle();
	}

	inline uint32 GetCPUBufferOffset() const
	{
		return CPUBuffer->GetBufferOffset();
	}

protected:
	FVulkanRingBuffer* CPUBuffer;
	friend class FVulkanCommandListContext;
};

class FVulkanResourceMultiBuffer : public FRHIBuffer, public VulkanRHI::FDeviceChild
{
public:
	FVulkanResourceMultiBuffer(FVulkanDevice* InDevice, uint32 InSize, EBufferUsageFlags InUEUsage, uint32 InStride, FRHIResourceCreateInfo& CreateInfo, class FRHICommandListBase* InRHICmdList = nullptr, const FRHITransientHeapAllocation* InTransientHeapAllocation = nullptr);
	virtual ~FVulkanResourceMultiBuffer();

	inline const VulkanRHI::FVulkanAllocation& GetCurrentAllocation() const
	{
		return Current.Alloc;
	}

	inline VkBuffer GetHandle() const
	{
		return Current.Handle;
	}

	inline bool IsDynamic() const
	{
		return NumBuffers > 1;
	}

	inline int32 GetDynamicIndex() const
	{
		return DynamicBufferIndex;
	}

	inline bool IsVolatile() const
	{
		return NumBuffers == 0;
	}

	inline uint32 GetVolatileLockCounter() const
	{
		check(IsVolatile());
		return VolatileLockInfo.LockCounter;
	}
	inline uint32 GetVolatileLockSize() const
	{
		check(IsVolatile());
		return VolatileLockInfo.Size;
	}

	inline int32 GetNumBuffers() const
	{
		return NumBuffers;
	}

	// Offset used for Binding a VkBuffer
	inline uint32 GetOffset() const
	{
		return Current.Offset;
	}

	// Remaining size from the current offset
	inline uint64 GetCurrentSize() const
	{
		return Current.Alloc.Size - (Current.Offset - Current.Alloc.Offset);
	}

	inline VkBufferUsageFlags GetBufferUsageFlags() const
	{
		return BufferUsageFlags;
	}

	inline VkIndexType GetIndexType() const
	{
		return (GetStride() == 4)? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
	}

	void* Lock(FRHICommandListBase& RHICmdList, EResourceLockMode LockMode, uint32 Size, uint32 Offset);
	void* Lock(FVulkanCommandListContext& Context, EResourceLockMode LockMode, uint32 Size, uint32 Offset);

	inline void Unlock(FRHICommandListBase& RHICmdList)
	{
		Unlock(&RHICmdList, nullptr);
	}
	inline void Unlock(FVulkanCommandListContext& Context)
	{
		Unlock(nullptr, &Context);
	}

	void Swap(FVulkanResourceMultiBuffer& Other);


	template<typename T>
	void DumpMemory(T Callback)
	{
		Callback(TEXT("FVulkanResourceMultiBuffer"), FName(), this, 0, GetCurrentSize() * GetNumBuffers(), 1, 1, VK_FORMAT_UNDEFINED);
	}

#if VULKAN_RHI_RAYTRACING
	VkDeviceAddress GetDeviceAddress() const;
#endif

	static VkBufferUsageFlags UEToVKBufferUsageFlags(FVulkanDevice* InDevice, EBufferUsageFlags InUEUsage, bool bZeroSize);

	static inline int32 GetNumBuffersFromUsage(EBufferUsageFlags InUEUsage)
	{
		const bool bDynamic = EnumHasAnyFlags(InUEUsage, BUF_Dynamic);
		return bDynamic ? NUM_BUFFERS : 1;
	}

protected:
	void Unlock(FRHICommandListBase* RHICmdList, FVulkanCommandListContext* Context);


	VkBufferUsageFlags BufferUsageFlags;
	uint8 NumBuffers;
	uint8 DynamicBufferIndex;
	enum class ELockStatus : uint8
	{
		Unlocked,
		Locked,
		PersistentMapping,
	} LockStatus;

	enum
	{
		NUM_BUFFERS = 3,
	};

	VulkanRHI::FVulkanAllocation Buffers[NUM_BUFFERS];
	struct
	{
		VulkanRHI::FVulkanAllocation Alloc;
		VkBuffer Handle = VK_NULL_HANDLE;
		uint64 Offset = 0;
		uint64 Size = 0;
	} Current;
	VulkanRHI::FTempFrameAllocationBuffer::FTempAllocInfo VolatileLockInfo;

	static void InternalUnlock(FVulkanCommandListContext& Context, VulkanRHI::FPendingBufferLock& PendingLock, FVulkanResourceMultiBuffer* MultiBuffer, int32 InDynamicBufferIndex);

	friend class FVulkanCommandListContext;
	friend struct FRHICommandMultiBufferUnlock;
};

class FVulkanUniformBuffer : public FRHIUniformBuffer
{
public:
	FVulkanUniformBuffer(FVulkanDevice& Device, const FRHIUniformBufferLayout* InLayout, const void* Contents, EUniformBufferUsage InUsage, EUniformBufferValidation Validation);
	virtual ~FVulkanUniformBuffer();

	const TArray<TRefCountPtr<FRHIResource>>& GetResourceTable() const { return ResourceTable; }

	void UpdateResourceTable(const FRHIUniformBufferLayout& InLayout, const void* Contents, int32 ResourceNum);
	void UpdateResourceTable(FRHIResource** Resources, int32 ResourceNum);

	inline uint32 GetOffset() const
	{
		return Allocation.Offset;
	}

	inline void UpdateAllocation(VulkanRHI::FVulkanAllocation& NewAlloc)
	{
		NewAlloc.Swap(Allocation);
	}
	
public:
	FVulkanDevice* Device;
	VulkanRHI::FVulkanAllocation Allocation;
	EUniformBufferUsage Usage;
protected:
	TArray<TRefCountPtr<FRHIResource>> ResourceTable;
	
};

class FVulkanUnorderedAccessView : public FRHIUnorderedAccessView, public VulkanRHI::FVulkanViewBase
{
public:

	FVulkanUnorderedAccessView(FVulkanDevice* Device, FVulkanResourceMultiBuffer* Buffer, bool bUseUAVCounter, bool bAppendBuffer);
	FVulkanUnorderedAccessView(FVulkanDevice* Device, FRHITexture* TextureRHI, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices);
	FVulkanUnorderedAccessView(FVulkanDevice* Device, FVulkanResourceMultiBuffer* Buffer, EPixelFormat Format);

	~FVulkanUnorderedAccessView();

	void Invalidate();

	void UpdateView();

protected:
	// The texture that this UAV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	FVulkanTextureView TextureView;
	uint32 MipLevel;
	uint16 FirstArraySlice;
	uint16 NumArraySlices;

	// The buffer this UAV comes from (can be null)
	TRefCountPtr<FVulkanResourceMultiBuffer> SourceBuffer;
	TRefCountPtr<FVulkanBufferView> BufferView;
	EPixelFormat BufferViewFormat;

	// Used to check on volatile buffers if a new BufferView is required
	uint32 VolatileLockCounter;
	friend class FVulkanPendingGfxState;
	friend class FVulkanPendingComputeState;
	friend class FVulkanDynamicRHI;
	friend class FVulkanCommandListContext;
};


class FVulkanShaderResourceView : public FRHIShaderResourceView, public VulkanRHI::FVulkanViewBase
{
public:
	FVulkanShaderResourceView(FVulkanDevice* Device, FRHIViewableResource* InRHIBuffer, FVulkanResourceMultiBuffer* InSourceBuffer, uint32 InSize, EPixelFormat InFormat, uint32 InOffset = 0);
	FVulkanShaderResourceView(FVulkanDevice* Device, FRHITexture* InSourceTexture, const FRHITextureSRVCreateInfo& InCreateInfo);
	FVulkanShaderResourceView(FVulkanDevice* Device, FVulkanResourceMultiBuffer* InSourceBuffer, uint32 InOffset = 0);

	void Clear();

	void Rename(FRHIResource* InRHIBuffer, FVulkanResourceMultiBuffer* InSourceBuffer, uint32 InSize, EPixelFormat InFormat);

	void Invalidate();
	void UpdateView();

	inline FVulkanBufferView* GetBufferView()
	{
		return BufferViews[BufferIndex];
	}

	EPixelFormat BufferViewFormat = PF_Unknown;
	ERHITextureSRVOverrideSRGBType SRGBOverride = SRGBO_Default;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	FVulkanTextureView TextureView;
	FVulkanResourceMultiBuffer* SourceStructuredBuffer = nullptr;
	uint32 MipLevel = 0;
	uint32 NumMips = MAX_uint32;
	uint32 FirstArraySlice = 0;
	uint32 NumArraySlices = 0;

	~FVulkanShaderResourceView();

	TArray<TRefCountPtr<FVulkanBufferView>> BufferViews;
	uint32 BufferIndex = 0;
	uint32 Size = 0;
	uint32 Offset = 0;
	// The buffer this SRV comes from (can be null)
	FVulkanResourceMultiBuffer* SourceBuffer = nullptr;
	// To keep a reference
	TRefCountPtr<FRHIResource> SourceRHIBuffer;

#if VULKAN_RHI_RAYTRACING
	VkAccelerationStructureKHR AccelerationStructureHandle = VK_NULL_HANDLE;
#endif // VULKAN_RHI_RAYTRACING

protected:
	// Used to check on volatile buffers if a new BufferView is required
	VkBuffer VolatileBufferHandle = VK_NULL_HANDLE;
	uint32 VolatileLockCounter = MAX_uint32;

	FVulkanShaderResourceView* NextView = 0;
	friend class FVulkanTexture;
};

class FVulkanVertexInputStateInfo
{
public:
	FVulkanVertexInputStateInfo();
	~FVulkanVertexInputStateInfo();

	void Generate(FVulkanVertexDeclaration* VertexDeclaration, uint32 VertexHeaderInOutAttributeMask);

	inline uint32 GetHash() const
	{
		check(Info.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
		return Hash;
	}

	inline const VkPipelineVertexInputStateCreateInfo& GetInfo() const
	{
		return Info;
	}

	bool operator ==(const FVulkanVertexInputStateInfo& Other);

protected:
	VkPipelineVertexInputStateCreateInfo Info;
	uint32 Hash;

	uint32 BindingsNum;
	uint32 BindingsMask;

	//#todo-rco: Remove these TMaps
	TMap<uint32, uint32> BindingToStream;
	TMap<uint32, uint32> StreamToBinding;
	VkVertexInputBindingDescription Bindings[MaxVertexElementCount];

	uint32 AttributesNum;
	VkVertexInputAttributeDescription Attributes[MaxVertexElementCount];

	friend class FVulkanPendingGfxState;
	friend class FVulkanPipelineStateCacheManager;
};

// This class holds the staging area for packed global uniform buffers for a given shader
class FPackedUniformBuffers
{
public:
	// One buffer is a chunk of bytes
	typedef TArray<uint8> FPackedBuffer;

	void Init(const FVulkanShaderHeader& InCodeHeader, uint64& OutPackedUniformBufferStagingMask)
	{
		PackedUniformBuffers.AddDefaulted(InCodeHeader.PackedUBs.Num());
		for (int32 Index = 0; Index < InCodeHeader.PackedUBs.Num(); ++Index)
		{
			PackedUniformBuffers[Index].AddUninitialized(InCodeHeader.PackedUBs[Index].SizeInBytes);
		}

		OutPackedUniformBufferStagingMask = ((uint64)1 << (uint64)InCodeHeader.PackedUBs.Num()) - 1;
		EmulatedUBsCopyInfo = InCodeHeader.EmulatedUBsCopyInfo;
		EmulatedUBsCopyRanges = InCodeHeader.EmulatedUBCopyRanges;
	}

	inline void SetPackedGlobalParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* RESTRICT NewValue, uint64& InOutPackedUniformBufferStagingDirty)
	{
		FPackedBuffer& StagingBuffer = PackedUniformBuffers[BufferIndex];
		check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
		check((NumBytes & 3) == 0 && (ByteOffset & 3) == 0);
		uint32* RESTRICT RawDst = (uint32*)(StagingBuffer.GetData() + ByteOffset);
		uint32* RESTRICT RawSrc = (uint32*)NewValue;
		uint32* RESTRICT RawSrcEnd = RawSrc + (NumBytes >> 2);

		bool bChanged = false;
		while (RawSrc != RawSrcEnd)
		{
			bChanged |= CopyAndReturnNotEqual(*RawDst++, *RawSrc++);
		}

		InOutPackedUniformBufferStagingDirty = InOutPackedUniformBufferStagingDirty | ((uint64)(bChanged ? 1 : 0) << (uint64)BufferIndex);
	}

	// Copies a 'real' constant buffer into the packed globals uniform buffer (only the used ranges)
	inline void SetEmulatedUniformBufferIntoPacked(uint32 BindPoint, const TArray<uint8>& ConstantData, uint64& NEWPackedUniformBufferStagingDirty)
	{
		// Emulated UBs. Assumes UniformBuffersCopyInfo table is sorted by CopyInfo.SourceUBIndex
		if (BindPoint < (uint32)EmulatedUBsCopyRanges.Num())
		{
			uint32 Range = EmulatedUBsCopyRanges[BindPoint];
			uint16 Start = (Range >> 16) & 0xffff;
			uint16 Count = Range & 0xffff;
			const uint8* RESTRICT SourceData = ConstantData.GetData();
			for (int32 Index = Start; Index < Start + Count; ++Index)
			{
				const CrossCompiler::FUniformBufferCopyInfo& CopyInfo = EmulatedUBsCopyInfo[Index];
				check(CopyInfo.SourceUBIndex == BindPoint);
				FPackedBuffer& StagingBuffer = PackedUniformBuffers[(int32)CopyInfo.DestUBIndex];
				//check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
				bool bChanged = false;
				uint32* RESTRICT RawDst = (uint32*)(StagingBuffer.GetData() + CopyInfo.DestOffsetInFloats * 4);
				uint32* RESTRICT RawSrc = (uint32*)(SourceData + CopyInfo.SourceOffsetInFloats * 4);
				uint32* RESTRICT RawSrcEnd = RawSrc + CopyInfo.SizeInFloats;
				do
				{
					bChanged |= CopyAndReturnNotEqual(*RawDst++, *RawSrc++);
				}
				while (RawSrc != RawSrcEnd);
				NEWPackedUniformBufferStagingDirty = NEWPackedUniformBufferStagingDirty | ((uint64)(bChanged ? 1 : 0) << (uint64)CopyInfo.DestUBIndex);
			}
		}
	}

	inline const FPackedBuffer& GetBuffer(int32 Index) const
	{
		return PackedUniformBuffers[Index];
	}

protected:
	TArray<FPackedBuffer>									PackedUniformBuffers;

	// Copies to Shader Code Header (shaders may be deleted when we use this object again)
	TArray<CrossCompiler::FUniformBufferCopyInfo>			EmulatedUBsCopyInfo;
	TArray<uint32>											EmulatedUBsCopyRanges;
};

class FVulkanStagingBuffer : public FRHIStagingBuffer
{
	friend class FVulkanCommandListContext;
public:
	FVulkanStagingBuffer()
		: FRHIStagingBuffer()
	{
		check(!bIsLocked);
	}

	virtual ~FVulkanStagingBuffer();

	virtual void* Lock(uint32 Offset, uint32 NumBytes) final override;
	virtual void Unlock() final override;

private:
	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	uint32 QueuedOffset = 0;
	uint32 QueuedNumBytes = 0;
	// The staging buffer was allocated from this device.
	FVulkanDevice* Device;
};

class FVulkanGPUFence : public FRHIGPUFence
{
public:
	FVulkanGPUFence(FName InName)
		: FRHIGPUFence(InName)
	{
	}

	virtual void Clear() final override;
	virtual bool Poll() const final override;

	FVulkanCmdBuffer* GetCmdBuffer() const { return CmdBuffer; }

protected:
	FVulkanCmdBuffer*	CmdBuffer = nullptr;
	uint64				FenceSignaledCounter = MAX_uint64;

	friend class FVulkanCommandListContext;
};

template<class T>
struct TVulkanResourceTraits
{
};
template<>
struct TVulkanResourceTraits<FRHIVertexDeclaration>
{
	typedef FVulkanVertexDeclaration TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIVertexShader>
{
	typedef FVulkanVertexShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIGeometryShader>
{
	typedef FVulkanGeometryShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIPixelShader>
{
	typedef FVulkanPixelShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIComputeShader>
{
	typedef FVulkanComputeShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITexture>
{
	typedef FVulkanTexture TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIRenderQuery>
{
	typedef FVulkanRenderQuery TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIUniformBuffer>
{
	typedef FVulkanUniformBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIBuffer>
{
	typedef FVulkanResourceMultiBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIShaderResourceView>
{
	typedef FVulkanShaderResourceView TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIUnorderedAccessView>
{
	typedef FVulkanUnorderedAccessView TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHISamplerState>
{
	typedef FVulkanSamplerState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIRasterizerState>
{
	typedef FVulkanRasterizerState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIDepthStencilState>
{
	typedef FVulkanDepthStencilState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIBlendState>
{
	typedef FVulkanBlendState TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIBoundShaderState>
{
	typedef FVulkanBoundShaderState TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIStagingBuffer>
{
	typedef FVulkanStagingBuffer TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIGPUFence>
{
	typedef FVulkanGPUFence TConcreteType;
};

template<typename TRHIType>
static FORCEINLINE typename TVulkanResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
{
	return static_cast<typename TVulkanResourceTraits<TRHIType>::TConcreteType*>(Resource);
}

template<typename TRHIType>
static FORCEINLINE typename TVulkanResourceTraits<TRHIType>::TConcreteType* ResourceCast(const TRHIType* Resource)
{
	return static_cast<const typename TVulkanResourceTraits<TRHIType>::TConcreteType*>(Resource);
}

#if VULKAN_RHI_RAYTRACING
class FVulkanRayTracingScene;
template<>
struct TVulkanResourceTraits<FRHIRayTracingScene>
{
	typedef FVulkanRayTracingScene TConcreteType;
};
class FVulkanRayTracingGeometry;
template<>
struct TVulkanResourceTraits<FRHIRayTracingGeometry>
{
	typedef FVulkanRayTracingGeometry TConcreteType;
};
#endif // VULKAN_RHI_RAYTRACING
