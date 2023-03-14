// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCore.h: Shader core module definitions.
=============================================================================*/

#pragma once

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
#include "PixelFormat.h"
#include "RHIDefinitions.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryLayout.h"
#include "ShaderParameterMetadata.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "UniformBuffer.h"

class Error;
class FMemoryImageWriter;
class FMemoryUnfreezeContent;
class FPointerTableBase;
class FSHA1;
class ITargetPlatform;

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
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("HLSL Translation"),STAT_ShaderCompiling_HLSLTranslation,STATGROUP_ShaderCompiling, RENDERCORE_API);
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
extern RENDERCORE_API void CheckShaderHashCacheInclude(const FString& VirtualFilePath, EShaderPlatform ShaderPlatform);

/** Initializes cached shader type data.  This must be called before creating any FShaderType. */
extern RENDERCORE_API void InitializeShaderTypes();

/** Uninitializes cached shader type data.  This is needed before unloading modules that contain FShaderTypes. */
extern RENDERCORE_API void UninitializeShaderTypes();

/** Returns true if debug viewmodes are allowed for the current platform. */
extern RENDERCORE_API bool AllowDebugViewmodes();

/** Returns true if debug viewmodes are allowed for the given platform. */
extern RENDERCORE_API bool AllowDebugViewmodes(EShaderPlatform Platform);

/** Returns the shader compression format (passing ShaderFormat for future proofing, but as of now the setting is global for all formats). */
extern RENDERCORE_API FName GetShaderCompressionFormat(const FName& ShaderFormat = NAME_None);

namespace FOodleDataCompression
{
	enum class ECompressor : uint8;
	enum class ECompressionLevel : int8;
}

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

	BindlessResourceIndex,
	BindlessSamplerIndex,

	Num
};

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
};

inline bool operator==(const FParameterAllocation& A, const FParameterAllocation& B)
{
	return
		A.BufferIndex == B.BufferIndex && A.BaseIndex == B.BaseIndex && A.Size == B.Size && A.Type == B.Type && A.bBound == B.bBound;
}

inline bool operator!=(const FParameterAllocation& A, const FParameterAllocation& B)
{
	return !(A == B);
}

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

/** Container for shader compiler definitions. */
class FShaderCompilerDefinitions
{
public:

	FShaderCompilerDefinitions()
	{
		// Presize to reduce re-hashing while building shader jobs
		Definitions.Empty(50);
	}

	/**
	 * Works for TCHAR
	 * e.g. SetDefine(TEXT("NUM_SAMPLES"), TEXT("1"));
	 */
	void SetDefine(const TCHAR* Name, const TCHAR* Value)
	{
		Definitions.Add(Name, Value);
	}

	void SetDefine(const TCHAR* Name, const FString& Value)
	{
		Definitions.Add(Name, Value);
	}

	void SetDefine(const TCHAR* Name, bool Value)
	{
		Definitions.Add(Name, Value ? TEXT("1") : TEXT("0"));
	}

	void SetDefine(const TCHAR* Name, uint32 Value)
	{
		// can be optimized
		switch (Value)
		{
		// Avoid Printf for common cases
		case 0u: Definitions.Add(Name, TEXT("0")); break;
		case 1u: Definitions.Add(Name, TEXT("1")); break;
		default: Definitions.Add(Name, FString::Printf(TEXT("%u"), Value)); break;
		}
	}

	void SetDefine(const TCHAR* Name, int32 Value)
	{
		// can be optimized
		switch (Value)
		{
		case 0: Definitions.Add(Name, TEXT("0")); break;
		case 1: Definitions.Add(Name, TEXT("1")); break;
		default: Definitions.Add(Name, FString::Printf(TEXT("%d"), Value));
		}
	}

	/**
	 * Works for float
	 */
	RENDERCORE_API void SetFloatDefine(const TCHAR* Name, float Value);

	const TMap<FString,FString>& GetDefinitionMap() const
	{
		return Definitions;
	}

	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerDefinitions& Defs)
	{
		return Ar << Defs.Definitions;
	}

	void Merge(const FShaderCompilerDefinitions& Other)
	{
		Definitions.Append(Other.Definitions);
	}

private:

	/** Map: definition -> value. */
	TMap<FString,FString> Definitions;
};

struct FBaseShaderResourceTable
{
	/** Bits indicating which resource tables contain resources bound to this shader. */
	uint32 ResourceTableBits;

	/** Mapping of bound SRVs to their location in resource tables. */
	TArray<uint32> ShaderResourceViewMap;

	/** Mapping of bound sampler states to their location in resource tables. */
	TArray<uint32> SamplerMap;

	/** Mapping of bound UAVs to their location in resource tables. */
	TArray<uint32> UnorderedAccessViewMap;

	/** Hash of the layouts of resource tables at compile time, used for runtime validation. */
	TArray<uint32> ResourceTableLayoutHashes;

	FBaseShaderResourceTable() :
		ResourceTableBits(0)
	{
	}

	friend bool operator==(const FBaseShaderResourceTable &A, const FBaseShaderResourceTable& B)
	{
		bool bEqual = true;
		bEqual &= (A.ResourceTableBits == B.ResourceTableBits);
		bEqual &= (A.ShaderResourceViewMap.Num() == B.ShaderResourceViewMap.Num());
		bEqual &= (A.SamplerMap.Num() == B.SamplerMap.Num());
		bEqual &= (A.UnorderedAccessViewMap.Num() == B.UnorderedAccessViewMap.Num());
		bEqual &= (A.ResourceTableLayoutHashes.Num() == B.ResourceTableLayoutHashes.Num());
		if (!bEqual)
		{
			return false;
		}
		bEqual &= (FMemory::Memcmp(A.ShaderResourceViewMap.GetData(), B.ShaderResourceViewMap.GetData(), A.ShaderResourceViewMap.GetTypeSize()*A.ShaderResourceViewMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.SamplerMap.GetData(), B.SamplerMap.GetData(), A.SamplerMap.GetTypeSize()*A.SamplerMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.UnorderedAccessViewMap.GetData(), B.UnorderedAccessViewMap.GetData(), A.UnorderedAccessViewMap.GetTypeSize()*A.UnorderedAccessViewMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.ResourceTableLayoutHashes.GetData(), B.ResourceTableLayoutHashes.GetData(), A.ResourceTableLayoutHashes.GetTypeSize()*A.ResourceTableLayoutHashes.Num()) == 0);
		return bEqual;
	}
};

inline FArchive& operator<<(FArchive& Ar, FBaseShaderResourceTable& SRT)
{
	Ar << SRT.ResourceTableBits;
	Ar << SRT.ShaderResourceViewMap;
	Ar << SRT.SamplerMap;
	Ar << SRT.UnorderedAccessViewMap;
	Ar << SRT.ResourceTableLayoutHashes;

	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FResourceTableEntry& Entry)
{
	Ar << Entry.UniformBufferName;
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
	return Ar;
}

using FThreadSafeSharedStringPtr = TSharedPtr<FString, ESPMode::ThreadSafe>;

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

/** The environment used to compile a shader. */
struct FShaderCompilerEnvironment
{
	// Map of the virtual file path -> content.
	// The virtual file paths are the ones that USF files query through the #include "<The Virtual Path of the file>"
	TMap<FString,FString> IncludeVirtualPathToContentsMap;
	
	TMap<FString, FThreadSafeSharedStringPtr> IncludeVirtualPathToExternalContentsMap;

	FShaderCompilerFlags CompilerFlags;
	TMap<uint32,uint8> RenderTargetOutputFormatsMap;
	TMap<FString, FResourceTableEntry> ResourceTableMap;
	TMap<FString, FUniformBufferEntry> UniformBufferMap;
	TMap<FString, FString> RemoteServerData;
	TMap<FString, FString> ShaderFormatCVars;

	const ITargetPlatform* TargetPlatform = nullptr;

	// Used for mobile platforms to allow per shader/material precision modes
	bool FullPrecisionInPS = 0;

	/** Default constructor. */
	FShaderCompilerEnvironment()
	{
		// Presize to reduce re-hashing while building shader jobs
		IncludeVirtualPathToContentsMap.Empty(15);
	}

	/** Initialization constructor. */
	explicit FShaderCompilerEnvironment(const FShaderCompilerDefinitions& InDefinitions)
		: Definitions(InDefinitions)
	{
	}

	// Used as a baseclasss, make sure we're not incorrectly destroyed through a baseclass pointer
	// This will be expensive to destroy anyway, additional vcall overhead should be small
	virtual ~FShaderCompilerEnvironment() = default;

	/**
	 * Works for TCHAR
	 * e.g. SetDefine(TEXT("NAME"), TEXT("Test"));
	 * e.g. SetDefine(TEXT("NUM_SAMPLES"), 1);
	 * e.g. SetDefine(TEXT("DOIT"), true);
	 */
	void SetDefine(const TCHAR* Name, const TCHAR* Value)	{ Definitions.SetDefine(Name, Value); }
	void SetDefine(const TCHAR* Name, const FString& Value) { Definitions.SetDefine(Name, Value); }
	void SetDefine(const TCHAR* Name, uint32 Value)			{ Definitions.SetDefine(Name, Value); }
	void SetDefine(const TCHAR* Name, int32 Value)			{ Definitions.SetDefine(Name, Value); }
	void SetDefine(const TCHAR* Name, bool Value)			{ Definitions.SetDefine(Name, Value); }
	void SetDefine(const TCHAR* Name, float Value)			{ Definitions.SetFloatDefine(Name, Value); }

	const TMap<FString,FString>& GetDefinitions() const
	{
		return Definitions.GetDefinitionMap();
	}

	void SetRenderTargetOutputFormat(uint32 RenderTargetIndex, EPixelFormat PixelFormat)
	{
		RenderTargetOutputFormatsMap.Add(RenderTargetIndex, UE_PIXELFORMAT_TO_UINT8(PixelFormat));
	}

	/** This "core" serialization is also used for the hashing the compiler job (where files are handled differently). Should stay in sync with the ShaderCompileWorker. */
	inline void SerializeEverythingButFiles(FArchive& Ar)
	{
		Ar << Definitions;
		Ar << CompilerFlags;
		Ar << RenderTargetOutputFormatsMap;
		Ar << ResourceTableMap;
		Ar << UniformBufferMap;
		Ar << RemoteServerData;
		Ar << ShaderFormatCVars;
		Ar << FullPrecisionInPS;
	}

	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerEnvironment& Environment)
	{
		// Note: this serialize is used to pass between UE and the shader compile worker, recompile both when modifying
		Ar << Environment.IncludeVirtualPathToContentsMap;

		// Note: skipping Environment.IncludeVirtualPathToExternalContentsMap, which is handled by FShaderCompileUtilities::DoWriteTasks in order to maintain sharing

		Environment.SerializeEverythingButFiles(Ar);
		return Ar;
	}
	
	void Merge(const FShaderCompilerEnvironment& Other)
	{
		// Merge the include maps
		// Merge the values of any existing keys
		for (TMap<FString,FString>::TConstIterator It(Other.IncludeVirtualPathToContentsMap); It; ++It )
		{
			FString* ExistingContents = IncludeVirtualPathToContentsMap.Find(It.Key());

			if (ExistingContents)
			{
				ExistingContents->Append(It.Value());
			}
			else
			{
				IncludeVirtualPathToContentsMap.Add(It.Key(), It.Value());
			}
		}

		check(Other.IncludeVirtualPathToExternalContentsMap.Num() == 0);

		CompilerFlags.Append(Other.CompilerFlags);
		ResourceTableMap.Append(Other.ResourceTableMap);
		UniformBufferMap.Append(Other.UniformBufferMap);
		Definitions.Merge(Other.Definitions);
		RenderTargetOutputFormatsMap.Append(Other.RenderTargetOutputFormatsMap);
		RemoteServerData.Append(Other.RemoteServerData);
		ShaderFormatCVars.Append(Other.ShaderFormatCVars);
		FullPrecisionInPS |= Other.FullPrecisionInPS;
	}

private:

	FShaderCompilerDefinitions Definitions;
};

struct FSharedShaderCompilerEnvironment final : public FShaderCompilerEnvironment, public FRefCountBase
{
	virtual ~FSharedShaderCompilerEnvironment() = default;
};

enum class EShaderResourceUsageFlags : uint8
{
	GlobalUniformBuffer = 1 << 0,
	BindlessResources   = 1 << 1,
	BindlessSamplers    = 1 << 2,
};
ENUM_CLASS_FLAGS(EShaderResourceUsageFlags)

// if this changes you need to make sure all shaders get invalidated
struct FShaderCodePackedResourceCounts
{
	// for FindOptionalData() and AddOptionalData()
	static const uint8 Key = 'p';

	EShaderResourceUsageFlags UsageFlags;
	uint8 NumSamplers;
	uint8 NumSRVs;
	uint8 NumCBs;
	uint8 NumUAVs;
};

struct FShaderCodeResourceMasks
{
	// for FindOptionalData() and AddOptionalData()
	static const uint8 Key = 'm';

	uint32 UAVMask; // Mask of UAVs bound
};

// if this changes you need to make sure all shaders get invalidated
enum class EShaderCodeFeatures : uint8
{
	None                    = 0,
	WaveOps                 = 1 << 0,
	SixteenBitTypes         = 1 << 1,
	TypedUAVLoadsExtended   = 1 << 2,
	Atomic64                = 1 << 3,
	DiagnosticBuffer        = 1 << 4,
	BindlessResources       = 1 << 5,
	BindlessSamplers        = 1 << 6,
};
ENUM_CLASS_FLAGS(EShaderCodeFeatures);

struct FShaderCodeFeatures
{
	// for FindOptionalData() and AddOptionalData()
	static const uint8 Key = 'x';

	EShaderCodeFeatures CodeFeatures = EShaderCodeFeatures::None;
};

// if this changes you need to make sure all shaders get invalidated
struct FShaderCodeName
{
	static const uint8 Key = 'n';

	// We store the straight ANSICHAR zero-terminated string
};

struct FShaderCodeUniformBuffers
{
	static const uint8 Key = 'u';
	// We store an array of FString objects
};

// if this changes you need to make sure all shaders get invalidated
struct FShaderCodeVendorExtension
{
	// for FindOptionalData() and AddOptionalData()
	static const uint8 Key = 'v';

	uint32 VendorId = 0;
	FParameterAllocation Parameter;

	FShaderCodeVendorExtension() = default;
	FShaderCodeVendorExtension(uint32 InVendorId, uint16 InBufferIndex, uint16 InBaseIndex, uint16 InSize, EShaderParameterType InType)
		: VendorId(InVendorId)
		, Parameter(InBufferIndex, InBaseIndex, InSize, InType)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FShaderCodeVendorExtension& Extension)
	{
		return Ar << Extension.VendorId << Extension.Parameter;
	}
};

inline bool operator==(const FShaderCodeVendorExtension& A, const FShaderCodeVendorExtension& B)
{
	return A.VendorId == B.VendorId && A.Parameter == B.Parameter;
}

inline bool operator!=(const FShaderCodeVendorExtension& A, const FShaderCodeVendorExtension& B)
{
	return !(A == B);
}

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
	const uint8* FindOptionalData(uint8 InKey, uint8 ValueSize) const
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
			uint8 Key = *Current++;
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

	const ANSICHAR* FindOptionalData(uint8 InKey) const
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
			uint8 Key = *Current++;
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
	const uint8* FindOptionalDataAndSize(uint8 InKey, int32& OutSize) const
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
			uint8 Key = *Current++;
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
	void AddOptionalData(uint8 Key, const uint8* ValuePtr, uint32 ValueSize)
	{
		check(ValuePtr);

		// don't add after Finalize happened
		check(OptionalDataSize >= 0);

		ShaderCodeWithOptionalData.Add(Key);
		ShaderCodeWithOptionalData.Append((const uint8*)&ValueSize, sizeof(ValueSize));
		ShaderCodeWithOptionalData.Append(ValuePtr, ValueSize);
		OptionalDataSize += sizeof(uint8) + sizeof(ValueSize) + (uint32)ValueSize;
	}

	// Note: we don't hash the optional attachments in GenerateOutputHash() as they would prevent sharing (e.g. many material share the save VS)
	// convenience, silently drops the data if string is too long
	// @param e.g. 'n' for the ShaderSourceFileName
	void AddOptionalData(uint8 Key, const ANSICHAR* InString)
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

extern void BuildShaderFileToUniformBufferMap(TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables, const FName* ShaderPlatformName = nullptr);

/**
 * Flushes the shader file and CRC cache, and regenerates the binary shader files if necessary.
 * Allows shader source files to be re-read properly even if they've been modified since startup.
 */
extern RENDERCORE_API void FlushShaderFileCache(const FName* ShaderPlatformName = nullptr);

extern RENDERCORE_API void VerifyShaderSourceFiles(EShaderPlatform ShaderPlatform);

struct FCachedUniformBufferDeclaration
{
	// Using SharedPtr so we can hand off lifetime ownership to FShaderCompilerEnvironment::IncludeVirtualPathToExternalContentsMap when invalidating this cache
	FThreadSafeSharedStringPtr Declaration;
};

/** Parses the given source file and its includes for references of uniform buffers, which are then stored in UniformBufferEntries. */
extern void GenerateReferencedUniformBuffers(
	const TCHAR* SourceFilename,
	const TCHAR* ShaderTypeName,
	const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables,
	TMap<const TCHAR*,FCachedUniformBufferDeclaration>& UniformBufferEntries);

struct FUniformBufferNameSortOrder
{
	FORCEINLINE bool operator()(const TCHAR* Name1, const TCHAR* Name2) const
	{
		return FCString::Strcmp(Name1, Name2) < 0;
	}
};

/** Records information about all the uniform buffer layouts referenced by UniformBufferEntries. */
extern RENDERCORE_API void SerializeUniformBufferInfo(class FShaderSaveArchive& Ar, const TSortedMap<const TCHAR*,FCachedUniformBufferDeclaration, FDefaultAllocator, FUniformBufferNameSortOrder>& UniformBufferEntries);

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
