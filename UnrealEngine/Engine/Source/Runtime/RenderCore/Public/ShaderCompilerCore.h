// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCompilerCore.h: Shader Compiler core module definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Templates/RefCounting.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"
#include "Misc/CoreStats.h"
#include "ShaderCore.h"

class Error;

// this is for the protocol, not the data, bump if FShaderCompilerInput or ProcessInputFromArchive changes.
const int32 ShaderCompileWorkerInputVersion = 17;
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes.
const int32 ShaderCompileWorkerOutputVersion = 8;
// this is for the protocol, not the data.
const int32 ShaderCompileWorkerSingleJobHeader = 'S';
// this is for the protocol, not the data.
const int32 ShaderCompileWorkerPipelineJobHeader = 'P';

/** Returns true if shader symbols should be kept for a given platform. */
extern RENDERCORE_API bool ShouldGenerateShaderSymbols(FName ShaderFormat);

/** Returns true if shader symbols should be exported to separate files for a given platform. */
extern RENDERCORE_API bool ShouldWriteShaderSymbols(FName ShaderFormat);

/** Returns true if the shader symbol path is overriden and OutPathOverride contains the override path. */
extern RENDERCORE_API bool GetShaderSymbolPathOverride(FString& OutPathOverride, FName ShaderFormat);

/** Returns true if (external) shader symbols should be specific to each shader rather than be de-duplicated. */
extern RENDERCORE_API bool ShouldAllowUniqueShaderSymbols(FName ShaderFormat);

/** Returns true if shaders should be combined into a single zip file instead of individual files. */
extern RENDERCORE_API bool ShouldWriteShaderSymbolsAsZip(FName ShaderFormat);

/** Returns true if the user wants more runtime shader data (names, extra info) */
extern RENDERCORE_API bool ShouldEnableExtraShaderData(FName ShaderFormat);

extern RENDERCORE_API bool ShouldOptimizeShaders(FName ShaderFormat);

UE_DEPRECATED(5.0, "ShouldGenerateShaderSymbols should be called to determine if symbols (debug data) should be generated")
bool ShouldKeepShaderDebugInfo(EShaderPlatform Platform);
UE_DEPRECATED(5.0, "ShouldWriteShaderSymbols should be called to determine if symbols (debug data) should be written")
bool ShouldExportShaderDebugInfo(EShaderPlatform Platform);

/** Returns true is shader compiling is allowed */
extern RENDERCORE_API bool AllowShaderCompiling();

/** Returns true if the global shader cache should be loaded (and potentially compiled if allowed/needed */
extern RENDERCORE_API bool AllowGlobalShaderLoad();


enum ECompilerFlags
{
	CFLAG_PreferFlowControl = 0,
	CFLAG_Debug,
	CFLAG_AvoidFlowControl,
	// Disable shader validation
	CFLAG_SkipValidation,
	// Only allows standard optimizations, not the longest compile times.
	CFLAG_StandardOptimization,
	// Always optimize, even when CFLAG_Debug is set. Required for some complex shaders and features.
	CFLAG_ForceOptimization,
	// Shader should generate symbols for debugging.
	CFLAG_GenerateSymbols,
	CFLAG_KeepDebugInfo UE_DEPRECATED(5.0, "CFLAG_GenerateSymbols should be used to signal if debug data needs to be generated") = CFLAG_GenerateSymbols,
	// Shader should insert debug/name info at the risk of generating non-deterministic libraries
	CFLAG_ExtraShaderData,
	// Allows the (external) symbols to be specific to each shader rather than trying to deduplicate.
	CFLAG_AllowUniqueSymbols,
	CFLAG_NoFastMath,
	// Explicitly enforce zero initialization on shader platforms that may omit it.
	CFLAG_ZeroInitialise,
	// Explicitly enforce bounds checking on shader platforms that may omit it.
	CFLAG_BoundsChecking,
	// Force removing unused interpolators for platforms that can opt out
	CFLAG_ForceRemoveUnusedInterpolators,
	// Hint that it is a vertex to geometry shader
	CFLAG_VertexToGeometryShader,
	// Hint that it is a vertex to primitive shader
	CFLAG_VertexToPrimitiveShader,
	// Hint that a vertex shader should use automatic culling on certain platforms.
	CFLAG_VertexUseAutoCulling,
	// Prepare the shader for archiving in the native binary shader cache format
	CFLAG_Archive,
	// Shaders uses external texture so may need special runtime handling
	CFLAG_UsesExternalTexture,
	// Use emulated uniform buffers on supported platforms
	CFLAG_UseEmulatedUB,
	// Enable wave operation intrinsics (requires DX12 and DXC/DXIL on PC).
	// Check GRHISupportsWaveOperations before using shaders compiled with this flag at runtime.
	// https://github.com/Microsoft/DirectXShaderCompiler/wiki/Wave-Intrinsics
	CFLAG_WaveOperations,
	// Use DirectX Shader Compiler (DXC) to compile all shaders, intended for compatibility testing.
	CFLAG_ForceDXC,
	CFLAG_SkipOptimizations,
	// Temporarily disable optimizations with DXC compiler only, intended to workaround shader compiler bugs until they can be resolved with 1st party
	CFLAG_SkipOptimizationsDXC,
	// Typed UAV loads are disallowed by default, as Windows 7 D3D 11.0 does not support them; this flag allows a shader to use them.
	CFLAG_AllowTypedUAVLoads,
	// Prefer shader execution in wave32 mode if possible.
	CFLAG_Wave32,
	// Enable support of inline raytracing in compute shader.
	CFLAG_InlineRayTracing,
	// Force using the SC rewrite functionality before calling DXC on D3D12
	CFLAG_D3D12ForceShaderConductorRewrite,
	// Enable support of C-style data types for platforms that can. Check for PLATFORM_SUPPORTS_REAL_TYPES and FDataDrivenShaderPlatformInfo::GetSupportsRealTypes()
	CFLAG_AllowRealTypes,
	// Precompile HLSL to optimized HLSL, then forward to FXC. Speeds up some shaders that take longer with FXC and works around crashes in FXC.
	CFLAG_PrecompileWithDXC,
	// Enable HLSL 2021 version. Enables templates, operator overloaing, C++ style function overloading. Contains breaking change with short-circuiting evaluation.
	CFLAG_HLSL2021,
	// Allow warnings to be treated as errors
	CFLAG_WarningsAsErrors,
	// Enabled if bindless resources are enabled for the platform
	CFLAG_BindlessResources,
	// Enabled if bindless samplers are enabled for the platform
	CFLAG_BindlessSamplers,
	// EXPERIMENTAL: Run the shader re-writer that removes any unused functions/resources/types from source code before compilation.
	CFLAG_RemoveDeadCode,

	CFLAG_Max,
};
static_assert(CFLAG_Max < 64, "Out of bitfield space! Modify FShaderCompilerFlags");

struct FShaderCompilerResourceTable
{
	/** Bits indicating which resource tables contain resources bound to this shader. */
	uint32 ResourceTableBits;

	/** The max index of a uniform buffer from which resources are bound. */
	uint32 MaxBoundResourceTable;

	/** Mapping of bound Textures to their location in resource tables. */
	TArray<uint32> TextureMap;

	/** Mapping of bound SRVs to their location in resource tables. */
	TArray<uint32> ShaderResourceViewMap;

	/** Mapping of bound sampler states to their location in resource tables. */
	TArray<uint32> SamplerMap;

	/** Mapping of bound UAVs to their location in resource tables. */
	TArray<uint32> UnorderedAccessViewMap;

	/** Hash of the layouts of resource tables at compile time, used for runtime validation. */
	TArray<uint32> ResourceTableLayoutHashes;

	FShaderCompilerResourceTable()
		: ResourceTableBits(0)
		, MaxBoundResourceTable(0)
	{
	}
};

/** Additional compilation settings that can be configured by each FMaterial instance before compilation */
struct FExtraShaderCompilerSettings
{
	bool bExtractShaderSource = false;
	FString OfflineCompilerPath;

	friend FArchive& operator<<(FArchive& Ar, FExtraShaderCompilerSettings& StatsSettings)
	{
		// Note: this serialize is used to pass between UE and the shader compile worker, recompile both when modifying
		return Ar << StatsSettings.bExtractShaderSource << StatsSettings.OfflineCompilerPath;
	}
};

namespace FOodleDataCompression
{
	enum class ECompressor : uint8;
	enum class ECompressionLevel : int8;
}

/** Struct that gathers all readonly inputs needed for the compilation of a single shader. */
struct FShaderCompilerInput
{
	FShaderTarget Target;
	
	FName ShaderFormat;
	FName CompressionFormat;
	FName ShaderPlatformName;
	
	FString SourceFilePrefix;
	FString VirtualSourceFilePath;
	FString EntryPointName;
	FString ShaderName;

	// Skips the preprocessor and instead loads the usf file directly
	bool bSkipPreprocessedCache;

	bool bGenerateDirectCompileFile;

	// Shader pipeline information
	bool bCompilingForShaderPipeline;
	bool bIncludeUsedOutputs;
	TArray<FString> UsedOutputs;

	// Dump debug path (up to platform) e.g. "D:/Project/Saved/ShaderDebugInfo/PCD3D_SM5"
	FString DumpDebugInfoRootPath;
	// only used if enabled by r.DumpShaderDebugInfo (platform/groupname) e.g. ""
	FString DumpDebugInfoPath;
	// materialname or "Global" "for debugging and better error messages
	FString DebugGroupName;

	FString DebugExtension;

	// Description of the configuration used when compiling. 
	FString DebugDescription;

	// Compilation Environment
	FShaderCompilerEnvironment Environment;
	TRefCountPtr<FSharedShaderCompilerEnvironment> SharedEnvironment;

	// The root of the shader parameter structures / uniform buffers bound to this shader to generate shader resource table from.
	// This only set if a shader class is defining the 
	const FShaderParametersMetadata* RootParametersStructure = nullptr;


	// Additional compilation settings that can be filled by FMaterial::SetupExtaCompilationSettings
	// FMaterial::SetupExtaCompilationSettings is usually called by each (*)MaterialShaderType::BeginCompileShader() function
	FExtraShaderCompilerSettings ExtraSettings;

	/** Oodle-specific compression algorithm - used if CompressionFormat is set to NAME_Oodle. */
	FOodleDataCompression::ECompressor OodleCompressor;

	/** Oodle-specific compression level - used if CompressionFormat is set to NAME_Oodle. */
	FOodleDataCompression::ECompressionLevel OodleLevel;

	FShaderCompilerInput() :
		Target(SF_NumFrequencies, SP_NumPlatforms),
		bSkipPreprocessedCache(false),
		bGenerateDirectCompileFile(false),
		bCompilingForShaderPipeline(false),
		bIncludeUsedOutputs(false)
	{
	}

	// generate human readable name for debugging
	FString GenerateShaderName() const
	{
		FString Name;

		if(DebugGroupName == TEXT("Global"))
		{
			Name = VirtualSourceFilePath + TEXT("|") + EntryPointName;
		}
		else
		{
			// we skip EntryPointName as it's usually not useful
			Name = DebugGroupName + TEXT(":") + VirtualSourceFilePath;
		}

		return Name;
	}

	FString GetSourceFilename() const
	{
		return FPaths::GetCleanFilename(VirtualSourceFilePath);
	}

	void GatherSharedInputs(
		TMap<FString, FString>& ExternalIncludes,
		TArray<TRefCountPtr<FSharedShaderCompilerEnvironment>>& SharedEnvironments,
		TArray<const FShaderParametersMetadata*>& ParametersStructures)
	{
		check(!SharedEnvironment || SharedEnvironment->IncludeVirtualPathToExternalContentsMap.Num() == 0);

		for (const auto& It : Environment.IncludeVirtualPathToExternalContentsMap)
		{
			FString* FoundEntry = ExternalIncludes.Find(It.Key);

			if (!FoundEntry)
			{
				ExternalIncludes.Add(It.Key, *It.Value);
			}
		}

		if (SharedEnvironment)
		{
			SharedEnvironments.AddUnique(SharedEnvironment);
		}

		if (RootParametersStructure)
		{
			ParametersStructures.AddUnique(RootParametersStructure);
		}
	}

	void SerializeSharedInputs(FArchive& Ar, const TArray<TRefCountPtr<FSharedShaderCompilerEnvironment>>& SharedEnvironments, const TArray<const FShaderParametersMetadata*>& ParametersStructures)
	{
		check(Ar.IsSaving());

		TArray<FString> ReferencedExternalIncludes;
		ReferencedExternalIncludes.Empty(Environment.IncludeVirtualPathToExternalContentsMap.Num());

		for (const auto& It : Environment.IncludeVirtualPathToExternalContentsMap)
		{
			ReferencedExternalIncludes.Add(It.Key);
		}

		Ar << ReferencedExternalIncludes;

		int32 SharedEnvironmentIndex = SharedEnvironments.Find(SharedEnvironment);
		Ar << SharedEnvironmentIndex;

		int32 ShaderParameterStructureIndex = INDEX_NONE;
		if (RootParametersStructure)
		{
			ShaderParameterStructureIndex = ParametersStructures.Find(RootParametersStructure);
			check(ShaderParameterStructureIndex != INDEX_NONE);
		}
		Ar << ShaderParameterStructureIndex;
	}

	void DeserializeSharedInputs(
		FArchive& Ar,
		const TMap<FString, FThreadSafeSharedStringPtr>& ExternalIncludes,
		const TArray<FShaderCompilerEnvironment>& SharedEnvironments,
		const TArray<TUniquePtr<FShaderParametersMetadata>>& ShaderParameterStructures)
	{
		check(Ar.IsLoading());

		TArray<FString> ReferencedExternalIncludes;
		Ar << ReferencedExternalIncludes;

		Environment.IncludeVirtualPathToExternalContentsMap.Reserve(ReferencedExternalIncludes.Num());

		for (int32 i = 0; i < ReferencedExternalIncludes.Num(); i++)
		{
			Environment.IncludeVirtualPathToExternalContentsMap.Add(ReferencedExternalIncludes[i], ExternalIncludes.FindChecked(ReferencedExternalIncludes[i]));
		}

		int32 SharedEnvironmentIndex = 0;
		Ar << SharedEnvironmentIndex;

		if (SharedEnvironments.IsValidIndex(SharedEnvironmentIndex))
		{
			Environment.Merge(SharedEnvironments[SharedEnvironmentIndex]);
		}

		int32 ShaderParameterStructureIndex;
		Ar << ShaderParameterStructureIndex;
		if (ShaderParameterStructureIndex != INDEX_NONE)
		{
			RootParametersStructure = ShaderParameterStructures[ShaderParameterStructureIndex].Get();
		}
	}

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FShaderCompilerInput& Input);

	bool IsRayTracingShader() const
	{
		return IsRayTracingShaderFrequency(Target.GetFrequency());
	}
};

/** A shader compiler error or warning. */
struct FShaderCompilerError
{
	FShaderCompilerError(const TCHAR* InStrippedErrorMessage = TEXT(""))
		: ErrorVirtualFilePath(TEXT(""))
		, ErrorLineString(TEXT(""))
		, StrippedErrorMessage(InStrippedErrorMessage)
		, HighlightedLine(TEXT(""))
		, HighlightedLineMarker(TEXT(""))
	{}

	FShaderCompilerError(const TCHAR* InVirtualFilePath, const TCHAR* InLineString, const TCHAR* InStrippedErrorMessage)
		: ErrorVirtualFilePath(InVirtualFilePath)
		, ErrorLineString(InLineString)
		, StrippedErrorMessage(InStrippedErrorMessage)
		, HighlightedLine(TEXT(""))
		, HighlightedLineMarker(TEXT(""))
	{}

	FShaderCompilerError(FString&& InStrippedErrorMessage)
		: ErrorVirtualFilePath(TEXT(""))
		, ErrorLineString(TEXT(""))
		, StrippedErrorMessage(MoveTemp(InStrippedErrorMessage))
		, HighlightedLine(TEXT(""))
		, HighlightedLineMarker(TEXT(""))
	{}

	FShaderCompilerError(FString&& InStrippedErrorMessage, FString&& InHighlightedLine, FString&& InHighlightedLineMarker)
		: ErrorVirtualFilePath(TEXT(""))
		, ErrorLineString(TEXT(""))
		, StrippedErrorMessage(MoveTemp(InStrippedErrorMessage))
		, HighlightedLine(MoveTemp(InHighlightedLine))
		, HighlightedLineMarker(MoveTemp(InHighlightedLineMarker))
	{}

	FString ErrorVirtualFilePath;
	FString ErrorLineString;
	FString StrippedErrorMessage;
	FString HighlightedLine;
	FString HighlightedLineMarker;

	/** Returns the error message with source file and source line (if present), as well as a line marker separated with a LINE_TERMINATOR. */
	FString RENDERCORE_API GetErrorStringWithSourceLocation() const;
	
	/** Returns the error message with source file and source line (if present), as well as a line marker separated with a LINE_TERMINATOR. */
	FString RENDERCORE_API GetErrorStringWithLineMarker() const;

	/** Returns the error message with source file and source line (if present). */
	FString RENDERCORE_API GetErrorString(bool bOmitLineMarker = false) const;

	/**
	Returns true if this error message has a marker string for the highlighted source line where the error occurred. Example:
		/Engine/Private/MySourceFile.usf(120): error: undeclared identifier 'a'
		float b = a;
				  ^
	*/
	FORCEINLINE bool HasLineMarker() const
	{
		return !HighlightedLine.IsEmpty() && !HighlightedLineMarker.IsEmpty();
	}

	/** Extracts the file path and source line from StrippedErrorMessage to ErrorVirtualFilePath and ErrorLineString. */
	bool RENDERCORE_API ExtractSourceLocation();

	/** Returns the path of the underlying source file relative to the process base dir. */
	FString RENDERCORE_API GetShaderSourceFilePath() const;

	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerError& Error)
	{
		return Ar << Error.ErrorVirtualFilePath << Error.ErrorLineString << Error.StrippedErrorMessage << Error.HighlightedLine << Error.HighlightedLineMarker;
	}
};

/**
 *	The output of the shader compiler.
 *	Bump ShaderCompileWorkerOutputVersion if FShaderCompilerOutput changes
 */
struct FShaderCompilerOutput
{
	FShaderCompilerOutput()
	:	NumInstructions(0)
	,	NumTextureSamplers(0)
	,	CompileTime(0.0)
	,	PreprocessTime(0.0)
	,	bSucceeded(false)
	,	bFailedRemovingUnused(false)
	,	bSupportsQueryingUsedAttributes(false)
	,	bUsedHLSLccCompiler(false)
	{
	}

	FShaderParameterMap ParameterMap;
	TArray<FShaderCompilerError> Errors;
	TArray<FString> PragmaDirectives;
	FShaderTarget Target;
	FShaderCode ShaderCode;
	FSHAHash OutputHash;
	uint32 NumInstructions;
	uint32 NumTextureSamplers;
	double CompileTime;
	double PreprocessTime;
	bool bSucceeded;
	bool bFailedRemovingUnused;
	bool bSupportsQueryingUsedAttributes;
	bool bUsedHLSLccCompiler;
	TArray<FString> UsedAttributes;

	FString OptionalPreprocessedShaderSource;
	FString OptionalFinalShaderSource;

	TArray<uint8> PlatformDebugData;

	/** Generates OutputHash from the compiler output. */
	RENDERCORE_API void GenerateOutputHash();

	/** Calls GenerateOutputHash() before the compression, replaces FShaderCode with the compressed data (if compression result was smaller). */
	RENDERCORE_API void CompressOutput(FName ShaderCompressionFormat, FOodleDataCompression::ECompressor OodleCompressor, FOodleDataCompression::ECompressionLevel OodleLevel);

	friend FArchive& operator<<(FArchive& Ar, FShaderCompilerOutput& Output)
	{
		// Note: this serialize is used to pass between UE and the shader compile worker, recompile both when modifying
		Ar << Output.ParameterMap << Output.Errors << Output.Target << Output.ShaderCode << Output.OutputHash << Output.NumInstructions << Output.NumTextureSamplers << Output.bSucceeded;
		Ar << Output.bFailedRemovingUnused << Output.bSupportsQueryingUsedAttributes << Output.UsedAttributes;
		Ar << Output.CompileTime;
		Ar << Output.PreprocessTime;
		Ar << Output.OptionalPreprocessedShaderSource;
		Ar << Output.OptionalFinalShaderSource;
		Ar << Output.PlatformDebugData;

		return Ar;
	}
};

enum class ESCWErrorCode
{
	NotSet = -1,
	Success,
	GeneralCrash,
	BadShaderFormatVersion,
	BadInputVersion,
	BadSingleJobHeader,
	BadPipelineJobHeader,
	CantDeleteInputFile,
	CantSaveOutputFile,
	NoTargetShaderFormatsFound,
	CantCompileForSpecificFormat,
	CrashInsidePlatformCompiler,
	BadInputFile
};


inline bool ShouldUseStableConstantBuffer(const FShaderCompilerInput& Input)
{
	// stable constant buffer is for the FShaderParameterBindings::BindForLegacyShaderParameters() code path.
	// Ray tracing shaders use FShaderParameterBindings::BindForRootShaderParameters instead.
	if (Input.IsRayTracingShader())
	{
		return false;
	}

	return Input.RootParametersStructure != nullptr;
}


/**
 * Validates the format of a virtual shader file path.
 * Meant to be use as such: check(CheckVirtualShaderFilePath(VirtualFilePath));
 * CompileErrors output array is optional.
 */
extern RENDERCORE_API bool CheckVirtualShaderFilePath(FStringView VirtualPath, TArray<FShaderCompilerError>* CompileErrors = nullptr);

/**
 * Loads the shader file with the given name.
 * @param VirtualFilePath - The virtual path of shader file to load.
 * @param OutFileContents - If true is returned, will contain the contents of the shader file. Can be null.
 * @return True if the file was successfully loaded.
 */
extern RENDERCORE_API bool LoadShaderSourceFile(const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform, FString* OutFileContents, TArray<FShaderCompilerError>* OutCompileErrors, const FName* ShaderPlatformName = nullptr);

enum class EShaderCompilerWorkerType : uint8
{
	None,
	LocalThread,
	Distributed,
};

enum class EShaderCompileJobType : uint8
{
	Single,
	Pipeline,
	Num,
};
static const int32 NumShaderCompileJobTypes = (int32)EShaderCompileJobType::Num;

enum class EShaderCompileJobPriority : uint8
{
	None = 0xff,

	Low = 0u,
	Normal,
	High,
	ForceLocal, // Force shader to skip XGE and compile on local machine
	Num,
};
static const int32 NumShaderCompileJobPriorities = (int32)EShaderCompileJobPriority::Num;

inline const TCHAR* ShaderCompileJobPriorityToString(EShaderCompileJobPriority v)
{
	switch (v)
	{
	case EShaderCompileJobPriority::None: return TEXT("None");
	case EShaderCompileJobPriority::Low: return TEXT("Low");
	case EShaderCompileJobPriority::Normal: return TEXT("Normal");
	case EShaderCompileJobPriority::High: return TEXT("High");
	case EShaderCompileJobPriority::ForceLocal: return TEXT("ForceLocal");
	default: checkNoEntry(); return TEXT("");
	}
}
