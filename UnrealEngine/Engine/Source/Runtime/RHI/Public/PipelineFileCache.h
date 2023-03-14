// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Containers/List.h"
#include "Containers/StringView.h"
#include "RHI.h"

DECLARE_STATS_GROUP(TEXT("ShaderPipelineCache"),STATGROUP_PipelineStateCache, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Total Graphics Pipeline State Count"), STAT_TotalGraphicsPipelineStateCount, STATGROUP_PipelineStateCache, RHI_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Total Compute Pipeline State Count"), STAT_TotalComputePipelineStateCount, STATGROUP_PipelineStateCache, RHI_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Total RayTracing Pipeline State Count"), STAT_TotalRayTracingPipelineStateCount, STATGROUP_PipelineStateCache, RHI_API);

#define PIPELINE_CACHE_DEFAULT_ENABLED (!WITH_EDITOR)

/**
 * PSO_COOKONLY_DATA
 * - Is a transitory data area that should only be used during the cook and stable pipeline cache file generation processes.
 * - If def'ing it out in GAME builds helps to reduce confusion as to where the actual data resides
 * - Should not be serialized or used in comparsion operations (e.g. UsageMask: PSO need to be able to compare equal with different Masks during cook).
 */
#define PSO_COOKONLY_DATA (WITH_EDITOR || IS_PROGRAM)

struct RHI_API FPipelineFileCacheRasterizerState
{
	FPipelineFileCacheRasterizerState() { FMemory::Memzero(*this); }
	FPipelineFileCacheRasterizerState(FRasterizerStateInitializerRHI const& Other) { operator=(Other); }
	
	float DepthBias;
	float SlopeScaleDepthBias;
	TEnumAsByte<ERasterizerFillMode> FillMode;
	TEnumAsByte<ERasterizerCullMode> CullMode;
	ERasterizerDepthClipMode DepthClipMode;
	bool bAllowMSAA;
	bool bEnableLineAA;
	
	FPipelineFileCacheRasterizerState& operator=(FRasterizerStateInitializerRHI const& Other)
	{
		DepthBias = Other.DepthBias;
		SlopeScaleDepthBias = Other.SlopeScaleDepthBias;
		FillMode = Other.FillMode;
		CullMode = Other.CullMode;
		DepthClipMode = Other.DepthClipMode;
		bAllowMSAA = Other.bAllowMSAA;
		bEnableLineAA = Other.bEnableLineAA;
		return *this;
	}
	
	operator FRasterizerStateInitializerRHI() const
	{
		FRasterizerStateInitializerRHI Initializer(FillMode, CullMode, DepthBias, SlopeScaleDepthBias, DepthClipMode, bAllowMSAA, bEnableLineAA);
		return Initializer;
	}
	
	friend RHI_API FArchive& operator<<(FArchive& Ar,FPipelineFileCacheRasterizerState& RasterizerStateInitializer)
	{
		Ar << RasterizerStateInitializer.DepthBias;
		Ar << RasterizerStateInitializer.SlopeScaleDepthBias;
		Ar << RasterizerStateInitializer.FillMode;
		Ar << RasterizerStateInitializer.CullMode;
		Ar << RasterizerStateInitializer.DepthClipMode;
		Ar << RasterizerStateInitializer.bAllowMSAA;
		Ar << RasterizerStateInitializer.bEnableLineAA;
		return Ar;
	}

	friend RHI_API uint32 GetTypeHash(const FPipelineFileCacheRasterizerState &Key)
	{
		uint32 KeyHash = (*((uint32*)&Key.DepthBias) ^ *((uint32*)&Key.SlopeScaleDepthBias));
		KeyHash ^= (Key.FillMode << 8);
		KeyHash ^= Key.CullMode;
		KeyHash ^= Key.DepthClipMode == ERasterizerDepthClipMode::DepthClamp ? 0x951f4c3b : 0; // crc32 "DepthClamp"
		KeyHash ^= Key.bAllowMSAA ? 0x694ea601 : 0; // crc32 "bAllowMSAA"
		KeyHash ^= Key.bEnableLineAA ? 0x48271d01 : 0; // crc32 "bEnableLineAA"
		return KeyHash;
	}
	FString ToString() const;
	void FromString(const FStringView& Src);
};

class FRayTracingPipelineStateInitializer;
class FRHIRayTracingShader;

/**
 * Tracks stats for the current session between opening & closing the file-cache.
 */
struct RHI_API FPipelineStateStats
{
	FPipelineStateStats()
	: FirstFrameUsed(-1)
	, LastFrameUsed(-1)
	, CreateCount(0)
	, TotalBindCount(0)
	, PSOHash(0)
	{
	}
	
	~FPipelineStateStats()
	{
	}

	static void UpdateStats(FPipelineStateStats* Stats);
	
	friend RHI_API FArchive& operator<<( FArchive& Ar, FPipelineStateStats& Info );

	int64 FirstFrameUsed;
	int64 LastFrameUsed;
	uint64 CreateCount;
	int64 TotalBindCount;
	uint32 PSOHash;
};

struct RHI_API FPipelineCacheFileFormatPSO
{
	using TReadableStringBuilder = TStringBuilder<1024>;

	struct RHI_API ComputeDescriptor
	{
		FSHAHash ComputeShader;

		FString ToString() const;
		void AddToReadableString(TReadableStringBuilder& OutBuilder) const;
		static FString HeaderLine();
		void FromString(const FStringView& Src);
	};
	struct RHI_API GraphicsDescriptor
	{
		FSHAHash VertexShader;
		FSHAHash FragmentShader;
		FSHAHash GeometryShader;
		FSHAHash MeshShader;
		FSHAHash AmplificationShader;

		FVertexDeclarationElementList VertexDescriptor;
		FBlendStateInitializerRHI BlendState;
		FPipelineFileCacheRasterizerState RasterizerState;
		FDepthStencilStateInitializerRHI DepthStencilState;
		
		EPixelFormat RenderTargetFormats[MaxSimultaneousRenderTargets];
		ETextureCreateFlags RenderTargetFlags[MaxSimultaneousRenderTargets];
		uint32 RenderTargetsActive;
		uint32 MSAASamples;
		
		EPixelFormat DepthStencilFormat;
		ETextureCreateFlags DepthStencilFlags;
		ERenderTargetLoadAction DepthLoad;
		ERenderTargetLoadAction StencilLoad;
		ERenderTargetStoreAction DepthStore;
		ERenderTargetStoreAction StencilStore;
		
		EPrimitiveType PrimitiveType;
		
		uint8 SubpassHint;	
		uint8 SubpassIndex;

		uint8	MultiViewCount;
		bool	bHasFragmentDensityAttachment;
		
		FString ToString() const;
		void AddToReadableString(TReadableStringBuilder& OutBuilder) const;
		static FString HeaderLine();
		bool FromString(const FStringView& Src);

		FString ShadersToString() const;
		void AddShadersToReadableString(TReadableStringBuilder& OutBuilder) const;

		static FString ShaderHeaderLine();
		void ShadersFromString(const FStringView& Src);

		FString StateToString() const;
		void AddStateToReadableString(TReadableStringBuilder& OutBuilder) const;
		static FString StateHeaderLine();
		bool StateFromString(const FStringView& Src);

		/** Not all RT flags make sense for the replayed PSO, only those that can influence the RT formats */
		static ETextureCreateFlags ReduceRTFlags(ETextureCreateFlags InFlags);
	};
	struct RHI_API FPipelineFileCacheRayTracingDesc
	{
		FSHAHash ShaderHash;
		uint32 MaxPayloadSizeInBytes = 0;
		EShaderFrequency Frequency = SF_RayGen;
		bool bAllowHitGroupIndexing = true;

		FPipelineFileCacheRayTracingDesc() = default;
		FPipelineFileCacheRayTracingDesc(const FRayTracingPipelineStateInitializer& Initializer, const FRHIRayTracingShader* ShaderRHI);

		FString ToString() const;
		void AddToReadableString(TReadableStringBuilder& OutBuilder) const;
		FString HeaderLine() const;
		void FromString(const FString& Src);

		friend uint32 GetTypeHash(const FPipelineFileCacheRayTracingDesc& Desc)
		{
			return GetTypeHash(Desc.ShaderHash) ^
				GetTypeHash(Desc.MaxPayloadSizeInBytes) ^
				GetTypeHash(Desc.Frequency) ^
				GetTypeHash(Desc.bAllowHitGroupIndexing);
		}

		bool operator == (const FPipelineFileCacheRayTracingDesc& Other) const
		{
			return ShaderHash == Other.ShaderHash &&
				MaxPayloadSizeInBytes == Other.MaxPayloadSizeInBytes &&
				Frequency == Other.Frequency &&
				bAllowHitGroupIndexing == Other.bAllowHitGroupIndexing;
		}
	};
	enum class DescriptorType : uint32
	{
		Compute = 0,
		Graphics = 1,
		RayTracing = 2,
	};
	
	DescriptorType Type;
	ComputeDescriptor ComputeDesc;
	GraphicsDescriptor GraphicsDesc;
	FPipelineFileCacheRayTracingDesc RayTracingDesc;

	mutable volatile uint32 Hash;
	
#if PSO_COOKONLY_DATA
	uint64 UsageMask;
	int64 BindCount;
#endif
	
	FPipelineCacheFileFormatPSO();
	~FPipelineCacheFileFormatPSO();
	
	FPipelineCacheFileFormatPSO& operator=(const FPipelineCacheFileFormatPSO& Other);
	FPipelineCacheFileFormatPSO(const FPipelineCacheFileFormatPSO& Other);
	
	bool operator==(const FPipelineCacheFileFormatPSO& Other) const;

	friend RHI_API uint32 GetTypeHash(const FPipelineCacheFileFormatPSO &Key);
	friend RHI_API FArchive& operator<<( FArchive& Ar, FPipelineCacheFileFormatPSO& Info );
	
	static bool Init(FPipelineCacheFileFormatPSO& PSO, FRHIComputeShader const* Init);
	static bool Init(FPipelineCacheFileFormatPSO& PSO, FGraphicsPipelineStateInitializer const& Init);
	static bool Init(FPipelineCacheFileFormatPSO & PSO, FPipelineFileCacheRayTracingDesc const& Desc);

	FString CommonToString() const;
	static FString CommonHeaderLine();
	void CommonFromString(const FStringView& Src);

	/** Prints out human-readable representation of the PSO, for any type */
	FString ToStringReadable();
	
	// Potential cases for seperating verify logic if requiired: RunTime-Logging, RunTime-UserCaching, RunTime-PreCompile, CommandLet-Cooking
	bool Verify() const;
};

struct RHI_API FPipelineCacheFileFormatPSORead
{	
	FPipelineCacheFileFormatPSORead()
	: Ar(nullptr)
	, Hash(0)
	, bReadCompleted(false)
	, bValid(false)
	{}
	
	~FPipelineCacheFileFormatPSORead()
	{
		if(Ar != nullptr)
		{
			delete Ar;
			Ar = nullptr;
		}
	}
	
	TArray<uint8> Data;
	FArchive* Ar;
	
	uint32 Hash;
	bool bReadCompleted;
	bool bValid;
	
	// Note that the contract of IAsyncReadFileHandle and IAsyncReadRequest requires that we delete the ReadRequest before deleting its ParentFileHandle. 
	// We therefore require that ParentFileHandle is declared before ReadRequest, so that the class destructor tears down first ReadRequest then ParentFileHandle.
	TSharedPtr<class IAsyncReadFileHandle, ESPMode::ThreadSafe> ParentFileHandle;
	TSharedPtr<class IAsyncReadRequest, ESPMode::ThreadSafe> ReadRequest;
};

struct RHI_API FPipelineCachePSOHeader
{
	TSet<FSHAHash> Shaders;
	uint32 Hash;
};

extern RHI_API const uint32 FPipelineCacheFileFormatCurrentVersion;

/*
 * User definable Mask Comparsion function:
 * @param ReferenceMask is the Current Bitmask set via SetGameUsageMask
 * @param PSOMask is the PSO UsageMask
 * @return Function should return true if this PSO is to be precompiled or false otherwise
 */
typedef bool(*FPSOMaskComparisonFn)(uint64 ReferenceMask, uint64 PSOMask);

/**
 * FPipelineFileCacheManager:
 * The RHI-level backend for FShaderPipelineCache, responsible for tracking PSOs and their usage stats as well as dealing with the pipeline cache files.
 * It is not expected that games or end-users invoke this directly, they should be calling FShaderPipelineCache which exposes this functionality in a usable form. 
 */

struct FPSOUsageData
{
	FPSOUsageData(): UsageMask(0), PSOHash(0), EngineFlags(0) {}
	FPSOUsageData(uint32 InPSOHash, uint64 InUsageMask, uint16 InEngineFlags): UsageMask(InUsageMask), PSOHash(InPSOHash), EngineFlags(InEngineFlags) {}
	uint64 UsageMask;
	uint32 PSOHash;
	uint16 EngineFlags;
};

class RHI_API FPipelineFileCacheManager
{
    friend class FPipelineCacheFile;
public:
	enum class SaveMode : uint32
	{
		Incremental = 0, // Fast(er) approach which saves new entries incrementally at the end of the file, replacing the table-of-contents, but leaves everything else alone.
        BoundPSOsOnly = 1, // Slower approach which consolidates and saves all PSOs used in this run of the program, removing any entry that wasn't seen, and sorted by the desired sort-mode.
	};
	
	enum class PSOOrder : uint32
	{
		Default = 0, // Whatever order they are already in.
		FirstToLatestUsed = 1, // Start with the PSOs with the lowest first-frame used and work toward those with the highest.
		MostToLeastUsed = 2 // Start with the most often used PSOs working toward the least.
	};

public:
	
	static void Initialize(uint32 GameVersion);
	static void Shutdown();
	
	static bool LoadPipelineFileCacheInto(FString const& Path, TSet<FPipelineCacheFileFormatPSO>& PSOs);
	static bool SavePipelineFileCacheFrom(uint32 GameVersion, EShaderPlatform Platform, FString const& Path, const TSet<FPipelineCacheFileFormatPSO>& PSOs);
	static bool MergePipelineFileCaches(FString const& PathA, FString const& PathB, FPipelineFileCacheManager::PSOOrder Order, FString const& OutputPath);
																				
	/* Open the pipeline file cache for the specfied name and platform. If successful, the GUID of the game file will be returned in OutGameFileGuid */
	static bool OpenPipelineFileCache(const FString& Key, const FString& CacheName, EShaderPlatform Platform, FGuid& OutGameFileGuid);

	/* Open the user pipeline file cache for the specified name and platform. The user cache is always created even if the file was not present when opened.
	* Name is the name used when opening the file, the key value for the user cache is held within UserCacheName.
	* returns true if the file was opened.
	*/
	static bool OpenUserPipelineFileCache(const FString& Key, const FString& CacheName, EShaderPlatform Platform, FGuid& OutGameFileGuid);

	static bool SavePipelineFileCache(SaveMode Mode);

	static void CloseUserPipelineFileCache();

	static void CacheGraphicsPSO(uint32 RunTimeHash, FGraphicsPipelineStateInitializer const& Initializer);
	static void CacheComputePSO(uint32 RunTimeHash, FRHIComputeShader const* Initializer);
	static void CacheRayTracingPSO(const FRayTracingPipelineStateInitializer& Initializer);

	// true if the named PSOFC is currently open.
	static bool HasPipelineFileCache(const FString& PSOCacheKey);

	static FPipelineStateStats* RegisterPSOStats(uint32 RunTimeHash);
	
	/*
	 * This PSO has failed compile and is invalid - this cache should not return this invalid PSO from subsequent calls for PreCompile requests.
	 * Note: Not implementated for Compute that has no flag to say it came from this cache - don't want to consume failures that didn't propagate from this cache.
	 */
	static void RegisterPSOCompileFailure(uint32 RunTimeHash, FGraphicsPipelineStateInitializer const& Initializer);
	
	/**
	 * Event signature for being notified that a new PSO has been logged
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FPipelineStateLoggedEvent, FPipelineCacheFileFormatPSO&);

	/**
	 * Gets the event delegate to register for pipeline state logging events.
	 */
	static FPipelineStateLoggedEvent& OnPipelineStateLogged();
	
	static void GetOrderedPSOHashes(const FString& PSOCacheKey, TArray<FPipelineCachePSOHeader>& PSOHashes, PSOOrder Order, int64 MinBindCount, TSet<uint32> const& AlreadyCompiledHashes);
	static void FetchPSODescriptors(const FString& PSOCacheKey, TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>& LoadedBatch);

	static int32 GetTotalPSOCount(const FString& PSOCacheKey);

	static uint32 NumPSOsLogged();
	
	static bool IsPipelineFileCacheEnabled();
	static bool LogPSOtoFileCache();
    static bool ReportNewPSOs();
	/* Report additional data about new PSOs to the log. */
	static bool LogPSODetails();
	
	/**
	 * Define the Current Game Usage Mask and a comparison function to compare this mask against the recorded mask in each PSO
	 * @param GameUsageMask Current Game Usage Mask to set, typically from user quality settings
	 * @param InComparisonFnPtr Pointer to the comparsion function - see above FPSOMaskComparisonFn definition for details
	 * @returns the old mask
	 */
	static uint64 SetGameUsageMaskWithComparison(uint64 GameUsageMask, FPSOMaskComparisonFn InComparisonFnPtr);
	static uint64 GetGameUsageMask()	{ return GameUsageMask;}
	
	static void PreCompileComplete();
private:
	
	static void RegisterPSOUsageDataUpdateForNextSave(FPSOUsageData& UsageData);
	static void ClearOSPipelineCache();
	static bool ShouldEnableFileCache();
	
	static bool IsBSSEquivalentPSOEntryCached(FPipelineCacheFileFormatPSO const& NewEntry);
	static bool IsPSOEntryCached(FPipelineCacheFileFormatPSO const& NewEntry, FPSOUsageData* EntryData = nullptr);
private:
	static FRWLock FileCacheLock;

	// Containers for the multiple bundled PSOFCs
	// Name to PipelineCacheFile
	static TMap<FString, TUniquePtr<class FPipelineCacheFile>> FileCacheMap;
	// PipelineCacheFile GUID to Name
	static TMap<FGuid, FString> GameGuidToCacheKey;
	// User cache's key within FileCacheMap
	static FString UserCacheKey;
	// Helper for retrieving a file cache from the name.
	static class FPipelineCacheFile* GetPipelineCacheFileFromKey(const FString& PSOCacheKey)
	{
		TUniquePtr<FPipelineCacheFile>* FileCacheFound = FileCacheMap.Find(PSOCacheKey);
		return FileCacheFound ? FileCacheFound->Get() : nullptr;
	}

	// PSO recording
	static TMap<uint32, FPSOUsageData> RunTimeToPSOUsage;		// Fast check structure - Not saved (External state cache runtime hash to seen usage data)
	static TMap<uint32, FPSOUsageData> NewPSOUsage;				// For mask or engine updates - Merged + Saved (Our internal PSO hash to latest usage data) - temp working scratch, only holds updates since last "save" so is not the authority on state
	static TMap<uint32, FPipelineStateStats*> Stats;
	static TSet<FPipelineCacheFileFormatPSO> NewPSOs;
 	static TSet<uint32> NewPSOHashes;
	static uint32 NumNewPSOs;
	static PSOOrder RequestedOrder;
	static bool FileCacheEnabled;
	static FPipelineStateLoggedEvent PSOLoggedEvent;
	static uint64 GameUsageMask;
	static FPSOMaskComparisonFn MaskComparisonFn;
};
