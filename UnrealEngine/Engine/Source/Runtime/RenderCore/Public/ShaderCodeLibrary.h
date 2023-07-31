// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCodeLibrary.h: 
=============================================================================*/

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Logging/LogMacros.h"
#include "Misc/Crc.h"
#include "Misc/SecureHash.h"
#include "RHI.h"
#include "RHIDefinitions.h"
#include "Serialization/Archive.h"
#include "Templates/RefCounting.h"
#include "UObject/NameTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogShaderLibrary, Log, All);

class FShaderMapResource;
class FShaderMapResourceCode;
class FShaderPipeline;

using FShaderMapAssetPaths = TSet<FName>;
class FIoBuffer;
class FIoChunkId;
class UObject;

#if WITH_EDITOR
class FCbFieldView;
class FCbWriter;
#endif

struct RENDERCORE_API FShaderCodeLibraryPipeline
{
	FSHAHash Shaders[SF_NumGraphicsFrequencies];
	mutable uint32 Hash;
	
	/** Fills the hashes from the pipeline stage shaders */
	void Initialize(const FShaderPipeline* Pipeline);

	FShaderCodeLibraryPipeline() : Hash(0) {}
	
	friend bool operator ==(const FShaderCodeLibraryPipeline& A,const FShaderCodeLibraryPipeline& B)
	{
		for (uint32 Frequency = 0u; Frequency < SF_NumGraphicsFrequencies; ++Frequency)
		{
			if (A.Shaders[Frequency] != B.Shaders[Frequency])
			{
				return false;
			}
		}
		return true;
	}
	
	friend uint32 GetTypeHash(const FShaderCodeLibraryPipeline &Key)
	{
		if(!Key.Hash)
		{
			for (uint32 Frequency = 0u; Frequency < SF_NumGraphicsFrequencies; ++Frequency)
			{
				Key.Hash = FCrc::MemCrc32(Key.Shaders[Frequency].Hash, sizeof(FSHAHash), Key.Hash);
			}
		}
		return Key.Hash;
	}

	/** Computes a longer hash that uniquely identifies the whole pipeline, used in FStableShaderKeyAndValue */
	void GetPipelineHash(FSHAHash& Output);
	
	friend FArchive& operator<<( FArchive& Ar, FShaderCodeLibraryPipeline& Info )
	{
		for (uint32 Frequency = 0u; Frequency < SF_NumGraphicsFrequencies; ++Frequency)
		{
			Ar << Info.Shaders[Frequency];
		}
		return Ar << Info.Hash;
	}
};

struct RENDERCORE_API FCompactFullName
{
	TArray<FName, TInlineAllocator<16>> ObjectClassAndPath;

	bool operator==(const FCompactFullName& Other) const
	{
		return ObjectClassAndPath == Other.ObjectClassAndPath;
	}

	FString ToString() const;
	FString ToStringPathOnly() const;
	void AppendString(FStringBuilderBase& Out) const;
	void AppendString(FAnsiStringBuilderBase& Out) const;
	void ParseFromString(const FStringView& Src);
	friend RENDERCORE_API uint32 GetTypeHash(const FCompactFullName& A);

#if WITH_EDITOR
	/** Used to set up some compact FName paths for the FCompactFullName */
	void SetCompactFullNameFromObject(UObject* InDepObject);
#endif
};


struct RENDERCORE_API FStableShaderKeyAndValue
{
	FCompactFullName ClassNameAndObjectPath;
	FName ShaderType;
	FName ShaderClass;
	FName MaterialDomain;
	FName FeatureLevel;
	FName QualityLevel;
	FName TargetFrequency;
	FName TargetPlatform;
	FName VFType;
	FName PermutationId;
	FSHAHash PipelineHash;

	uint32 KeyHash;

	FSHAHash OutputHash;

	FStableShaderKeyAndValue()
		: KeyHash(0)
	{
	}

	void ComputeKeyHash();
	void ParseFromString(const FStringView& Src);
	void ParseFromStringCached(const FStringView& Src, class TMap<uint32, FName>& NameCache);
	FString ToString() const;
	void ToString(FString& OutResult) const;
	void AppendString(FAnsiStringBuilderBase& Out) const;
	static FString HeaderLine();

	/** Computes pipeline hash from the passed pipeline. Pass nullptr to clear */
	void SetPipelineHash(const FShaderPipeline* Pipeline);

	friend bool operator ==(const FStableShaderKeyAndValue& A, const FStableShaderKeyAndValue& B)
	{
		return
			A.ClassNameAndObjectPath == B.ClassNameAndObjectPath &&
			A.ShaderType == B.ShaderType &&
			A.ShaderClass == B.ShaderClass &&
			A.MaterialDomain == B.MaterialDomain &&
			A.FeatureLevel == B.FeatureLevel &&
			A.QualityLevel == B.QualityLevel &&
			A.TargetFrequency == B.TargetFrequency &&
			A.TargetPlatform == B.TargetPlatform &&
			A.VFType == B.VFType &&
			A.PermutationId == B.PermutationId &&
			A.PipelineHash == B.PipelineHash;
	}

	friend uint32 GetTypeHash(const FStableShaderKeyAndValue &Key)
	{
		return Key.KeyHash;
	}
};
#if WITH_EDITOR
RENDERCORE_API void WriteToCompactBinary(FCbWriter& Writer, const FStableShaderKeyAndValue& Key, 
	const TMap<FSHAHash, int32>& HashToIndex);
RENDERCORE_API bool LoadFromCompactBinary(FCbFieldView Field, FStableShaderKeyAndValue& Key,
	const TArray<FSHAHash>& IndexToHash);
#endif


DECLARE_MULTICAST_DELEGATE_TwoParams(FSharedShaderCodeRequest, const FSHAHash&, FArchive*);
DECLARE_MULTICAST_DELEGATE_OneParam(FSharedShaderCodeRelease, const FSHAHash&);

// Collection of unique shader code
// Populated at cook time
struct RENDERCORE_API FShaderCodeLibrary
{
	/** Adds the hooks for OnPakFileMounted, since pak opening normally happens earlier. */
	static void PreInit();

	/** This is the real initialization function. */
	static void InitForRuntime(EShaderPlatform ShaderPlatform);
	static void Shutdown();

	static bool IsEnabled();

	/**
	 * Makes a number of ChunkIDs known to the library.
	 * 
	 * Normally the library is tracking chunk IDs runtime itself by listening to the pak mounted notifications.
	 * However, sometimes shaderbytecode files are moved between the chunks by the packaging code can can end up being
	 * preloaded.
	 * Takes C array and not TArray because that makes easier to hardcode the chunk IDs (which is the intended use case for this function).
	 */
	static void AddKnownChunkIDs(const int32* IDs, const int32 NumChunkIDs);

	/** 
	 * Open a named library.
	 * 
	 * At runtime this will open the shader library with this name.
	 * @param Name is a high level description of the library (usually a project name or "Global")
	 * @param Directory location of the .ushadercode file
	 * @return true if successful
	 */
	static bool OpenLibrary(FString const& Name, FString const& Directory);

	/**
	 * Close a named library.
	 *
	 * At runtime this will release the library data and further requests for shaders from this library will fail.
	 */
	static void CloseLibrary(FString const& Name);

    static bool ContainsShaderCode(const FSHAHash& Hash);

	static TRefCountPtr<FShaderMapResource> LoadResource(const FSHAHash& Hash, FArchive* Ar);

	static bool PreloadShader(const FSHAHash& Hash, FArchive* Ar);
	static bool ReleasePreloadedShader(const FSHAHash& Hash);

	static FVertexShaderRHIRef CreateVertexShader(EShaderPlatform Platform, const FSHAHash& Hash);
	static FPixelShaderRHIRef CreatePixelShader(EShaderPlatform Platform, const FSHAHash& Hash);
	static FGeometryShaderRHIRef CreateGeometryShader(EShaderPlatform Platform, const FSHAHash& Hash);
	static FComputeShaderRHIRef CreateComputeShader(EShaderPlatform Platform, const FSHAHash& Hash);
	static FMeshShaderRHIRef CreateMeshShader(EShaderPlatform Platform, const FSHAHash& Hash);
	static FAmplificationShaderRHIRef CreateAmplificationShader(EShaderPlatform Platform, const FSHAHash& Hash);
	static FRayTracingShaderRHIRef CreateRayTracingShader(EShaderPlatform Platform, const FSHAHash& Hash, EShaderFrequency Frequency);

	// Total number of shader entries in the library
	static uint32 GetShaderCount(void);
	
	// The shader platform that the library manages - at runtime this will only be one
	static EShaderPlatform GetRuntimeShaderPlatform(void);

	// Safely assign the hash to a shader object
	static void SafeAssignHash(FRHIShader* InShader, const FSHAHash& Hash);

	// Delegate called whenever shader code is requested.
	static FDelegateHandle RegisterSharedShaderCodeRequestDelegate_Handle(const FSharedShaderCodeRequest::FDelegate& Delegate);
	static void UnregisterSharedShaderCodeRequestDelegate_Handle(FDelegateHandle Handle);
};

#if WITH_EDITOR
class ITargetPlatform;

struct RENDERCORE_API FShaderLibraryCooker
{
	// Initialize the library cooker
	static void InitForCooking(bool bNativeFormat);
	// Shutdown the library cooker
	static void Shutdown();

	// Clean the cook directories
	static void CleanDirectories(TArray<FName> const& ShaderFormats);

	struct FShaderFormatDescriptor
	{
		FName ShaderFormat;
		bool bNeedsStableKeys;
		bool bNeedsDeterministicOrder;
	};

	/**
	 * Opens a named library for cooking and sets it as the default.
	 *
	 * This will place all added shaders & pipelines into the library file with this name.
	 * @param Name is a high level description of the library (usually a project name or "Global")
	 * @return true if successful
	 */
	static bool BeginCookingLibrary(FString const& Name);

	/**
	 * Close a named library.
	 *  For cooking, after this point any AddShaderCode/AddShaderPipeline calls will be invalid until OpenLibrary is called again.
	 */
	static void EndCookingLibrary(FString const& Name);

	/**
	 * Whether storing shaders in the shader library is enabled
	 */
	static bool IsShaderLibraryEnabled();

	// Specify the shader formats to cook and which ones needs stable keys. Provide an array of FShaderFormatDescriptors
	static void CookShaderFormats(TArray<FShaderFormatDescriptor> const& ShaderFormats);

	// At cook time, add shader code to collection
	static bool AddShaderCode(EShaderPlatform ShaderPlatform, const FShaderMapResourceCode* Code, const FShaderMapAssetPaths& AssociatedAssets);

#if WITH_EDITOR
	/** Called from a CookWorker to send all contents of the ShaderLibrary to the CookDirector */
	static void CopyToCompactBinaryAndClear(FCbWriter& Writer, bool& bOutHasData);

	/** Called On the CookDirector to receive ShaderLibrary contents from a CookWorker */
	static bool AppendFromCompactBinary(FCbFieldView Field);
#endif

	// We check this early in the callstack to avoid creating a bunch of FName and keys and things we will never save anyway. 
	// Pass the shader platform to check or EShaderPlatform::SP_NumPlatforms to check if any of the registered types require
	// stable keys.
	static bool NeedsShaderStableKeys(EShaderPlatform ShaderPlatform);

	// At cook time, add the human readable key value information
	static void AddShaderStableKeyValue(EShaderPlatform ShaderPlatform, FStableShaderKeyAndValue& StableKeyValue);

	/** Finishes collection of data that should be in the named code library. This includes loading data from a previous iterative cook. */
	static void FinishPopulateShaderLibrary(const ITargetPlatform* TargetPlatform, FString const& Name, FString const& SandboxDestinationPath,
		FString const& SandboxMetadataPath);

	/**
	 * Saves collected shader code to a single file per shader platform
	 * When chunking is enabled, this call will not write the shader code, only the SCL.CSV file with the stable shader info.
	 * 
	 * @param TargetPlatform target platform
	 * @param Name shader library name
	 * @param SandboxDestinationPath where to put the .ushaderbytecode file(s)
	 * @param SandboxMetadataPath path for the metadata (not a part of the build itself, but produced together with the build)
	 * @param PlatformSCLCSVPaths path where to put the information about the shader hashes
	 * @param OutErrorMessage used to return the details of the failure (if failed)
	 * @param bOutHasData Reports whether any files were written to PlatformSCLCSVPaths
	 * @return true if successful
	 */
	static bool SaveShaderLibraryWithoutChunking(const ITargetPlatform* TargetPlatform, FString const& Name, FString const& SandboxDestinationPath,
		FString const& SandboxMetadataPath, TArray<FString>& PlatformSCLCSVPaths, FString& OutErrorMessage, bool& bOutHasData);

	/** 
	 * Saves a single chunk of the collected shader code (per shader platform). Does not save SCL.CSV info.
	 * This code path is only called if we're chunking.
	 * FIXME: this function does not write build metadata
	 * 
	 * @param ChunkId the chunk id
	 * @param InPackagesInChunk packages that belong to the chunk
	 * @param TargetPlatform target platform
	 * @param SandboxDestinationPath where to put the .ushaderbytecode file(s)
	 * @param OutChunkFilenames array where the function will append the full paths of the written files
	 * @param bOutHasData Reports whether any files were written to OutChunkFilenames
	 * @return true if successful
	 */
	static bool SaveShaderLibraryChunk(int32 ChunkId, const TSet<FName>& InPackagesInChunk, const ITargetPlatform* TargetPlatform,
		const FString& SandboxDestinationPath, const FString& SandboxMetadataPath, TArray<FString>& OutChunkFilenames, bool& bOutHasData);

	// Dump collected stats for each shader platform
	static void DumpShaderCodeStats();

	// Create a smaller 'patch' library that only contains data from 'NewMetaDataDir' not contained in any of 'OldMetaDataDirs'
	static bool CreatePatchLibrary(TArray<FString> const& OldMetaDataDirs, FString const& NewMetaDataDir, FString const& OutDir, bool bNativeFormat, bool bNeedsDeterministicOrder);
};
#endif
