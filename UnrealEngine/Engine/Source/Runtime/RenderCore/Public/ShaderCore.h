// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCore.h: Shader core module definitions.
=============================================================================*/

#pragma once

#include "Compression/OodleDataCompression.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/SortedMap.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/CoreStats.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Optional.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/TVariant.h"
#include "PixelFormat.h"
#include "RHIDefinitions.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryLayout.h"
#include "ShaderParameterMetadata.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "Templates/Function.h"
#include "Templates/PimplPtr.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "UniformBuffer.h"

// Temporarily included here until we can fully deprecate access to FShaderCompilerDefinitions in a future version
#include "ShaderCompilerDefinitions.h"

class Error;
class FMemoryImageWriter;
class FMemoryUnfreezeContent;
class FPointerTableBase;
class FShaderCompilerDefinitions;
class FShaderCompileUtilities;
class FShaderPreprocessorUtilities;
class FSHA1;
class ITargetPlatform;

using FShaderStatVariant = TVariant<bool, float, int32, uint32>;
DECLARE_INTRINSIC_TYPE_LAYOUT(FShaderStatVariant);

/**
 * Controls whether shader related logs are visible.
 * Note: The runtime verbosity is driven by the console variable 'r.ShaderDevelopmentMode'
 */
#if UE_BUILD_DEBUG && (PLATFORM_UNIX)
RENDERCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogShaders, Log, All);
#else
RENDERCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogShaders, Error, All);
#endif

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Total Niagara Shaders"), STAT_ShaderCompiling_NumTotalNiagaraShaders, STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total Niagara Shader Compiling Time"), STAT_ShaderCompiling_NiagaraShaders, STATGROUP_ShaderCompiling, RENDERCORE_API);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Total OpenColorIO Shaders"), STAT_ShaderCompiling_NumTotalOpenColorIOShaders, STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total OpenColorIO Shader Compiling Time"), STAT_ShaderCompiling_OpenColorIOShaders, STATGROUP_ShaderCompiling, RENDERCORE_API);

DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total Material Shader Compiling Time"),STAT_ShaderCompiling_MaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total Global Shader Compiling Time"),STAT_ShaderCompiling_GlobalShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("RHI Compile Time"),STAT_ShaderCompiling_RHI,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Loading Shader Files"),STAT_ShaderCompiling_LoadingShaderFiles,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("CRCing Shader Files"),STAT_ShaderCompiling_HashingShaderFiles,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("HLSL Translation"), STAT_ShaderCompiling_HLSLTranslation, STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("DDC Loading"),STAT_ShaderCompiling_DDCLoading,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Material Loading"),STAT_ShaderCompiling_MaterialLoading,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Material Compiling"),STAT_ShaderCompiling_MaterialCompiling,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Total Material Shaders"),STAT_ShaderCompiling_NumTotalMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Special Material Shaders"),STAT_ShaderCompiling_NumSpecialMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Particle Material Shaders"),STAT_ShaderCompiling_NumParticleMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Skinned Material Shaders"),STAT_ShaderCompiling_NumSkinnedMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Lit Material Shaders"),STAT_ShaderCompiling_NumLitMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Unlit Material Shaders"),STAT_ShaderCompiling_NumUnlitMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Transparent Material Shaders"),STAT_ShaderCompiling_NumTransparentMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Opaque Material Shaders"),STAT_ShaderCompiling_NumOpaqueMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Masked Material Shaders"),STAT_ShaderCompiling_NumMaskedMaterialShaders,STATGROUP_ShaderCompiling, RENDERCORE_API);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shaders Loaded"),STAT_Shaders_NumShadersLoaded,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shader Resources Loaded"),STAT_Shaders_NumShaderResourcesLoaded,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shader Maps Registered"),STAT_Shaders_NumShaderMaps,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RT Shader Load Time"),STAT_Shaders_RTShaderLoadTime,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shaders Used"),STAT_Shaders_NumShadersUsedForRendering,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total RT Shader Init Time"),STAT_Shaders_TotalRTShaderInitForRenderingTime,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Frame RT Shader Init Time"),STAT_Shaders_FrameRTShaderInitForRenderingTime,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Shader Memory"),STAT_Shaders_ShaderMemory,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Shader Resource Mem"),STAT_Shaders_ShaderResourceMemory,STATGROUP_Shaders, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Shader Preload Mem"), STAT_Shaders_ShaderPreloadMemory, STATGROUP_Shaders, RENDERCORE_API);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shaders Registered"), STAT_Shaders_NumShadersRegistered, STATGROUP_Shaders, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shaders Duplicated"), STAT_Shaders_NumShadersDuplicated, STATGROUP_Shaders, RENDERCORE_API);

inline TStatId GetMemoryStatType(EShaderFrequency ShaderFrequency)
{
	static_assert(10 == SF_NumFrequencies, "EShaderFrequency has a bad size.");

	switch(ShaderFrequency)
	{
		case SF_Pixel:				return GET_STATID(STAT_PixelShaderMemory);
		case SF_Compute:			return GET_STATID(STAT_PixelShaderMemory);
		case SF_RayGen:				return GET_STATID(STAT_PixelShaderMemory);
		case SF_RayMiss:			return GET_STATID(STAT_PixelShaderMemory);
		case SF_RayHitGroup:		return GET_STATID(STAT_PixelShaderMemory);
		case SF_RayCallable:		return GET_STATID(STAT_PixelShaderMemory);
	}
	return GET_STATID(STAT_VertexShaderMemory);
}

/** Initializes shader hash cache from IShaderFormatModules. This must be called before reading any shader include. */
extern RENDERCORE_API void InitializeShaderHashCache();

/** Updates the PreviewPlatform's IncludeDirectory to match that of the Parent Platform*/
extern RENDERCORE_API void UpdateIncludeDirectoryForPreviewPlatform(EShaderPlatform PreviewPlatform, EShaderPlatform ActualPlatform);

/** Checks if shader include isn't skipped by a shader hash cache. */
extern RENDERCORE_API void CheckShaderHashCacheInclude(const FString& VirtualFilePath, EShaderPlatform ShaderPlatform, const FString& ShaderFormatName);

/** Initializes cached shader type data.  This must be called before creating any FShaderType. */
extern RENDERCORE_API void InitializeShaderTypes();

/** Uninitializes cached shader type data.  This is needed before unloading modules that contain FShaderTypes. */
extern RENDERCORE_API void UninitializeShaderTypes();

/** Returns true if debug viewmodes are allowed for the current platform. */
extern RENDERCORE_API bool AllowDebugViewmodes();

/** Returns true if debug viewmodes are allowed for the given platform. */
extern RENDERCORE_API bool AllowDebugViewmodes(EShaderPlatform Platform);

/** Returns the shader compression format. Oodle is used exclusively now. r.Shaders.SkipCompression configures Oodle to be uncompressed instead of returning NAME_None.*/
extern RENDERCORE_API FName GetShaderCompressionFormat();

/** Returns Oodle-specific shader compression format settings (passing ShaderFormat for future proofing, but as of now the setting is global for all formats). */
extern RENDERCORE_API void GetShaderCompressionOodleSettings(FOodleDataCompression::ECompressor& OutCompressor, FOodleDataCompression::ECompressionLevel& OutLevel, const FName& ShaderFormat = NAME_None);

struct FShaderTarget
{
	// The rest of uint32 holding the bitfields can be left unitialized. Union with a uint32 serves to prevent that to be able to set the whole uint32 value
	union
	{
		uint32 Packed;
		struct
		{
			uint32 Frequency : SF_NumBits;
			uint32 Platform : SP_NumBits;
		};
	};

	FShaderTarget()
		: Packed(0)
	{}

	FShaderTarget(EShaderFrequency InFrequency,EShaderPlatform InPlatform)
	:	Packed(0)
	{
		Frequency = InFrequency;
		Platform = InPlatform;
	}

	friend bool operator==(const FShaderTarget& X, const FShaderTarget& Y)
	{
		return X.Frequency == Y.Frequency && X.Platform == Y.Platform;
	}

	friend FArchive& operator<<(FArchive& Ar,FShaderTarget& Target)
	{
		uint32 TargetFrequency = Target.Frequency;
		uint32 TargetPlatform = Target.Platform;
		Ar << TargetFrequency << TargetPlatform;
		if (Ar.IsLoading())
		{
			Target.Packed = 0;
			Target.Frequency = TargetFrequency;
			Target.Platform = TargetPlatform;
		}
		return Ar;
	}

	EShaderPlatform GetPlatform() const
	{
		return (EShaderPlatform)Platform;
	}

	EShaderFrequency GetFrequency() const
	{
		return (EShaderFrequency)Frequency;
	}

	friend inline uint32 GetTypeHash(FShaderTarget Target)
	{
		return ((Target.Frequency << SP_NumBits) | Target.Platform);
	}
};
DECLARE_INTRINSIC_TYPE_LAYOUT(FShaderTarget);

static_assert(sizeof(FShaderTarget) == sizeof(uint32), "FShaderTarget is expected to be bit-packed into a single uint32.");

enum class EShaderParameterType : uint8
{
	LooseData,
	UniformBuffer,
	Sampler,
	SRV,
	UAV,

	BindlessSampler,
	BindlessSRV,
	BindlessUAV,

	Num
};

enum class EShaderParameterTypeMask : uint16
{
	LooseDataMask = 1 << uint16(EShaderParameterType::LooseData),
	UniformBufferMask = 1 << uint16(EShaderParameterType::UniformBuffer),
	SamplerMask = 1 << uint16(EShaderParameterType::Sampler),
	SRVMask = 1 << uint16(EShaderParameterType::SRV),
	UAVMask = 1 << uint16(EShaderParameterType::UAV),
	BindlessSamplerMask = 1 << uint16(EShaderParameterType::BindlessSampler),
	BindlessSRVMask = 1 << uint16(EShaderParameterType::BindlessSRV),
	BindlessUAVMask = 1 << uint16(EShaderParameterType::BindlessUAV),
};
ENUM_CLASS_FLAGS(EShaderParameterTypeMask);

inline bool IsParameterBindless(EShaderParameterType ParameterType)
{
	return ParameterType == EShaderParameterType::BindlessSampler
		|| ParameterType == EShaderParameterType::BindlessSRV
		|| ParameterType == EShaderParameterType::BindlessUAV
		;
}

struct FParameterAllocation
{
	uint16 BufferIndex = 0;
	uint16 BaseIndex = 0;
	uint16 Size = 0;
	EShaderParameterType Type{ EShaderParameterType::Num };
	mutable bool bBound = false;

	FParameterAllocation() = default;
	FParameterAllocation(uint16 InBufferIndex, uint16 InBaseIndex, uint16 InSize, EShaderParameterType InType)
		: BufferIndex(InBufferIndex)
		, BaseIndex(InBaseIndex)
		, Size(InSize)
		, Type(InType)
	{
	}

	friend FArchive& operator<<(FArchive& Ar,FParameterAllocation& Allocation)
	{
		Ar << Allocation.BufferIndex << Allocation.BaseIndex << Allocation.Size << Allocation.bBound;
		Ar << Allocation.Type;
		return Ar;
	}

	friend inline bool operator==(const FParameterAllocation& A, const FParameterAllocation& B)
	{
		return
			A.BufferIndex == B.BufferIndex && A.BaseIndex == B.BaseIndex && A.Size == B.Size && A.Type == B.Type && A.bBound == B.bBound;
	}

	friend inline bool operator!=(const FParameterAllocation& A, const FParameterAllocation& B)
	{
		return !(A == B);
	}
};

/**
 * A map of shader parameter names to registers allocated to that parameter.
 */
class FShaderParameterMap
{
public:

	FShaderParameterMap()
	{}

	RENDERCORE_API TOptional<FParameterAllocation> FindParameterAllocation(const FString& ParameterName) const;
	RENDERCORE_API bool FindParameterAllocation(const TCHAR* ParameterName,uint16& OutBufferIndex,uint16& OutBaseIndex,uint16& OutSize) const;
	RENDERCORE_API bool ContainsParameterAllocation(const TCHAR* ParameterName) const;
	RENDERCORE_API void AddParameterAllocation(const TCHAR* ParameterName,uint16 BufferIndex,uint16 BaseIndex,uint16 Size,EShaderParameterType ParameterType);
	RENDERCORE_API void RemoveParameterAllocation(const TCHAR* ParameterName);

	/** Returns an array of all parameters with the given type. */
	RENDERCORE_API TArray<FString> GetAllParameterNamesOfType(EShaderParameterType InType) const;

	/** Checks that all parameters are bound and asserts if any aren't in a debug build
	* @param InVertexFactoryType can be 0
	*/
	RENDERCORE_API void VerifyBindingsAreComplete(const TCHAR* ShaderTypeName, FShaderTarget Target, const class FVertexFactoryType* InVertexFactoryType) const;

	/** Updates the hash state with the contents of this parameter map. */
	void UpdateHash(FSHA1& HashState) const;

	friend FArchive& operator<<(FArchive& Ar,FShaderParameterMap& InParameterMap)
	{
		// Note: this serialize is used to pass between UE and the shader compile worker, recompile both when modifying
		Ar << InParameterMap.ParameterMap;
		return Ar;
	}

	inline void GetAllParameterNames(TArray<FString>& OutNames) const
	{
		ParameterMap.GenerateKeyArray(OutNames);
	}

	inline const TMap<FString, FParameterAllocation>& GetParameterMap() const { return ParameterMap; }

	TMap<FString,FParameterAllocation> ParameterMap;
};

struct FShaderResourceTable
{
	/** Bits indicating which resource tables contain resources bound to this shader. */
	uint32 ResourceTableBits = 0;

	/** Mapping of bound SRVs to their location in resource tables. */
	TArray<uint32> ShaderResourceViewMap;

	/** Mapping of bound sampler states to their location in resource tables. */
	TArray<uint32> SamplerMap;

	/** Mapping of bound UAVs to their location in resource tables. */
	TArray<uint32> UnorderedAccessViewMap;

	/** Hash of the layouts of resource tables at compile time, used for runtime validation. */
	TArray<uint32> ResourceTableLayoutHashes;

	/** Mapping of bound Textures to their location in resource tables. */
	TArray<uint32> TextureMap;

	friend bool operator==(const FShaderResourceTable&A, const FShaderResourceTable& B)
	{
		bool bEqual = true;
		bEqual &= (A.ResourceTableBits == B.ResourceTableBits);
		bEqual &= (A.ShaderResourceViewMap    .Num() == B.ShaderResourceViewMap    .Num());
		bEqual &= (A.SamplerMap               .Num() == B.SamplerMap               .Num());
		bEqual &= (A.UnorderedAccessViewMap   .Num() == B.UnorderedAccessViewMap   .Num());
		bEqual &= (A.ResourceTableLayoutHashes.Num() == B.ResourceTableLayoutHashes.Num());
		bEqual &= (A.TextureMap               .Num() == B.TextureMap               .Num());

		if (!bEqual)
		{
			return false;
		}

		bEqual &= (FMemory::Memcmp(A.ShaderResourceViewMap    .GetData(), B.ShaderResourceViewMap    .GetData(), A.ShaderResourceViewMap    .GetTypeSize() * A.ShaderResourceViewMap    .Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.SamplerMap               .GetData(), B.SamplerMap               .GetData(), A.SamplerMap               .GetTypeSize() * A.SamplerMap               .Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.UnorderedAccessViewMap   .GetData(), B.UnorderedAccessViewMap   .GetData(), A.UnorderedAccessViewMap   .GetTypeSize() * A.UnorderedAccessViewMap   .Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.ResourceTableLayoutHashes.GetData(), B.ResourceTableLayoutHashes.GetData(), A.ResourceTableLayoutHashes.GetTypeSize() * A.ResourceTableLayoutHashes.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.TextureMap               .GetData(), B.TextureMap               .GetData(), A.TextureMap               .GetTypeSize() * A.TextureMap               .Num()) == 0);
		return bEqual;
	}
};

inline FArchive& operator<<(FArchive& Ar, FShaderResourceTable& SRT)
{
	Ar << SRT.ResourceTableBits;
	Ar << SRT.ShaderResourceViewMap;
	Ar << SRT.SamplerMap;
	Ar << SRT.UnorderedAccessViewMap;
	Ar << SRT.ResourceTableLayoutHashes;
	Ar << SRT.TextureMap;

	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FUniformResourceEntry& Entry)
{
	if (Ar.IsLoading())
	{
		// Filled in later in FShaderResourceTableMap::FixupOnLoad
		Entry.UniformBufferMemberName = nullptr;
	}
	Ar << Entry.UniformBufferNameLength;
	Ar << Entry.Type;
	Ar << Entry.ResourceIndex;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FUniformBufferEntry& Entry)
{
	Ar << Entry.StaticSlotName;
	Ar << Entry.LayoutHash;
	Ar << Entry.BindingFlags;
	Ar << Entry.bNoEmulatedUniformBuffer;
	if (Ar.IsLoading())
	{
		Entry.MemberNameBuffer = MakeShareable(new TArray<TCHAR>());
	}
	Ar << *Entry.MemberNameBuffer.Get();
	return Ar;
}

using FThreadSafeSharedStringPtr = TSharedPtr<FString, ESPMode::ThreadSafe>;
using FThreadSafeNameBufferPtr = TSharedPtr<TArray<TCHAR>, ESPMode::ThreadSafe>;

// Simple wrapper for a uint64 bitfield; doesn't use TBitArray as it is fixed size and doesn't need dynamic memory allocations
class FShaderCompilerFlags
{
public:
	FShaderCompilerFlags(uint64 InData = 0)
		: Data(InData)
	{
	}

	inline void Append(const FShaderCompilerFlags& In)
	{
		Data |= In.Data;
	}

	inline void Add(uint32 InFlag)
	{
		const uint64 FlagBit = (uint64)1 << (uint64)InFlag;
		Data = Data | FlagBit;
	}

	inline void Remove(uint32 InFlag)
	{
		const uint64 FlagBit = (uint64)1 << (uint64)InFlag;
		Data = Data & ~FlagBit;
	}

	inline bool Contains(uint32 InFlag) const
	{
		const uint64 FlagBit = (uint64)1 << (uint64)InFlag;
		return (Data & FlagBit) == FlagBit;
	}

	inline void Iterate(TFunction<void(uint32)> Callback) const
	{
		uint64 Remaining = Data;
		uint32 Index = 0;
		while (Remaining)
		{
			if (Remaining & (uint64)1)
			{
				Callback(Index);
			}
			++Index;
			Remaining = Remaining >> (uint64)1;
		}
	}

	friend inline FArchive& operator << (FArchive& Ar, FShaderCompilerFlags& F)
	{
		Ar << F.Data;
		return Ar;
	}

	inline uint64 GetData() const
	{
		return Data;
	}

private:
	uint64 Data;
};

struct FShaderResourceTableMap
{
	TArray<FUniformResourceEntry> Resources;

	RENDERCORE_API void Append(const FShaderResourceTableMap& Other);
	RENDERCORE_API void FixupOnLoad(const TMap<FString, FUniformBufferEntry>& UniformBufferMap);
};

/** The environment used to compile a shader. */
struct FShaderCompilerEnvironment
{
	// Map of the virtual file path -> content.
	// The virtual file paths are the ones that USF files query through the #include "<The Virtual Path of the file>"
	TMap<FString,FString> IncludeVirtualPathToContentsMap;

	TMap<FString, FThreadSafeSharedAnsiStringPtr> IncludeVirtualPathToSharedContentsMap;

	UE_DEPRECATED(5.4, "IncludeVirtualPathToExternalContentsMap has been replaced with IncludeVirtualPathToSharedContentsMap (type change from FString to ANSI string).")
	TMap<FString, FThreadSafeSharedStringPtr> IncludeVirtualPathToExternalContentsMap;

	FShaderCompilerFlags CompilerFlags;
	TMap<uint32,uint8> RenderTargetOutputFormatsMap;
	FShaderResourceTableMap ResourceTableMap;
	TMap<FString, FUniformBufferEntry> UniformBufferMap;

	UE_DEPRECATED(5.3, "RemoteServerData field is deprecated (no longer used in compilation backends).")
	TMap<FString, FString> RemoteServerData;

	const ITargetPlatform* TargetPlatform = nullptr;

	// Used for mobile platforms to allow per shader/material precision modes
	bool FullPrecisionInPS = 0;

	/** Default constructor. */
	RENDERCORE_API FShaderCompilerEnvironment();

	/** Initialization constructor. */
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.4, "FShaderCompilerDefinitions is being made private in the future, do not use this constructor.")
	RENDERCORE_API explicit FShaderCompilerEnvironment(const FShaderCompilerDefinitions& InDefinitions);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Used as a baseclass, make sure we're not incorrectly destroyed through a baseclass pointer
	// This will be expensive to destroy anyway, additional vcall overhead should be small
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Temporarily disable deprecation warnings for this default destructor triggering due to the 
	// deprecated RemoteServerData field. When the field is removed warning disable should be as 
	// well, but the defaulted destructor should remain due to the above comment.
	virtual ~FShaderCompilerEnvironment() = default;

	// Explicitly default assignment operator and copy constructor operator with warnings disabled
	// to avoid warnings in implicitly-generated functions due to deprecation of RemoteServerData. 
	// These can be removed entirely (revert to implicitly-generated) when the field itself is.
	FShaderCompilerEnvironment(const FShaderCompilerEnvironment&) = default;
	FShaderCompilerEnvironment& operator=(const FShaderCompilerEnvironment&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Works for TCHAR
	 * e.g. SetDefine(TEXT("NAME"), TEXT("Test"));
	 * e.g. SetDefine(TEXT("NUM_SAMPLES"), 1);
	 * e.g. SetDefine(TEXT("DOIT"), true);
	 *
	 * Or use optimized macros, which can cache FName and map lookups to improve performance:
	 * e.g. SET_SHADER_DEFINE(NAME, TEXT("Test"));
	 * e.g. SET_SHADER_DEFINE(NUM_SAMPLES, 1);
	 * e.g. SET_SHADER_DEFINE(DOIT, true);
	 */
	RENDERCORE_API void SetDefine(const TCHAR* Name, const TCHAR* Value);
	RENDERCORE_API void SetDefine(const TCHAR* Name, const FString& Value);
	RENDERCORE_API void SetDefine(const TCHAR* Name, uint32 Value);
	RENDERCORE_API void SetDefine(const TCHAR* Name, int32 Value);
	RENDERCORE_API void SetDefine(const TCHAR* Name, bool Value);
	RENDERCORE_API void SetDefine(const TCHAR* Name, float Value);

	RENDERCORE_API void SetDefine(FName Name, const TCHAR* Value);
	RENDERCORE_API void SetDefine(FName Name, const FString& Value);
	RENDERCORE_API void SetDefine(FName Name, uint32 Value);
	RENDERCORE_API void SetDefine(FName Name, int32 Value);
	RENDERCORE_API void SetDefine(FName Name, bool Value);
	RENDERCORE_API void SetDefine(FName Name, float Value);

	RENDERCORE_API void SetDefine(FShaderCompilerDefineNameCache& Name, const TCHAR* Value);
	RENDERCORE_API void SetDefine(FShaderCompilerDefineNameCache& Name, const FString& Value);
	RENDERCORE_API void SetDefine(FShaderCompilerDefineNameCache& Name, uint32 Value);
	RENDERCORE_API void SetDefine(FShaderCompilerDefineNameCache& Name, int32 Value);
	RENDERCORE_API void SetDefine(FShaderCompilerDefineNameCache& Name, bool Value);
	RENDERCORE_API void SetDefine(FShaderCompilerDefineNameCache& Name, float Value);

	RENDERCORE_API int32 GetIntegerValue(FName Name) const;
	RENDERCORE_API int32 GetIntegerValue(FShaderCompilerDefineNameCache& NameCache, int32 ResultIfNotFound = 0) const;

	RENDERCORE_API bool ContainsDefinition(FName Name) const;

	template <typename ValueType> void SetDefineIfUnset(const TCHAR* Name, ValueType Value)
	{
		FName NameKey(Name);
		if (!ContainsDefinition(NameKey))
		{
			SetDefine(NameKey, Value);
		}
	}


	// Sets a generic parameter which can be read in the various shader format backends to modify compilation
	// behaviour. Intended to replace any usage of definitions after shader preprocessing.
	template <typename ValueType> void SetCompileArgument(const TCHAR* Name, ValueType Value)
	{
		CompileArgs.Add(Name, TVariant<bool, float, int32, uint32, FString>(TInPlaceType<ValueType>(), Value));
	}
	
	// Like above, but this overload takes in the define value variant explicitly.
	void SetCompileArgument(const TCHAR* Name, TVariant<bool, float, int32, uint32, FString> Value)
	{
		CompileArgs.Add(Name, MoveTempIfPossible(Value));
	}

	// Helper to set both a define and a compile argument to the same value. Useful for various parameters which
	// need to be consumed both by preprocessing and in the shader format backends to modify compilation behaviour.
	template <typename ValueType> void SetDefineAndCompileArgument(const TCHAR* Name, ValueType Value)
	{
		SetDefine(Name, Value);
		SetCompileArgument(Name, Value);
	}

	// If a compile argument with the given name exists, returns true. 
	bool HasCompileArgument(const TCHAR* Name) const
	{
		if (CompileArgs.Contains(Name))
		{
			return true;
		}
		return false;
	}

	// If a compile argument with the given name exists and is of the specified type, returns its value. Otherwise, 
	// either the named argument doesn't exist or the type does not match, and the default value will be returned.
	template <typename ValueType> ValueType GetCompileArgument(const TCHAR* Name, const ValueType& DefaultValue) const
	{
		const TVariant<bool, float, int32, uint32, FString>* StoredValue = CompileArgs.Find(Name);
		if (StoredValue && StoredValue->IsType<ValueType>())
		{
			return StoredValue->Get<ValueType>();
		}
		return DefaultValue;
	}

	// If a compile argument with the given name exists and is of the specified type, its value will be assigned to OutValue
	// and the function will return true. Otherwise, either the named argument doesn't exist or the type does not match, the
	// OutValue will be left unmodified and the function will return false.
	template <typename ValueType> bool GetCompileArgument(const TCHAR* Name, ValueType& OutValue) const
	{
		const TVariant<bool, float, int32, uint32, FString>* StoredValue = CompileArgs.Find(Name);
		if (StoredValue && StoredValue->IsType<ValueType>())
		{
			OutValue = StoredValue->Get<ValueType>();
			return true;
		}
		return false;
	}

	UE_DEPRECATED(5.3, "GetDefinitions is deprecated; preprocessor defines must now only be accessed by core shader system code. Use Get/SetCompileArgument for generic params instead.")
	const TMap<FString,FString>& GetDefinitions() const
	{
		return UnusedStringDefinitions;
	}

	void SetRenderTargetOutputFormat(uint32 RenderTargetIndex, EPixelFormat PixelFormat)
	{
		RenderTargetOutputFormatsMap.Add(RenderTargetIndex, UE_PIXELFORMAT_TO_UINT8(PixelFormat));
	}

	/** This "core" serialization is also used for the hashing the compiler job (where files are handled differently). Should stay in sync with the ShaderCompileWorker. */
	RENDERCORE_API void SerializeEverythingButFiles(FArchive& Ar);

	// Serializes the portions of the environment that are used as input to the backend compilation process (i.e. after all preprocessing)
	RENDERCORE_API void SerializeCompilationDependencies(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerEnvironment& Environment)
	{
		// Note: this serialize is used to pass between UE and the shader compile worker, recompile both when modifying
		Ar << Environment.IncludeVirtualPathToContentsMap;

		// Note: skipping Environment.IncludeVirtualPathToSharedContentsMap, which is handled by FShaderCompileUtilities::DoWriteTasks in order to maintain sharing

		Environment.SerializeEverythingButFiles(Ar);
		return Ar;
	}
	
	RENDERCORE_API void Merge(const FShaderCompilerEnvironment& Other);

	RENDERCORE_API FString GetDefinitionsAsCommentedCode() const;

private:

	friend class FShaderCompileUtilities;
	friend class FShaderPreprocessorUtilities;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	TPimplPtr<FShaderCompilerDefinitions, EPimplPtrMode::DeepCopy> Definitions;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	TMap<FString, TVariant<bool, float, int32, uint32, FString>> CompileArgs;

	/** Unused data kept around for deprecated FShaderCompilerEnvironment::GetDefinitions call */
	TMap<FString, FString> UnusedStringDefinitions;
};


/** Optimized define setting macros that cache the FName lookup, and potentially the map index. */
#define SET_SHADER_DEFINE(ENVIRONMENT, NAME, VALUE) \
	do {																	\
		static FShaderCompilerDefineNameCache Cache_##NAME(TEXT(#NAME));	\
		(ENVIRONMENT).SetDefine(Cache_##NAME, VALUE);						\
	} while(0)

#define SET_SHADER_DEFINE_AND_COMPILE_ARGUMENT(ENVIRONMENT, NAME, VALUE) \
	do {																	\
		static FShaderCompilerDefineNameCache Cache_##NAME(TEXT(#NAME));	\
		(ENVIRONMENT).SetDefine(Cache_##NAME, VALUE);						\
		(ENVIRONMENT).SetCompileArgument(TEXT(#NAME), VALUE);				\
	} while(0)


struct FSharedShaderCompilerEnvironment final : public FShaderCompilerEnvironment, public FRefCountBase
{
	virtual ~FSharedShaderCompilerEnvironment() = default;
};

enum class EShaderOptionalDataKey : uint8
{
	AttributeInputs      = uint8('i'),
	AttributeOutputs     = uint8('o'),
	CompressedDebugCode  = uint8('z'),
	Diagnostic           = uint8('D'),
	Features             = uint8('x'),
	Name                 = uint8('n'),
	NativePath           = uint8('P'),
	ObjectFile           = uint8('O'),
	PackedResourceCounts = uint8('p'),
	ResourceMasks        = uint8('m'),
	ShaderModel6         = uint8('6'),
	SourceCode           = uint8('c'),
	UncompressedSize     = uint8('U'),
	UniformBuffers       = uint8('u'),
	Validation           = uint8('V'),
	VendorExtension      = uint8('v'),
};

enum class EShaderResourceUsageFlags : uint8
{
	GlobalUniformBuffer   = 1 << 0,
	BindlessResources     = 1 << 1,
	BindlessSamplers      = 1 << 2,
	RootConstants         = 1 << 3,
	NoDerivativeOps       = 1 << 4,
	ShaderBundle          = 1 << 5,
};
ENUM_CLASS_FLAGS(EShaderResourceUsageFlags)

// if this changes you need to make sure all shaders get invalidated
struct FShaderCodePackedResourceCounts
{
	// for FindOptionalData() and AddOptionalData()
	static const EShaderOptionalDataKey Key = EShaderOptionalDataKey::PackedResourceCounts;

	EShaderResourceUsageFlags UsageFlags;
	uint8 NumSamplers;
	uint8 NumSRVs;
	uint8 NumCBs;
	uint8 NumUAVs;
};

struct FShaderCodeResourceMasks
{
	// for FindOptionalData() and AddOptionalData()
	static const EShaderOptionalDataKey Key = EShaderOptionalDataKey::ResourceMasks;

	uint32 UAVMask; // Mask of UAVs bound
};

// if this changes you need to make sure all shaders get invalidated
enum class EShaderCodeFeatures : uint16
{
	None                    = 0,
	WaveOps                 = 1 << 0,
	SixteenBitTypes         = 1 << 1,
	TypedUAVLoadsExtended   = 1 << 2,
	Atomic64                = 1 << 3,
	DiagnosticBuffer        = 1 << 4,
	BindlessResources       = 1 << 5,
	BindlessSamplers        = 1 << 6,
	StencilRef              = 1 << 7,
	BarycentricsSemantic    = 1 << 8,
};
ENUM_CLASS_FLAGS(EShaderCodeFeatures);

struct FShaderCodeFeatures
{
	// for FindOptionalData() and AddOptionalData()
	static const EShaderOptionalDataKey Key = EShaderOptionalDataKey::Features;

	EShaderCodeFeatures CodeFeatures = EShaderCodeFeatures::None;
};

// if this changes you need to make sure all shaders get invalidated
struct FShaderCodeName
{
	static const EShaderOptionalDataKey Key = EShaderOptionalDataKey::Name;

	// We store the straight ANSICHAR zero-terminated string
};

struct FShaderCodeUniformBuffers
{
	static const EShaderOptionalDataKey Key = EShaderOptionalDataKey::UniformBuffers;
	// We store an array of FString objects
};

// if this changes you need to make sure all shaders get invalidated
struct FShaderCodeVendorExtension
{
	// for FindOptionalData() and AddOptionalData()
	static const EShaderOptionalDataKey Key = EShaderOptionalDataKey::VendorExtension;

	EGpuVendorId VendorId = EGpuVendorId::NotQueried;
	FParameterAllocation Parameter;

	FShaderCodeVendorExtension() = default;
	FShaderCodeVendorExtension(EGpuVendorId InVendorId, uint16 InBufferIndex, uint16 InBaseIndex, uint16 InSize, EShaderParameterType InType)
		: VendorId(InVendorId)
		, Parameter(InBufferIndex, InBaseIndex, InSize, InType)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FShaderCodeVendorExtension& Extension)
	{
		return Ar << Extension.VendorId << Extension.Parameter;
	}

	friend inline bool operator==(const FShaderCodeVendorExtension& A, const FShaderCodeVendorExtension& B)
	{
		return A.VendorId == B.VendorId && A.Parameter == B.Parameter;
	}

	friend inline bool operator!=(const FShaderCodeVendorExtension& A, const FShaderCodeVendorExtension& B)
	{
		return !(A == B);
	}

};


inline FArchive& operator<<(FArchive& Ar, FShaderCodeValidationStride& ShaderCodeValidationStride)
{
	return Ar << ShaderCodeValidationStride.BindPoint << ShaderCodeValidationStride.Stride;
}

inline FArchive& operator<<(FArchive& Ar, FShaderCodeValidationType& ShaderCodeValidationType)
{
	return Ar << ShaderCodeValidationType.BindPoint << ShaderCodeValidationType.Type;
}

inline FArchive& operator<<(FArchive& Ar, FShaderCodeValidationUBSize& ShaderCodeValidationSize)
{
	return Ar << ShaderCodeValidationSize.BindPoint << ShaderCodeValidationSize.Size;
}

struct FShaderCodeValidationExtension
{
	// for FindOptionalData() and AddOptionalData()
	static constexpr EShaderOptionalDataKey Key = EShaderOptionalDataKey::Validation;
	static constexpr uint16 StaticVersion = 0;

	TArray<FShaderCodeValidationStride> ShaderCodeValidationStride;
	TArray<FShaderCodeValidationType> ShaderCodeValidationSRVType;
	TArray<FShaderCodeValidationType> ShaderCodeValidationUAVType;
	TArray<FShaderCodeValidationUBSize> ShaderCodeValidationUBSize;
	uint16 Version = StaticVersion;

	friend FArchive& operator<<(FArchive& Ar, FShaderCodeValidationExtension& Extension)
	{
		Ar << Extension.Version;
		Ar << Extension.ShaderCodeValidationStride;
		Ar << Extension.ShaderCodeValidationSRVType;
		Ar << Extension.ShaderCodeValidationUAVType;
		Ar << Extension.ShaderCodeValidationUBSize;
		return Ar;
	}
};

struct FShaderDiagnosticData
{
	uint32 Hash;
	FString Message;
};

inline FArchive& operator<<(FArchive& Ar, FShaderDiagnosticData& ShaderCodeDiagnosticData)
{
	return Ar << ShaderCodeDiagnosticData.Hash << ShaderCodeDiagnosticData.Message;
}

struct FShaderDiagnosticExtension
{
	// for FindOptionalData() and AddOptionalData()
	static constexpr EShaderOptionalDataKey Key = EShaderOptionalDataKey::Diagnostic;
	static constexpr uint16 StaticVersion = 0;

	TArray<FShaderDiagnosticData> ShaderDiagnosticDatas;
	uint16 Version = StaticVersion;

	friend FArchive& operator<<(FArchive& Ar, FShaderDiagnosticExtension& Extension)
	{
		Ar << Extension.Version;
		Ar << Extension.ShaderDiagnosticDatas;
		return Ar;
	}
};

#ifndef RENDERCORE_ATTRIBUTE_UNALIGNED
// TODO find out if using GCC_ALIGN(1) instead of this new #define break on all kinds of platforms...
#define RENDERCORE_ATTRIBUTE_UNALIGNED
#endif
typedef int32  RENDERCORE_ATTRIBUTE_UNALIGNED unaligned_int32;
typedef uint32 RENDERCORE_ATTRIBUTE_UNALIGNED unaligned_uint32;

// later we can transform that to the actual class passed around at the RHI level
class FShaderCodeReader
{
	TArrayView<const uint8> ShaderCode;

public:
	FShaderCodeReader(TArrayView<const uint8> InShaderCode)
		: ShaderCode(InShaderCode)
	{
		check(ShaderCode.Num());
	}

	uint32 GetActualShaderCodeSize() const
	{
		return ShaderCode.Num() - GetOptionalDataSize();
	}

	TArrayView<const uint8> GetOffsetShaderCode(int32 Offset)
	{
		return MakeArrayView(ShaderCode.GetData() + Offset, GetActualShaderCodeSize() - Offset);
	}

	// for convenience
	template <class T>
	const T* FindOptionalData() const
	{
		return (const T*)FindOptionalData(T::Key, sizeof(T));
	}


	// @param InKey e.g. FShaderCodePackedResourceCounts::Key
	// @return 0 if not found
	const uint8* FindOptionalData(EShaderOptionalDataKey InKey, uint8 ValueSize) const
	{
		check(ValueSize);

		const uint8* End = &ShaderCode[0] + ShaderCode.Num();

		int32 LocalOptionalDataSize = GetOptionalDataSize();

		const uint8* Start = End - LocalOptionalDataSize;
		// while searching don't include the optional data size
		End = End - sizeof(LocalOptionalDataSize);
		const uint8* Current = Start;

		while(Current < End)
		{
			EShaderOptionalDataKey Key = EShaderOptionalDataKey(*Current++);
			uint32 Size = *((const unaligned_uint32*)Current);
			Current += sizeof(Size);

			if(Key == InKey && Size == ValueSize)
			{
				return Current;
			}

			Current += Size;
		}

		return 0;
	}

	const ANSICHAR* FindOptionalData(EShaderOptionalDataKey InKey) const
	{
		check(ShaderCode.Num() >= 4);

		const uint8* End = &ShaderCode[0] + ShaderCode.Num();

		int32 LocalOptionalDataSize = GetOptionalDataSize();

		const uint8* Start = End - LocalOptionalDataSize;
		// while searching don't include the optional data size
		End = End - sizeof(LocalOptionalDataSize);
		const uint8* Current = Start;

		while(Current < End)
		{
			EShaderOptionalDataKey Key = EShaderOptionalDataKey(*Current++);
			uint32 Size = *((const unaligned_uint32*)Current);
			Current += sizeof(Size);

			if(Key == InKey)
			{
				return (ANSICHAR*)Current;
			}

			Current += Size;
		}

		return 0;
	}

	// Returns nullptr and Size -1 if key was not found
	const uint8* FindOptionalDataAndSize(EShaderOptionalDataKey InKey, int32& OutSize) const
	{
		check(ShaderCode.Num() >= 4);

		const uint8* End = &ShaderCode[0] + ShaderCode.Num();

		int32 LocalOptionalDataSize = GetOptionalDataSize();

		const uint8* Start = End - LocalOptionalDataSize;
		// while searching don't include the optional data size
		End = End - sizeof(LocalOptionalDataSize);
		const uint8* Current = Start;

		while (Current < End)
		{
			EShaderOptionalDataKey Key = EShaderOptionalDataKey(*Current++);
			uint32 Size = *((const unaligned_uint32*)Current);
			Current += sizeof(Size);

			if (Key == InKey)
			{
				OutSize = Size;
				return Current;
			}

			Current += Size;
		}

		OutSize = -1;
		return nullptr;
	}

	int32 GetOptionalDataSize() const
	{
		if(ShaderCode.Num() < sizeof(int32))
		{
			return 0;
		}

		const uint8* End = &ShaderCode[0] + ShaderCode.Num();

		int32 LocalOptionalDataSize = ((const unaligned_int32*)End)[-1];

		check(LocalOptionalDataSize >= 0);
		check(ShaderCode.Num() >= LocalOptionalDataSize);

		return LocalOptionalDataSize;
	}

	int32 GetShaderCodeSize() const
	{
		return ShaderCode.Num() - GetOptionalDataSize();
	}
};

class FShaderCode
{
	// -1 if ShaderData was finalized
	mutable int32 OptionalDataSize;
	// access through class methods
	mutable TArray<uint8> ShaderCodeWithOptionalData;

	/** ShaderCode may be compressed in SCWs on demand. If this value isn't null, the shader code is compressed. */
	mutable int32 UncompressedSize;

	/** Compression algo */
	mutable FName CompressionFormat;

	/** Oodle-specific compression algorithm - used if CompressionFormat is set to NAME_Oodle. */
	FOodleDataCompression::ECompressor OodleCompressor;

	/** Oodle-specific compression level - used if CompressionFormat is set to NAME_Oodle. */
	FOodleDataCompression::ECompressionLevel OodleLevel;

	/** We cannot get the code size after the compression, so store it here */
	mutable int32 ShaderCodeSize;

public:

	FShaderCode()
	: OptionalDataSize(0)
	, UncompressedSize(0)
	, CompressionFormat(NAME_None)
	, OodleCompressor(FOodleDataCompression::ECompressor::NotSet)
	, OodleLevel(FOodleDataCompression::ECompressionLevel::None)
	, ShaderCodeSize(0)
	{
	}

	// adds CustomData or does nothing if that was already done before
	void FinalizeShaderCode() const
	{
		if(OptionalDataSize != -1)
		{
			checkf(UncompressedSize == 0, TEXT("FShaderCode::FinalizeShaderCode() was called after compressing the code"));
			OptionalDataSize += sizeof(OptionalDataSize);
			ShaderCodeWithOptionalData.Append((const uint8*)&OptionalDataSize, sizeof(OptionalDataSize));
			OptionalDataSize = -1;
		}
	}

	void Compress(FName ShaderCompressionFormat, FOodleDataCompression::ECompressor InOodleCompressor, FOodleDataCompression::ECompressionLevel InOodleLevel);

	// Write access for regular microcode: Optional Data must be added AFTER regular microcode and BEFORE Finalize
	TArray<uint8>& GetWriteAccess()
	{
		checkf(OptionalDataSize != -1, TEXT("Tried to add ShaderCode after being finalized!"));
		checkf(OptionalDataSize == 0, TEXT("Tried to add ShaderCode after adding Optional data!"));
		return ShaderCodeWithOptionalData;
	}

	int32 GetShaderCodeSize() const
	{
		// use the cached size whenever available
		if (ShaderCodeSize != 0)
		{
			return ShaderCodeSize;
		}
		else
		{
			FinalizeShaderCode();

			FShaderCodeReader Wrapper(ShaderCodeWithOptionalData);
			return Wrapper.GetShaderCodeSize();
		}
	}

	// for read access, can have additional data attached to the end. Can also be compressed
	const TArray<uint8>& GetReadAccess() const
	{
		FinalizeShaderCode();

		return ShaderCodeWithOptionalData;
	}

	bool IsCompressed() const
	{
		return UncompressedSize != 0;
	}

	FName GetCompressionFormat() const
	{
		return CompressionFormat;
	}

	FOodleDataCompression::ECompressor GetOodleCompressor() const
	{
		return OodleCompressor;
	}

	FOodleDataCompression::ECompressionLevel GetOodleLevel() const
	{
		return OodleLevel;
	}

	int32 GetUncompressedSize() const
	{
		return UncompressedSize;
	}

	// for convenience
	template <class T>
	void AddOptionalData(const T &In)
	{
		AddOptionalData(T::Key, (uint8*)&In, sizeof(T));
	}

	// Note: we don't hash the optional attachments in GenerateOutputHash() as they would prevent sharing (e.g. many material share the save VS)
	// can be called after the non optional data was stored in ShaderData
	// @param Key uint8 to save memory so max 255, e.g. FShaderCodePackedResourceCounts::Key
	// @param Size >0, only restriction is that sum of all optional data values must be < 4GB
	void AddOptionalData(EShaderOptionalDataKey Key, const uint8* ValuePtr, uint32 ValueSize)
	{
		check(ValuePtr);

		// don't add after Finalize happened
		check(OptionalDataSize >= 0);

		ShaderCodeWithOptionalData.Add(uint8(Key));
		ShaderCodeWithOptionalData.Append((const uint8*)&ValueSize, sizeof(ValueSize));
		ShaderCodeWithOptionalData.Append(ValuePtr, ValueSize);
		OptionalDataSize += sizeof(uint8) + sizeof(ValueSize) + (uint32)ValueSize;
	}

	// Note: we don't hash the optional attachments in GenerateOutputHash() as they would prevent sharing (e.g. many material share the save VS)
	// convenience, silently drops the data if string is too long
	// @param e.g. 'n' for the ShaderSourceFileName
	void AddOptionalData(EShaderOptionalDataKey Key, const ANSICHAR* InString)
	{
		uint32 Size = FCStringAnsi::Strlen(InString) + 1;
		AddOptionalData(Key, (uint8*)InString, Size);
	}

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FShaderCode& Output);
};

/**
* Convert the virtual shader path to an actual file system path.
* CompileErrors output array is optional.
*/
extern RENDERCORE_API FString GetShaderSourceFilePath(const FString& VirtualFilePath, TArray<struct FShaderCompilerError>* CompileErrors = nullptr);

/**
 * Converts an absolute or relative shader filename to a filename relative to
 * the shader directory.
 * @param InFilename - The shader filename.
 * @returns a filename relative to the shaders directory.
 */
extern RENDERCORE_API FString ParseVirtualShaderFilename(const FString& InFilename);

/** Replaces virtual platform path with appropriate path for a given ShaderPlatform. Returns true if path was changed. */
extern RENDERCORE_API bool ReplaceVirtualFilePathForShaderPlatform(FString& InOutVirtualFilePath, EShaderPlatform ShaderPlatform);

/** Replaces virtual platform path with appropriate autogen path for a given ShaderPlatform. Returns true if path was changed. */
extern RENDERCORE_API bool ReplaceVirtualFilePathForShaderAutogen(FString& InOutVirtualFilePath, EShaderPlatform ShaderPlatform, const FName* InShaderPlatformName = nullptr);

/** Loads the shader file with the given name.  If the shader file couldn't be loaded, throws a fatal error. */
extern RENDERCORE_API void LoadShaderSourceFileChecked(const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform, FString& OutFileContents, const FName* ShaderPlatformName = nullptr);

/**
 * Recursively populates IncludeFilenames with the include filenames from Filename
 */
extern RENDERCORE_API void GetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit=100, const FName* ShaderPlatformName = nullptr);
extern RENDERCORE_API void GetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, const FString& FileContents, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit = 100, const FName* ShaderPlatformName = nullptr);

/**
 * Calculates a Hash for the given filename if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 * @param ShaderPlatform - shader platform to Hash
 */
extern RENDERCORE_API const class FSHAHash& GetShaderFileHash(const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform);

/**
 * Calculates a hash for the given source file and all files included from it.
 * @param HashingArchive - hash to update
 * @param VirtualFilePath - name of this source code path (won't be loaded, as it is expected to be generated)
 * @param FileContents - shader source code to Hash (included files will be hashed, too)
 * @param ShaderPlatform - shader platform to Hash
 * @param bOnlyHashIncludedFiles - skip hashing contents of the file itself (useful if it was already hashed outside of this function)
 */
extern RENDERCORE_API void HashShaderFileWithIncludes(FArchive& HashingArchive, const TCHAR* VirtualFilePath, const FString& FileContents, EShaderPlatform ShaderPlatform, bool bOnlyHashIncludedFiles);

/**
 * Calculates a Hash for the list of filenames if it does not already exist in the Hash cache.
 */
extern RENDERCORE_API const class FSHAHash& GetShaderFilesHash(const TArray<FString>& VirtualFilePaths, EShaderPlatform ShaderPlatform);

/**
 * Flushes the shader file and CRC cache, and regenerates the binary shader files if necessary.
 * Allows shader source files to be re-read properly even if they've been modified since startup.
 */
extern RENDERCORE_API void FlushShaderFileCache();

extern RENDERCORE_API void VerifyShaderSourceFiles(EShaderPlatform ShaderPlatform);

#if WITH_EDITOR

class FShaderType;
class FVertexFactoryType;
class FShaderPipelineType;

// Text to use as line terminator for HLSL files (may differ from platform LINE_TERMINATOR)
#define HLSL_LINE_TERMINATOR TEXT("\n")

/** Force updates each shader/pipeline type provided to update their list of referenced uniform buffers. */
RENDERCORE_API void UpdateReferencedUniformBufferNames(
	TArrayView<const FShaderType*> OutdatedShaderTypes,
	TArrayView<const FVertexFactoryType*> OutdatedFactoryTypes,
	TArrayView<const FShaderPipelineType*> OutdatedShaderPipelineTypes);

/** Parses the given source file and its includes for references of uniform buffers. */
extern void GenerateReferencedUniformBufferNames(
	const TCHAR* SourceFilename,
	const TCHAR* ShaderTypeName,
	const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables,
	TSet<const TCHAR*, TStringPointerSetKeyFuncs_DEPRECATED<const TCHAR*>>& UniformBufferNames);

struct FUniformBufferNameSortOrder
{
	FORCEINLINE bool operator()(const TCHAR* Name1, const TCHAR* Name2) const
	{
		return FCString::Strcmp(Name1, Name2) < 0;
	}
};

/**
 * Return the hash of the given type layout for a partical platform type layout. This function employs caching to avoid re-hashing the same parameters several times.
 */
extern RENDERCORE_API FSHAHash GetShaderTypeLayoutHash(const FTypeLayoutDesc& TypeDesc, FPlatformTypeLayoutParameters LayoutParameters);

// Forward declarations
class FShaderTypeDependency;
class FShaderPipelineTypeDependency;
class FVertexFactoryTypeDependency;

/** Appends information to a KeyString for a given shader to reflect its dependencies */
extern RENDERCORE_API void AppendKeyStringShaderDependencies(
	TConstArrayView<FShaderTypeDependency> ShaderTypeDependencies,
	FPlatformTypeLayoutParameters LayoutParams,
	FString& OutKeyString,
	bool bIncludeSourceHashes = true);

extern RENDERCORE_API void AppendKeyStringShaderDependencies(
	TConstArrayView<FShaderTypeDependency> ShaderTypeDependencies,
	TConstArrayView<FShaderPipelineTypeDependency> ShaderPipelineTypeDependencies,
	TConstArrayView<FVertexFactoryTypeDependency> VertexFactoryTypeDependencies,
	FPlatformTypeLayoutParameters LayoutParams,
	FString& OutKeyString,
	bool bIncludeSourceHashes = true);
#endif // WITH_EDITOR

/** Create a block of source code to be injected in the preprocessed shader code. The Block will be put into a #line directive
 * to show up in case shader compilation failures happen in this code block.
 */
FString RENDERCORE_API MakeInjectedShaderCodeBlock(const TCHAR* BlockName, const FString& CodeToInject);


/**
 * Returns the map virtual shader directory path -> real shader directory path.
 */
extern RENDERCORE_API const TMap<FString, FString>& AllShaderSourceDirectoryMappings();

/** Hook for shader compile worker to reset the directory mappings. */
extern RENDERCORE_API void ResetAllShaderSourceDirectoryMappings();

/**
 * Maps a real shader directory existing on disk to a virtual shader directory.
 * @param VirtualShaderDirectory Unique absolute path of the virtual shader directory (ex: /Project).
 * @param RealShaderDirectory FPlatformProcess::BaseDir() relative path of the directory map.
 */
extern RENDERCORE_API void AddShaderSourceDirectoryMapping(const FString& VirtualShaderDirectory, const FString& RealShaderDirectory);

extern RENDERCORE_API void AddShaderSourceFileEntry(TArray<FString>& OutVirtualFilePaths, FString VirtualFilePath, EShaderPlatform ShaderPlatform, const FName* ShaderPlatformName = nullptr);
extern RENDERCORE_API void GetAllVirtualShaderSourcePaths(TArray<FString>& OutVirtualFilePaths, EShaderPlatform ShaderPlatform, const FName* ShaderPlatformName = nullptr);
