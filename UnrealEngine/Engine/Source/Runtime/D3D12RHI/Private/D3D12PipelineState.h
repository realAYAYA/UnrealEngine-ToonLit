// Copyright Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

#pragma once

#include "Async/AsyncWork.h"
#include "D3D12DiskCache.h"
#include "D3D12Shader.h"

class FD3D12VertexShader;
class FD3D12MeshShader;
class FD3D12AmplificationShader;
class FD3D12PixelShader;
class FD3D12GeometryShader;
class FD3D12ComputeShader;

// FORT-101886
// UE implemented high level PSO caches on the general RHI level already
// D3D12RHI high level PSO caches never cleanup (until shutdown) currently
// and stale PSOs remain in the cache, which is being suspected as the cause
// of some crashes
// TODO: Remove or rewrite D3D12RHI high level PSO cache
#ifndef D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
#define D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE 0
#endif
#ifndef D3D12_USE_DERIVED_PSO
	#define D3D12_USE_DERIVED_PSO 0
#endif
#if D3D12_USE_DERIVED_PSO
	#ifndef D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
		#define D3D12_USE_DERIVED_PSO_SHADER_EXPORTS 0
	#endif // D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
#endif // D3D12_USE_DERIVED_PSO

#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Graphics: Num high-level cache entries"), STAT_PSOGraphicsNumHighlevelCacheEntries, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Graphics: High-level cache hit"), STAT_PSOGraphicsHighlevelCacheHit, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Graphics: High-level cache miss"), STAT_PSOGraphicsHighlevelCacheMiss, STATGROUP_D3D12PipelineState);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Compute: Num high-level cache entries"), STAT_PSOComputeNumHighlevelCacheEntries, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Compute: High-level cache hit"), STAT_PSOComputeHighlevelCacheHit, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Compute: High-level cache miss"), STAT_PSOComputeHighlevelCacheMiss, STATGROUP_D3D12PipelineState);
#endif

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Graphics: Num low-level cache entries"), STAT_PSOGraphicsNumLowlevelCacheEntries, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Graphics: Low-level cache hit"), STAT_PSOGraphicsLowlevelCacheHit, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Graphics: Low-level cache miss"), STAT_PSOGraphicsLowlevelCacheMiss, STATGROUP_D3D12PipelineState);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Compute: Num low-level cache entries"), STAT_PSOComputeNumLowlevelCacheEntries, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Compute: Low-level cache hit"), STAT_PSOComputeLowlevelCacheHit, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Compute: Low-level cache miss"), STAT_PSOComputeLowlevelCacheMiss, STATGROUP_D3D12PipelineState);


// Graphics pipeline struct that represents the latest versions of PSO subobjects currently supported by the RHI.
struct FD3D12_GRAPHICS_PIPELINE_STATE_DESC
{
	ID3D12RootSignature *pRootSignature;
	D3D12_SHADER_BYTECODE VS;
	D3D12_SHADER_BYTECODE MS;
	D3D12_SHADER_BYTECODE AS;
	D3D12_SHADER_BYTECODE PS;
	D3D12_SHADER_BYTECODE GS;
#if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
	D3D12_BLEND_DESC BlendState;
#endif // #if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
#if !D3D12_USE_DERIVED_PSO
	uint32 SampleMask;
	D3D12_RASTERIZER_DESC RasterizerState;
	D3D12_DEPTH_STENCIL_DESC1 DepthStencilState;
#endif // #if !D3D12_USE_DERIVED_PSO
	D3D12_INPUT_LAYOUT_DESC InputLayout;
	D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
	D3D12_RT_FORMAT_ARRAY RTFormatArray;
	DXGI_FORMAT DSVFormat;
	DXGI_SAMPLE_DESC SampleDesc;
	uint32 NodeMask;
	D3D12_CACHED_PIPELINE_STATE CachedPSO;
	D3D12_PIPELINE_STATE_FLAGS Flags;

	FD3D12_GRAPHICS_PIPELINE_STATE_STREAM PipelineStateStream() const;
#if PLATFORM_SUPPORTS_MESH_SHADERS
	FD3D12_MESH_PIPELINE_STATE_STREAM MeshPipelineStateStream() const;
#endif
};

struct FD3D12LowLevelGraphicsPipelineStateDesc
{
	const FD3D12RootSignature *pRootSignature;
	FD3D12_GRAPHICS_PIPELINE_STATE_DESC Desc;
	ShaderBytecodeHash VSHash;
	ShaderBytecodeHash MSHash;
	ShaderBytecodeHash ASHash;
	ShaderBytecodeHash GSHash;
	ShaderBytecodeHash PSHash;
	uint32 InputLayoutHash;
	bool bFromPSOFileCache;

	SIZE_T CombinedHash;

	FORCEINLINE bool UsesMeshShaders() const
	{
		return Desc.MS.BytecodeLength > 0;
	}

#if D3D12RHI_NEEDS_VENDOR_EXTENSIONS
	// TODO: Replace with a global hash lookup to reduce overall footprint?
	// Very few permutations, so a single > 0 u32 hash code would be lower
	// memory usage, and very rarely cause a look up.
	const TArray<FShaderCodeVendorExtension>* VSExtensions;
	const TArray<FShaderCodeVendorExtension>* MSExtensions;
	const TArray<FShaderCodeVendorExtension>* ASExtensions;
	const TArray<FShaderCodeVendorExtension>* GSExtensions;
	const TArray<FShaderCodeVendorExtension>* PSExtensions;

	FORCEINLINE bool HasVendorExtensions() const
	{
		return (
			VSExtensions != nullptr ||
			MSExtensions != nullptr ||
			ASExtensions != nullptr ||
			PSExtensions != nullptr ||
			GSExtensions != nullptr);
	}
#endif

	FORCEINLINE FString GetName() const { return FString::Printf(TEXT("%llu"), CombinedHash); }

#if D3D12_USE_DERIVED_PSO
	void Destroy();
#endif
};

// Compute pipeline struct that represents the latest versions of PSO subobjects currently supported by the RHI.
struct FD3D12_COMPUTE_PIPELINE_STATE_DESC : public D3D12_COMPUTE_PIPELINE_STATE_DESC
{
	FD3D12_COMPUTE_PIPELINE_STATE_STREAM PipelineStateStream() const;
};

struct FD3D12ComputePipelineStateDesc
{
	const FD3D12RootSignature* pRootSignature;
	FD3D12_COMPUTE_PIPELINE_STATE_DESC Desc;
	ShaderBytecodeHash CSHash;

	SIZE_T CombinedHash;

#if D3D12RHI_NEEDS_VENDOR_EXTENSIONS
	const TArray<FShaderCodeVendorExtension>* Extensions;
	FORCEINLINE bool HasVendorExtensions() const { return (Extensions != nullptr); }
#endif

	FORCEINLINE FString GetName() const { return FString::Printf(TEXT("%llu"), CombinedHash); }

#if D3D12_USE_DERIVED_PSO
	void Destroy();
#endif
};


#define PSO_IF_NOT_EQUAL_RETURN_FALSE( value ) if(lhs.value != rhs.value){ return false; }
#define PSO_IF_MEMCMP_FAILS_RETURN_FALSE( value ) if(FMemory::Memcmp(&lhs.value, &rhs.value, sizeof(rhs.value)) != 0){ return false; }
#define PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE( value ) \
	const char* const lhString = lhs.value; \
	const char* const rhString = rhs.value; \
	if (lhString != rhString) \
	{ \
		if (strcmp(lhString, rhString) != 0) \
		{ \
			return false; \
		} \
	}

template <typename TDesc> struct equality_pipeline_state_desc;

template <> struct equality_pipeline_state_desc<FD3D12LowLevelGraphicsPipelineStateDesc>
{
	bool operator()(const FD3D12LowLevelGraphicsPipelineStateDesc& lhs, const FD3D12LowLevelGraphicsPipelineStateDesc& rhs)
	{
		// Order from most likely to change to least
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.VS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.MS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.AS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.GS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.NumElements)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.RTFormatArray.NumRenderTargets)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DSVFormat)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PrimitiveTopologyType)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
#if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.BlendState)
#endif // #if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
#if !D3D12_USE_DERIVED_PSO
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleMask)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.RasterizerState)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.DepthStencilState)
#endif // #if !D3D12_USE_DERIVED_PSO
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.IBStripCutValue)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Count)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Quality)

		for (uint32 i = 0; i < lhs.Desc.RTFormatArray.NumRenderTargets; i++)
		{
			PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.RTFormatArray.RTFormats[i]);
		}

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(VSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(MSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(ASHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(GSHash)

		if (lhs.Desc.InputLayout.pInputElementDescs != rhs.Desc.InputLayout.pInputElementDescs &&
			lhs.Desc.InputLayout.NumElements)
		{
			for (uint32 i = 0; i < lhs.Desc.InputLayout.NumElements; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticIndex)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].Format)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlot)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].AlignedByteOffset)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlotClass)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InstanceDataStepRate)
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticName)
			}
		}

	#if PLATFORM_WINDOWS
		PSO_IF_NOT_EQUAL_RETURN_FALSE(VSExtensions);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(MSExtensions);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(ASExtensions);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PSExtensions);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(GSExtensions);
	#endif

		return true;
	}
};

template <> struct equality_pipeline_state_desc<FD3D12ComputePipelineStateDesc>
{
	bool operator()(const FD3D12ComputePipelineStateDesc& lhs, const FD3D12ComputePipelineStateDesc& rhs)
	{
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.CS.BytecodeLength)
#if PLATFORM_WINDOWS
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
#endif
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(CSHash)

#if PLATFORM_WINDOWS
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Extensions)
#endif

		return true;
	}
};

struct ComputePipelineCreationArgs;
struct GraphicsPipelineCreationArgs;

struct FD3D12PipelineStateWorker : public FD3D12AdapterChild, public FNonAbandonableTask
{
	FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const ComputePipelineCreationArgs& InArgs);
	FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const GraphicsPipelineCreationArgs& InArgs);

	void DoWork();

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FD3D12PipelineStateWorker, STATGROUP_ThreadPoolAsyncTasks); }

	union PipelineCreationArgs
	{
		ComputePipelineCreationArgs_POD* ComputeArgs;
		GraphicsPipelineCreationArgs_POD* GraphicsArgs;
	} CreationArgs;

	const bool bIsGraphics;
	TRefCountPtr<ID3D12PipelineState> PSO;
};

struct FD3D12PipelineState : public FD3D12AdapterChild, public FD3D12MultiNodeGPUObject, public FNoncopyable, public FRefCountBase
{
public:
	explicit FD3D12PipelineState(FD3D12Adapter* Parent);
	~FD3D12PipelineState();

	void Create(const ComputePipelineCreationArgs& InCreationArgs);
	void CreateAsync(const ComputePipelineCreationArgs& InCreationArgs);

#if D3D12_USE_DERIVED_PSO
	void Create(const FGraphicsPipelineStateInitializer& Initializer, FD3D12PipelineState* BasePSO);
#endif

	void Create(const GraphicsPipelineCreationArgs& InCreationArgs);
	void CreateAsync(const GraphicsPipelineCreationArgs& InCreationArgs);

	FORCEINLINE bool IsValid()
	{
		return (GetPipelineState() != nullptr);
	}

	FORCEINLINE ID3D12PipelineState* GetPipelineState()
	{
		if (InitState == PSOInitState::Initialized)
		{
			return PipelineState.GetReference();
		}
		else
		{
			return InternalGetPipelineState();
		}
	}

	FORCEINLINE uint64 GetContextSortKey() const
	{
		return ContextSortKey;
	}

	FORCEINLINE void SetContextSortKey(uint64 InContextSortKey)
	{
		ContextSortKey = InContextSortKey;
	}

	FD3D12PipelineState& operator=(const FD3D12PipelineState& other) = delete;

    static bool UsePSORefCounting();

private:
	ID3D12PipelineState* InternalGetPipelineState();

protected:
	TRefCountPtr<ID3D12PipelineState> PipelineState;
	FAsyncTask<FD3D12PipelineStateWorker>* Worker;
	FRWLock GetPipelineStateMutex;

	enum class PSOInitState
	{
		Initialized,
		Uninitialized,
		CreationFailed,
	};
	volatile PSOInitState InitState;

	// GRHISupportsPipelineStateSortKey
	uint64 ContextSortKey = 0;
};

struct FD3D12PipelineStateCommonData
{
	FD3D12PipelineStateCommonData(const FD3D12RootSignature* InRootSignature, FD3D12PipelineState* InPipelineState);

	const FD3D12RootSignature* const RootSignature;

	FD3D12PipelineState* PipelineState;
};

struct FD3D12GraphicsPipelineState : public FRHIGraphicsPipelineState, FD3D12PipelineStateCommonData
{
	FD3D12GraphicsPipelineState() = delete;
	FD3D12GraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer, const FD3D12RootSignature* InRootSignature, FD3D12PipelineState* InPipelineState);
	~FD3D12GraphicsPipelineState();

	FORCEINLINE FD3D12VertexShader*        GetVertexShader() const        { return (FD3D12VertexShader*)PipelineStateInitializer.BoundShaderState.GetVertexShader(); }
	FORCEINLINE FD3D12PixelShader*         GetPixelShader() const         { return (FD3D12PixelShader*)PipelineStateInitializer.BoundShaderState.GetPixelShader(); }
	FORCEINLINE FD3D12MeshShader*          GetMeshShader() const          { return (FD3D12MeshShader*)PipelineStateInitializer.BoundShaderState.GetMeshShader(); }
	FORCEINLINE FD3D12AmplificationShader* GetAmplificationShader() const { return (FD3D12AmplificationShader*)PipelineStateInitializer.BoundShaderState.GetAmplificationShader(); }
	FORCEINLINE FD3D12GeometryShader*      GetGeometryShader() const      { return (FD3D12GeometryShader*)PipelineStateInitializer.BoundShaderState.GetGeometryShader(); }

	FGraphicsPipelineStateInitializer PipelineStateInitializer;
	TStaticArray<uint16, MaxVertexElementCount> StreamStrides;
	bool bShaderNeedsGlobalConstantBuffer[SF_NumStandardFrequencies];
};

struct FD3D12ComputePipelineState : public FRHIComputePipelineState, FD3D12PipelineStateCommonData
{
	FD3D12ComputePipelineState() = delete;
	FD3D12ComputePipelineState(FD3D12ComputeShader* InComputeShader, const FD3D12RootSignature* InRootSignature, FD3D12PipelineState* InPipelineState);
	~FD3D12ComputePipelineState();

	FORCEINLINE FD3D12ComputeShader* GetComputeShader() const { return ComputeShader; }

	TRefCountPtr<FD3D12ComputeShader> ComputeShader;
	bool bShaderNeedsGlobalConstantBuffer;
};

#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
struct FInitializerToGPSOMapKey
{
	const FGraphicsPipelineStateInitializer* Initializer;
	uint32 Hash;

	FInitializerToGPSOMapKey() = default;

	FInitializerToGPSOMapKey(const FGraphicsPipelineStateInitializer* InInitializer, uint32 InHash) :
		Initializer(InInitializer),
		Hash(InHash)
	{}

	inline bool operator==(const FInitializerToGPSOMapKey& Other) const
	{
		return *Initializer == *Other.Initializer;
	}
};

inline uint32 GetTypeHash(const FInitializerToGPSOMapKey& Key)
{
	return Key.Hash;
}
#endif

class FD3D12PipelineStateCacheBase : public FD3D12AdapterChild
{
protected:
	enum PSO_CACHE_TYPE
	{
		PSO_CACHE_GRAPHICS,
		PSO_CACHE_COMPUTE,
		NUM_PSO_CACHE_TYPES
	};

	template <typename TDesc, typename TValue>
	struct TStateCacheKeyFuncs : BaseKeyFuncs<TPair<TDesc, TValue>, TDesc, false>
	{
		typedef typename TTypeTraits<TDesc>::ConstPointerType KeyInitType;
		typedef const TPairInitializer<typename TTypeTraits<TDesc>::ConstInitType, typename TTypeTraits<TValue>::ConstInitType>& ElementInitType;

		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			equality_pipeline_state_desc<TDesc> equal;
			return equal(A, B);
		}
		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
		{
			return Key.CombinedHash;
		}
	};

	template <typename TDesc, typename TValue = FD3D12PipelineState*>
	using TPipelineCache = TMap<TDesc, TValue, FDefaultSetAllocator, TStateCacheKeyFuncs<TDesc, TValue>>;
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	TMap<FInitializerToGPSOMapKey, FD3D12GraphicsPipelineState*> InitializerToGraphicsPipelineMap;
	TMap<FD3D12ComputeShader*, FD3D12ComputePipelineState*> ComputeShaderToComputePipelineMap;
#endif
	TPipelineCache<FD3D12LowLevelGraphicsPipelineStateDesc> LowLevelGraphicsPipelineStateCache;
	TPipelineCache<FD3D12ComputePipelineStateDesc> ComputePipelineStateCache;

	// Thread access mutual exclusion
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	mutable FRWLock InitializerToGraphicsPipelineMapMutex;
	mutable FRWLock ComputeShaderToComputePipelineMapMutex;
#endif
	mutable FRWLock LowLevelGraphicsPipelineStateCacheMutex;
	mutable FRWLock ComputePipelineStateCacheMutex;

	FRWLock DiskCachesCS;
	FDiskCacheInterface DiskCaches[NUM_PSO_CACHE_TYPES];

	void CleanupPipelineStateCaches();

	typedef TFunction<void(FD3D12PipelineState**, const FD3D12LowLevelGraphicsPipelineStateDesc&)> FPostCreateGraphicCallback;
	typedef TFunction<void(FD3D12PipelineState*, const FD3D12ComputePipelineStateDesc&)> FPostCreateComputeCallback;

#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	virtual FD3D12GraphicsPipelineState* AddToRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, const FD3D12RootSignature* RootSignature, FD3D12PipelineState* PipelineState);
	FD3D12ComputePipelineState* AddToRuntimeCache(FD3D12ComputeShader* ComputeShader, FD3D12PipelineState* PipelineState);
#endif

	FD3D12PipelineState* FindInLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc);
	FD3D12PipelineState* CreateAndAddToLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc);
	void AddToLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc, FD3D12PipelineState** OutPipelineState, const FPostCreateGraphicCallback& PostCreateCallback);
	virtual void OnPSOCreated(FD3D12PipelineState* PipelineState, const FD3D12LowLevelGraphicsPipelineStateDesc& Desc) = 0;
	
	FD3D12PipelineState* FindInLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc);
	FD3D12PipelineState* CreateAndAddToLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc);
	void AddToLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc, FD3D12PipelineState** OutPipelineState, const FPostCreateComputeCallback& PostCreateCallback);
	virtual void OnPSOCreated(FD3D12PipelineState* PipelineState, const FD3D12ComputePipelineStateDesc& Desc) = 0;

#if !D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	FD3D12GraphicsPipelineState* FindInLoadedCache(const FGraphicsPipelineStateInitializer& Initializer, const FD3D12RootSignature* RootSignature, FD3D12LowLevelGraphicsPipelineStateDesc& OutLowLevelDesc);
	FD3D12GraphicsPipelineState* CreateAndAdd(const FGraphicsPipelineStateInitializer& Initializer, const FD3D12RootSignature* RootSignature, const FD3D12LowLevelGraphicsPipelineStateDesc& LowLevelDesc);
#endif
public:
	void RemoveFromLowLevelCache(FD3D12PipelineState* PipelineState, const FGraphicsPipelineStateInitializer& PipelineStateInitializer, const FD3D12RootSignature* RootSignature);

#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	FD3D12GraphicsPipelineState* FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32& OutHash);
	FD3D12GraphicsPipelineState* FindInLoadedCache(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, const FD3D12RootSignature* RootSignature, FD3D12LowLevelGraphicsPipelineStateDesc& OutLowLevelDesc);
	FD3D12GraphicsPipelineState* CreateAndAdd(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, const FD3D12RootSignature* RootSignature, const FD3D12LowLevelGraphicsPipelineStateDesc& LowLevelDesc);

	FD3D12ComputePipelineState* FindInRuntimeCache(const FD3D12ComputeShader* ComputeShader);
#endif
	FD3D12ComputePipelineState* FindInLoadedCache(FD3D12ComputeShader* ComputeShader, const FD3D12RootSignature* RootSignature, FD3D12ComputePipelineStateDesc& OutLowLevelDesc);
	FD3D12ComputePipelineState* CreateAndAdd(FD3D12ComputeShader* ComputeShader, const FD3D12RootSignature* RootSignature, const FD3D12ComputePipelineStateDesc& LowLevelDesc);

	static uint64 HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc);
	static uint64 HashPSODesc(const FD3D12ComputePipelineStateDesc& Desc);

	static uint64 HashData(const void* Data, int32 NumBytes);

	FD3D12PipelineStateCacheBase(FD3D12Adapter* InParent);
	virtual ~FD3D12PipelineStateCacheBase();
};
