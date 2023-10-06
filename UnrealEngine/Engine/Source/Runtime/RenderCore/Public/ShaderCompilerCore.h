// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCompilerCore.h: Shader Compiler core module definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Hash/Blake3.h"
#include "Stats/Stats.h"
#include "Templates/RefCounting.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/CoreStats.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadata.h"
#include "ShaderParameterParser.h"

class Error;
class IShaderFormat;
class FShaderCommonCompileJob;
class FShaderCompileJob;
class FShaderPipelineCompileJob;

// this is for the protocol, not the data, bump if FShaderCompilerInput or ProcessInputFromArchive changes.
inline const int32 ShaderCompileWorkerInputVersion = 21;
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes.
inline const int32 ShaderCompileWorkerOutputVersion = 16;
// this is for the protocol, not the data.
inline const int32 ShaderCompileWorkerSingleJobHeader = 'S';
// this is for the protocol, not the data.
inline const int32 ShaderCompileWorkerPipelineJobHeader = 'P';

namespace UE::ShaderCompiler
{
	RENDERCORE_API ERHIBindlessConfiguration GetBindlessResourcesConfiguration(FName ShaderFormat);
	RENDERCORE_API ERHIBindlessConfiguration GetBindlessSamplersConfiguration(FName ShaderFormat);
}

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
	CFLAG_D3D12ForceShaderConductorRewrite UE_DEPRECATED(5.3, "CFLAG_D3D12ForceShaderConductorRewrite has been deprecated since UE5.3 and the flag is ignored"),
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
	CFLAG_UseLegacyPreprocessor UE_DEPRECATED(5.3, "Legacy preprocessor has been removed as of UE 5.3; please report any issues with the new preprocessor to the UE rendering team."),
	// Enable CullBeforeFetch optimization on supported platforms
	CFLAG_CullBeforeFetch,
	// Enable WarpCulling optimization on supported platforms
	CFLAG_WarpCulling,
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

enum class EShaderDebugInfoFlags : uint8
{
	Default = 0,
	DirectCompileCommandLine = 1 << 0,
	InputHash = 1 << 1,
	Diagnostics = 1 << 2,
};
ENUM_CLASS_FLAGS(EShaderDebugInfoFlags)

/** Struct that gathers all readonly inputs needed for the compilation of a single shader. */
struct FShaderCompilerInput
{
	FShaderTarget Target;
	
	FName ShaderFormat;
	FName CompressionFormat;
	FName ShaderPlatformName;
	
	FString VirtualSourceFilePath;
	FString EntryPointName;
	FString ShaderName;

	// Skips the preprocessor and instead loads the usf file directly
	bool bSkipPreprocessedCache;

	UE_DEPRECATED(5.3, "Use DebugInfoFlags field (EDebugInfoFlags::DirectCompileCommandLine)")
	bool bGenerateDirectCompileFile;

	// Indicates which additional debug outputs should be written for this compile job.
	EShaderDebugInfoFlags DebugInfoFlags;

	// True if the backend for this job implements the independent preprocessing API.
	bool bIndependentPreprocessed;

	// True if the cache key for this job should be based on preprocessed source. If so,
	// preprocessing will be executed in the cook process independent of compilation (and
	// as such this will only ever be set for jobs whose shader format supports independent
	// preprocessing)
	bool bCachePreprocessed;
	
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

	// Hash of this input (used as the key for the shader job cache)
	FBlake3Hash Hash;

	// Compilation Environment
	FShaderCompilerEnvironment Environment;
	TRefCountPtr<FSharedShaderCompilerEnvironment> SharedEnvironment;

	// The root of the shader parameter structures / uniform buffers bound to this shader to generate shader resource table from.
	// This only set if a shader class is defining the 
	const FShaderParametersMetadata* RootParametersStructure = nullptr;


	// Additional compilation settings that can be filled by FMaterial::SetupExtraCompilationSettings
	// FMaterial::SetupExtraCompilationSettings is usually called by each (*)MaterialShaderType::BeginCompileShader() function
	FExtraShaderCompilerSettings ExtraSettings;

	/** Oodle-specific compression algorithm - used if CompressionFormat is set to NAME_Oodle. */
	FOodleDataCompression::ECompressor OodleCompressor;

	/** Oodle-specific compression level - used if CompressionFormat is set to NAME_Oodle. */
	FOodleDataCompression::ECompressionLevel OodleLevel;

	FShaderCompilerInput() :
		Target(SF_NumFrequencies, SP_NumPlatforms),
		bSkipPreprocessedCache(false),
		DebugInfoFlags(EShaderDebugInfoFlags::Default),
		bIndependentPreprocessed(false),
		bCachePreprocessed(false),
		bCompilingForShaderPipeline(false),
		bIncludeUsedOutputs(false)
	{
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Explicitly-defaulted copy/move ctors & assignment operators are needed temporarily due to 
	// deprecation of bGenerateDirectCompileFile field. These can be removed once the deprecation
	// window for said field ends.
	FShaderCompilerInput(FShaderCompilerInput&&) = default;
	FShaderCompilerInput(const FShaderCompilerInput&) = default;
	FShaderCompilerInput& operator=(FShaderCompilerInput&&) = default;
	FShaderCompilerInput& operator=(const FShaderCompilerInput&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	bool DumpDebugInfoEnabled() const 
	{
		return DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*DumpDebugInfoPath);
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
			Name = DebugGroupName + TEXT(":") + VirtualSourceFilePath + TEXT("|") + EntryPointName;
		}

		return Name;
	}

	FStringView GetSourceFilenameView() const
	{
		return FPathViews::GetCleanFilename(VirtualSourceFilePath);
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

		// If the input is already preprocessed we don't need to serialize includes when writing worker input files
		if (!bCachePreprocessed)
		{
			for (const auto& It : Environment.IncludeVirtualPathToExternalContentsMap)
			{
				FString* FoundEntry = ExternalIncludes.Find(It.Key);

				if (!FoundEntry)
				{
					ExternalIncludes.Add(It.Key, *It.Value);
				}
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

		if (!bCachePreprocessed)
		{
			TArray<FString> ReferencedExternalIncludes;
			ReferencedExternalIncludes.Empty(Environment.IncludeVirtualPathToExternalContentsMap.Num());

			for (const auto& It : Environment.IncludeVirtualPathToExternalContentsMap)
			{
				ReferencedExternalIncludes.Add(It.Key);
			}

			Ar << ReferencedExternalIncludes;
		}

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

		if (!bCachePreprocessed)
		{
			TArray<FString> ReferencedExternalIncludes;
			Ar << ReferencedExternalIncludes;

			Environment.IncludeVirtualPathToExternalContentsMap.Reserve(ReferencedExternalIncludes.Num());

			for (int32 i = 0; i < ReferencedExternalIncludes.Num(); i++)
			{
				Environment.IncludeVirtualPathToExternalContentsMap.Add(ReferencedExternalIncludes[i], ExternalIncludes.FindChecked(ReferencedExternalIncludes[i]));
			}
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

	bool ShouldUseStableConstantBuffer() const
	{
		// stable constant buffer is for the FShaderParameterBindings::BindForLegacyShaderParameters() code path.
		// Ray tracing shaders use FShaderParameterBindings::BindForRootShaderParameters instead.
		if (IsRayTracingShader())
		{
			return false;
		}

		return RootParametersStructure != nullptr;
	}

	/** Returns whether this shader input *can* be compiled with the legacy FXC compiler. */
	UE_DEPRECATED(5.3, "CanCompileWithLegacyFxc doesn't have enough information to correctly flag the input as requiring FXC. Please use internal shader format code instead.")
	bool CanCompileWithLegacyFxc() const
	{
		return !(Target.GetPlatform() == SP_PCD3D_SM6
			|| IsRayTracingShader()
			|| Environment.CompilerFlags.Contains(CFLAG_WaveOperations)
			|| Environment.CompilerFlags.Contains(CFLAG_ForceDXC)
			|| Environment.CompilerFlags.Contains(CFLAG_InlineRayTracing)
			);
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
	,	bSupportsQueryingUsedAttributes(false)
	,	bSerializeModifiedSource(false)
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
	UE_DEPRECATED(5.3, "bFailedRemovingUnused field is no longer used")
	bool bFailedRemovingUnused;
	bool bSupportsQueryingUsedAttributes;
	UE_DEPRECATED(5.3, "bUsedHLSLccCompiler field is no longer used")
	bool bUsedHLSLccCompiler;
	bool bSerializeModifiedSource;
	TArray<FString> UsedAttributes;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Explicitly-defaulted copy/move ctors & assignment operators are needed temporarily due to 
	// deprecation of bFailedRemovingUnused/bUsedHLSLccCompiler fields. These can be removed once the deprecation
	// window for said fields ends.
	FShaderCompilerOutput(FShaderCompilerOutput&&) = default;
	FShaderCompilerOutput(const FShaderCompilerOutput&) = default;
	FShaderCompilerOutput& operator=(FShaderCompilerOutput&&) = default;
	FShaderCompilerOutput& operator=(const FShaderCompilerOutput&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	TArray<FShaderCodeValidationStride> ParametersStrideToValidate;

	/** This field should be set by backends which do not implement the independent preprocessing API to contain the "final" shader source as 
	 * passed to the platform compiler. For backends that do implement this API this is superceded by ModifiedShaderSource (and will eventually
	 * be deprecated).
	 */
	FString OptionalFinalShaderSource;

	/** Use this field to store the shader source code if it's modified as part of the shader format's compilation process. This field is only 
	 * currently required for shader formats which implement the independent preprocessing API and should only be set when additional manipulation 
	 * is required that is not part of the implementation of PreprocessShader. This version of the source, if set, will be what is written as part
	 * of the debug dumps of preprocessed source, as well as used in place of OptionalFinalShaderSource for upstream code which explicitly requests
	 * the final source code for other purposes (i.e. when ExtraSettings.bExtractShaderSource is set on the FShaderCompilerInput struct)
	 */
	FString ModifiedShaderSource;

	/** Use this field to store the entry point name if it's modified as part of the shader format's compilation process. This field is only 
	 * currently required for shader formats which implement the independent preprocessing API And should only be set when compilation requires
	 * a different entry point than was set on the FShaderCompilerInput struct. */
	FString ModifiedEntryPointName;

	TArray<uint8> PlatformDebugData;

	/** Generates OutputHash from the compiler output. */
	RENDERCORE_API void GenerateOutputHash();

	/** Calls GenerateOutputHash() before the compression, replaces FShaderCode with the compressed data (if compression result was smaller). */
	RENDERCORE_API void CompressOutput(FName ShaderCompressionFormat, FOodleDataCompression::ECompressor OodleCompressor, FOodleDataCompression::ECompressionLevel OodleLevel);

	/** Add optional data in ShaderCode to perform additional shader input validation at runtime*/
	RENDERCORE_API void SerializeShaderCodeValidation();

	friend FArchive& operator<<(FArchive& Ar, FShaderCompilerOutput& Output)
	{
		// Note: this serialize is used to pass between UE and the shader compile worker, recompile both when modifying
		Ar << Output.ParameterMap << Output.Errors << Output.Target << Output.ShaderCode << Output.OutputHash << Output.NumInstructions << Output.NumTextureSamplers << Output.bSucceeded;
		Ar << Output.bSupportsQueryingUsedAttributes << Output.UsedAttributes;
		Ar << Output.CompileTime;
		Ar << Output.PreprocessTime;
		Ar << Output.OptionalFinalShaderSource;
		Ar << Output.bSerializeModifiedSource;
		if (Output.bSerializeModifiedSource)
		{
			Ar << Output.ModifiedShaderSource;
			Ar << Output.ModifiedEntryPointName;
		}
		Ar << Output.PlatformDebugData;

		return Ar;
	}
};

struct FSCWErrorCode
{
	enum ECode : int32
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
		BadInputFile,
		OutOfMemory,
	};

	/**
	Sets the global SCW error code if it hasn't been set before.
	Call Reset first before setting a new value.
	Returns true on success, otherwise the error code has already been set.
	*/
	static RENDERCORE_API void Report(ECode Code, const FStringView& Info = {});

	/** Resets the global SCW error code to NotSet. */
	static RENDERCORE_API void Reset();

	/** Returns the global SCW error code. */
	static RENDERCORE_API ECode Get();

	/** Returns the global SCW error code information string. Empty string if not set. */
	static RENDERCORE_API const FString& GetInfo();

	/** Returns true if the SCW global error code has been set. Equivalent to 'Get() != NotSet'. */
	static RENDERCORE_API bool IsSet();
};

#if PLATFORM_WINDOWS
extern RENDERCORE_API int HandleShaderCompileException(Windows::LPEXCEPTION_POINTERS Info, FString& OutExMsg, FString& OutCallStack);
#endif
extern RENDERCORE_API const IShaderFormat* FindShaderFormat(FName Format, const TArray<const IShaderFormat*>& ShaderFormats);

// Executes preprocessing for the given job, if the job is marked to be preprocessed independently prior to compilation.
extern RENDERCORE_API void ConditionalPreprocessShader(FShaderCommonCompileJob* Job);
UE_DEPRECATED(5.3, "Use CompileShader overload which takes an FShaderCompileJob& rather than passing input/output directly.")
extern RENDERCORE_API void CompileShader(const TArray<const IShaderFormat*>& ShaderFormats, FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FString& WorkingDirectory, int32* CompileCount = nullptr);
extern RENDERCORE_API void CompileShader(const TArray<const IShaderFormat*>& ShaderFormats, FShaderCompileJob& Job, const FString& WorkingDirectory, int32* CompileCount = nullptr);
extern RENDERCORE_API void CompileShaderPipeline(const TArray<const IShaderFormat*>& ShaderFormats, FShaderPipelineCompileJob* PipelineJob, const FString& WorkingDirectory, int32* CompileCount = nullptr);

UE_DEPRECATED(5.2, "Functionality has moved to FShaderCompilerInput::ShouldUseStableConstantBuffer")
inline bool ShouldUseStableConstantBuffer(const FShaderCompilerInput& Input)
{
	return Input.ShouldUseStableConstantBuffer();
}


/**
 * Validates the format of a virtual shader file path.
 * Meant to be use as such: check(CheckVirtualShaderFilePath(VirtualFilePath));
 * CompileErrors output array is optional.
 */
extern RENDERCORE_API bool CheckVirtualShaderFilePath(FStringView VirtualPath, TArray<FShaderCompilerError>* CompileErrors = nullptr);

/**
 * Fixes up the given virtual file path (substituting virtual platform path/autogen path for the given platform)
 */
extern RENDERCORE_API void FixupShaderFilePath(FString& VirtualFilePath, EShaderPlatform ShaderPlatform, const FName* ShaderPlatformName);

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


