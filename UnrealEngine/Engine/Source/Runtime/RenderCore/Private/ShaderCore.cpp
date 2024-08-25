// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCore.cpp: Shader core module implementation.
=============================================================================*/

#include "ShaderCore.h"
#include "Algo/Find.h"
#include "Async/ParallelFor.h"
#include "Compression/OodleDataCompression.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformStackWalk.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Math/BigInt.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Compression.h"
#include "Misc/CoreMisc.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "RHIShaderFormatDefinitions.inl"
#include "Serialization/MemoryHasher.h"
#include "Serialization/MemoryWriter.h"
#include "Shader.h"
#include "ShaderCompilerCore.h"
#include "ShaderCompilerDefinitions.h"
#include "ShaderCompilerJobTypes.h"
#include "Stats/StatsMisc.h"
#include "String/Find.h"
#include "Tasks/Task.h"
#include "VertexFactory.h"
#if WITH_EDITOR
#include "Misc/ConfigCacheIni.h"
#include "Misc/StringBuilder.h"
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#	include <winnt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

static TAutoConsoleVariable<int32> CVarShaderDevelopmentMode(
	TEXT("r.ShaderDevelopmentMode"),
	0,
	TEXT("0: Default, 1: Enable various shader development utilities, such as the ability to retry on failed shader compile, and extra logging as shaders are compiled."),
	ECVF_Default);

static TAutoConsoleVariable<bool> CVarDumpDebugInfoForCacheHits(
	TEXT("r.ShaderCompiler.DumpDebugInfoForCacheHits"),
	true,
	TEXT("If true, debug info (via IShaderFormat::OutputDebugData) will be output for all jobs including duplicates and cache/DDC hits. If false, only jobs that actually executed compilation will dump debug info."),
	ECVF_Default);

void UpdateShaderDevelopmentMode()
{
	// Keep LogShaders verbosity in sync with r.ShaderDevelopmentMode
	// r.ShaderDevelopmentMode==1 results in all LogShaders log messages being displayed.
	// if r.ShaderDevelopmentMode isn't set, we leave the category alone (it defaults to Error, but we can be overriding it to something higher)
	bool bLogShadersUnsuppressed = UE_LOG_ACTIVE(LogShaders, Log);
	bool bDesiredLogShadersUnsuppressed = CVarShaderDevelopmentMode.GetValueOnGameThread() == 1;

	if (bLogShadersUnsuppressed != bDesiredLogShadersUnsuppressed)
	{
		if (bDesiredLogShadersUnsuppressed)
		{
			UE_SET_LOG_VERBOSITY(LogShaders, Log);
		}
	}
}

//
// Shader stats
//

DEFINE_STAT(STAT_ShaderCompiling_NiagaraShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumTotalNiagaraShaders);

DEFINE_STAT(STAT_ShaderCompiling_OpenColorIOShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumTotalOpenColorIOShaders);

DEFINE_STAT(STAT_ShaderCompiling_MaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_GlobalShaders);
DEFINE_STAT(STAT_ShaderCompiling_RHI);
DEFINE_STAT(STAT_ShaderCompiling_HashingShaderFiles);
DEFINE_STAT(STAT_ShaderCompiling_LoadingShaderFiles);
DEFINE_STAT(STAT_ShaderCompiling_HLSLTranslation);
DEFINE_STAT(STAT_ShaderCompiling_DDCLoading);
DEFINE_STAT(STAT_ShaderCompiling_MaterialLoading);
DEFINE_STAT(STAT_ShaderCompiling_MaterialCompiling);

DEFINE_STAT(STAT_ShaderCompiling_NumTotalMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumSpecialMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumParticleMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumSkinnedMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumLitMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumUnlitMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumTransparentMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumOpaqueMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumMaskedMaterialShaders);


DEFINE_STAT(STAT_Shaders_NumShadersLoaded);
DEFINE_STAT(STAT_Shaders_NumShaderResourcesLoaded);
DEFINE_STAT(STAT_Shaders_NumShaderMaps);
DEFINE_STAT(STAT_Shaders_RTShaderLoadTime);
DEFINE_STAT(STAT_Shaders_NumShadersUsedForRendering);
DEFINE_STAT(STAT_Shaders_TotalRTShaderInitForRenderingTime);
DEFINE_STAT(STAT_Shaders_FrameRTShaderInitForRenderingTime);
DEFINE_STAT(STAT_Shaders_ShaderMemory);
DEFINE_STAT(STAT_Shaders_ShaderResourceMemory);
DEFINE_STAT(STAT_Shaders_ShaderPreloadMemory);

DEFINE_STAT(STAT_Shaders_NumShadersRegistered);
DEFINE_STAT(STAT_Shaders_NumShadersDuplicated);

PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then

/**
 * Singleton initial set of defines added when constructing a defines structure with bIncludeInitialDefines==true.  The
 * advantage of using preset defines is that the index of the initial define can be cached in the FShaderCompilerDefineNameCache
 * class, allowing direct lookup by index, bypassing the hash table.  This optimization is applied to system level defines used
 * by every shader (ones referenced by FShaderCompileUtilities::ApplyDerivedDefines).
 */
FShaderCompilerDefinitions* FShaderCompilerDefinitions::GInitialDefines = nullptr;

FShaderCompilerDefinitions::FShaderCompilerDefinitions(bool bIncludeInitialDefines)
	: InitialDefineCount(0), ValueCount(0)
{
	if (bIncludeInitialDefines && GInitialDefines)
	{
		*this = *GInitialDefines;
	}
	else
	{
		Pairs.Reserve(16);
		ValueTypes.Reserve(16);
	}
}

FShaderCompilerDefinitions::FShaderCompilerDefinitions(const FShaderCompilerDefinitions&) = default;

void FShaderCompilerDefinitions::InitializeInitialDefines(const FShaderCompilerDefinitions& InDefines)
{
	check(GInitialDefines == nullptr);
	GInitialDefines = new FShaderCompilerDefinitions;
	*GInitialDefines = InDefines;
	GInitialDefines->InitialDefineCount = InDefines.Pairs.Num();
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS


// Apply lock striping as we're mostly reader lock bound.
constexpr int32 GSHADERFILECACHE_BUCKETS = 31; /* prime number for best distribution using modulo */

struct FShaderFileCacheEntry
{
	FString Source;
	FShaderSharedAnsiStringPtr StrippedSource;				// Source with comments stripped out, and converted to ANSICHAR
	FShaderPreprocessDependenciesShared Dependencies;		// Stripped source with include dependencies, all in one shareable struct
};

struct FShaderFileCache
{
	/** Protects Map from simultaneous access by multiple threads. */
	FRWLock Lock;

	/** The shader file cache, used to minimize shader file reads */
	TMap<FString, FShaderFileCacheEntry> Map;
} GShaderFileCache[GSHADERFILECACHE_BUCKETS];

class FShaderHashCache
{
public:
	FShaderHashCache()
		: bInitialized(false)
	{
	}

	void Initialize()
	{
		const FString EmptyDirectory("");
		for (auto& Platform : Platforms)
		{
			Platform.IncludeDirectory = EmptyDirectory;
			Platform.ShaderHashCache.Reset();
		}


		TArray<FName> Modules;
		FModuleManager::Get().FindModules(SHADERFORMAT_MODULE_WILDCARD, Modules);

		if (!Modules.Num())
		{
			UE_LOG(LogShaders, Error, TEXT("No target shader formats found!"));
		}

		TArray<FName> SupportedFormats;

		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			IShaderFormat* ShaderFormat = FModuleManager::LoadModuleChecked<IShaderFormatModule>(Modules[ModuleIndex]).GetShaderFormat();
			if (ShaderFormat)
			{
				FString IncludeDirectory = ShaderFormat->GetPlatformIncludeDirectory();
				if (!IncludeDirectory.IsEmpty())
				{
					IncludeDirectory = "/" + IncludeDirectory + "/";
				}

				SupportedFormats.Reset();
				ShaderFormat->GetSupportedFormats(SupportedFormats);

				for (int32 FormatIndex = 0; FormatIndex < SupportedFormats.Num(); ++FormatIndex)
				{
					const EShaderPlatform ShaderPlatform = ShaderFormatNameToShaderPlatform(SupportedFormats[FormatIndex]);
					if (ShaderPlatform != SP_NumPlatforms)
					{
						Platforms[ShaderPlatform].IncludeDirectory = IncludeDirectory;
					}
				}
			}
		}

#if WITH_EDITOR
		for (int i = 0; i < EShaderPlatform::SP_NumPlatforms; ++i)
		{
			EShaderPlatform ShaderPlatform = EShaderPlatform(i);
			if (FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform) && FDataDrivenShaderPlatformInfo::GetIsPreviewPlatform(ShaderPlatform))
			{
				const EShaderPlatform CompilingPlatform = ShaderFormatNameToShaderPlatform(FDataDrivenShaderPlatformInfo::GetShaderFormat(ShaderPlatform));
				UpdateIncludeDirectoryForPreviewPlatform(ShaderPlatform, CompilingPlatform);
			}
		}
#endif

		bInitialized = true;
	}

	void UpdateIncludeDirectoryForPreviewPlatform(EShaderPlatform PreviewShaderPlatform, EShaderPlatform ParentShaderPlatform)
	{
		Platforms[PreviewShaderPlatform].IncludeDirectory = Platforms[ParentShaderPlatform].IncludeDirectory;
	}

	const FSHAHash* FindHash(EShaderPlatform ShaderPlatform, const FString& VirtualFilePath) const
	{
		check(ShaderPlatform < UE_ARRAY_COUNT(Platforms));
		checkf(bInitialized, TEXT("GShaderHashCache::Initialize needs to be called before GShaderHashCache::FindHash."));

		return Platforms[ShaderPlatform].ShaderHashCache.Find(VirtualFilePath);
	}

	FSHAHash& AddHash(EShaderPlatform ShaderPlatform, const FString& VirtualFilePath)
	{
		check(ShaderPlatform < UE_ARRAY_COUNT(Platforms));
		checkf(bInitialized, TEXT("GShaderHashCache::Initialize needs to be called before GShaderHashCache::AddHash."));

		return Platforms[ShaderPlatform].ShaderHashCache.Add(VirtualFilePath, FSHAHash());
	}

	static bool IsPlatformInclude(const FString& VirtualFilePath)
	{
		return (VirtualFilePath.StartsWith(TEXT("/Engine/Private/Platform/"))
			|| VirtualFilePath.StartsWith(TEXT("/Engine/Public/Platform/"))
			|| VirtualFilePath.StartsWith(TEXT("/Platform/")));
	}

	bool ShouldIgnoreInclude(const FString& VirtualFilePath, EShaderPlatform ShaderPlatform) const
	{
		// Ignore only platform specific files, which won't be used by the target platform.
		if (IsPlatformInclude(VirtualFilePath))
		{
			const FString& PlatformIncludeDirectory = GetPlatformIncludeDirectory(ShaderPlatform);
			if (PlatformIncludeDirectory.IsEmpty() || !(VirtualFilePath.Contains(PlatformIncludeDirectory)))
			{
				return true;
			}
		}

		return false;
	}

	void Empty()
	{
		for (auto& Platform : Platforms)
		{
			Platform.ShaderHashCache.Reset();
		}
	}

	const FString& GetPlatformIncludeDirectory(EShaderPlatform ShaderPlatform) const
	{
		check(ShaderPlatform < EShaderPlatform::SP_NumPlatforms);
		checkf(bInitialized, TEXT("GShaderHashCache::Initialize needs to be called before GShaderHashCache::GetPlatformIncludeDirectory."));

		return Platforms[ShaderPlatform].IncludeDirectory;
	}

private:

	struct FPlatform
	{
		/** Folder with platform specific shader files. */
		FString IncludeDirectory;

		/** The shader file hash cache, used to minimize loading and hashing shader files; it includes also hashes for multiple filenames
		by making the key the concatenated list of filenames.
		*/
		TMap<FString, FSHAHash> ShaderHashCache;
	};

	FPlatform Platforms[EShaderPlatform::SP_NumPlatforms];
	bool bInitialized;	
};


static FSCWErrorCode::ECode GSCWErrorCode = FSCWErrorCode::NotSet;
static FString GSCWErrorCodeInfo;

void FSCWErrorCode::Report(ECode Code, const FStringView& Info)
{
	GSCWErrorCode = Code;
	GSCWErrorCodeInfo = Info;
}

void FSCWErrorCode::Reset()
{
	GSCWErrorCode = FSCWErrorCode::NotSet;
	GSCWErrorCodeInfo.Empty();
}

FSCWErrorCode::ECode FSCWErrorCode::Get()
{
	return GSCWErrorCode;
}

const FString& FSCWErrorCode::GetInfo()
{
	return GSCWErrorCodeInfo;
}

bool FSCWErrorCode::IsSet()
{
	return GSCWErrorCode != FSCWErrorCode::NotSet;
}

/** Protects GShaderHashCache from simultaneous modification by multiple threads. Note that it can cover more than one method of the class, e.g. a block of code doing Find() then Add() can be guarded */
FRWLock GShaderHashAccessRWLock;

FShaderHashCache GShaderHashCache;

/** Global map of virtual file path to physical file paths */
static TMap<FString, FString> GShaderSourceDirectoryMappings;

static TAutoConsoleVariable<int32> CVarForceDebugViewModes(
	TEXT("r.ForceDebugViewModes"),
	0,
	TEXT("0: Setting has no effect.\n")
	TEXT("1: Forces debug view modes to be available, even on cooked builds.")
	TEXT("2: Forces debug view modes to be unavailable, even on editor builds.  Removes many shader permutations for faster shader iteration."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

int32 GShaderCompilerDumpCompileJobInputs = 0;
static FAutoConsoleVariableRef CVarShaderCompilerDumpCompileJobInputs(
	TEXT("r.ShaderCompiler.DumpCompileJobInputs"),
	GShaderCompilerDumpCompileJobInputs,
	TEXT("if != 0, unpreprocessed input of the shader compiler jobs will be dumped into the debug directory for closer inspection. This is a debugging feature which is disabled by default."),
	ECVF_Default
);

/** Returns true if debug viewmodes are allowed for the current platform. */
bool AllowDebugViewmodes()
{	
	int32 ForceDebugViewValue = CVarForceDebugViewModes.GetValueOnAnyThread();

	// To use debug viewmodes on consoles, r.ForceDebugViewModes must be set to 1 in ConsoleVariables.ini
	// And EngineDebugMaterials must be in the StartupPackages for the target platform.

	// Force enabled: r.ForceDebugViewModes 1
	const bool bForceEnable = ForceDebugViewValue == 1;
	if (bForceEnable)
	{
		return true;
	}

	// Force disabled: r.ForceDebugViewModes 2
	const bool bForceDisable = ForceDebugViewValue == 2;
	if (bForceDisable)
	{
		return false;
	}

	// Disable when running a commandlet without -AllowCommandletRendering
	if (IsRunningCommandlet() && !IsAllowCommandletRendering())
	{
		return false;
	}

	// Disable if we require cooked data
	if (FPlatformProperties::RequiresCookedData())
	{
		return false;
	}

	return true;
}

/** Returns true if debug viewmodes are allowed for the current platform. */
bool AllowDebugViewmodes(EShaderPlatform Platform)
{
#if WITH_EDITOR
	const int32 ForceDebugViewValue = CVarForceDebugViewModes.GetValueOnAnyThread();
	bool bForceEnable = ForceDebugViewValue == 1;
	bool bForceDisable = ForceDebugViewValue == 2;

	#if 0
		// We can't distinguish between editor and non-editor targets solely from EShaderPlatform.
		// RequiresCookedData() was always returning true for Windows in UE 4, and false in UE 5, for Windows
		TStringBuilder<64> PlatformName;
		ShaderPlatformToPlatformName(Platform).ToString(PlatformName);
		ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatform(PlatformName);
		return (!bForceDisable) && (bForceEnable || !TargetPlatform || !TargetPlatform->RequiresCookedData());
	#else
		// Always include debug shaders for Windows targets until we have a way to distinguish
		return (!bForceDisable) && (bForceEnable || IsPCPlatform(Platform));
	#endif

#else
	return AllowDebugViewmodes();
#endif
}

struct FShaderSettingHelper
{
	const IConsoleVariable* const SettingCVar;
#if WITH_EDITOR
	const TCHAR* const SettingSection;
	const TCHAR* const SettingSectionBuildMachine;
	const TCHAR* const SettingName;
#endif

	FShaderSettingHelper() = delete;
	FShaderSettingHelper(
		const TCHAR* InSettingSection,
		const TCHAR* InSettingSectionBuildMachine,
		const TCHAR* InSettingName,
		const IConsoleVariable* InSettingCVar)
		: SettingCVar(InSettingCVar)
#if WITH_EDITOR
		, SettingSection(InSettingSection)
		, SettingSectionBuildMachine(InSettingSectionBuildMachine)
		, SettingName(InSettingName)
#endif
	{
	}

#if WITH_EDITOR
	static FConfigCacheIni* GetPlatformConfigForFormat(const FName ShaderFormat)
	{
		ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatformWithSupport(TEXT("ShaderFormat"), ShaderFormat);
		return TargetPlatform ? TargetPlatform->GetConfigSystem() : nullptr;
	}
#endif

	bool GetBoolForPlatform(FName ShaderFormat) const
	{
		bool bEnabled = false;

		// First check the global cvar
		if (SettingCVar && SettingCVar->GetInt())
		{
			bEnabled = true;
		}
#if WITH_EDITOR
		// Then check the per platform settings.
		else if (FConfigCacheIni* PlatformConfig = GetPlatformConfigForFormat(ShaderFormat))
		{
			// Override with a build machine specific setting, if present.
			bool bFoundConfig = false;
			if (GIsBuildMachine && SettingSectionBuildMachine)
			{
				bFoundConfig = PlatformConfig->GetValue(SettingSectionBuildMachine, SettingName, bEnabled, GEngineIni);
			}
			if (!bFoundConfig)
			{
				PlatformConfig->GetValue(SettingSection, SettingName, bEnabled, GEngineIni);
			}
		}
#endif

		return bEnabled;
	}

	int32 GetIntForPlatform(FName ShaderFormat) const
	{
		int32 Value = false;

		// First check the global cvar
		if (SettingCVar && SettingCVar->GetInt())
		{
			Value = SettingCVar->GetInt();
		}
#if WITH_EDITOR
		// Then check the per platform settings.
		else if (FConfigCacheIni* PlatformConfig = GetPlatformConfigForFormat(ShaderFormat))
		{
			// Override with a build machine specific setting, if present.
			bool bFoundConfig = false;
			if (GIsBuildMachine && SettingSectionBuildMachine)
			{
				bFoundConfig = PlatformConfig->GetValue(SettingSectionBuildMachine, SettingName, Value, GEngineIni);
			}
			if (!bFoundConfig)
			{
				PlatformConfig->GetValue(SettingSection, SettingName, Value, GEngineIni);
			}
		}
#endif

		return Value;
	}

	bool GetStringForPlatform(FString& OutputString, FName ShaderFormat) const
	{
		// First check the global cvar
		if (SettingCVar)
		{
			OutputString = SettingCVar->GetString();
		}

#if WITH_EDITOR
		if (OutputString.IsEmpty())
		{
			if (FConfigCacheIni* PlatformConfig = GetPlatformConfigForFormat(ShaderFormat))
			{
				// Override with a build machine specific setting, if present.
				if (GIsBuildMachine && SettingSectionBuildMachine)
				{
					OutputString = PlatformConfig->GetStr(SettingSectionBuildMachine, SettingName, GEngineIni);
				}
				if (OutputString.IsEmpty())
				{
					OutputString = PlatformConfig->GetStr(SettingSection, SettingName, GEngineIni);
				}
			}
		}
#endif

		return !OutputString.IsEmpty();
	}
};

struct FShaderSymbolSettingHelper : public FShaderSettingHelper
{
public:
	FShaderSymbolSettingHelper(const TCHAR* InSettingName, bool bPlatformOnly = false)
		: FShaderSettingHelper(TEXT("ShaderCompiler"), TEXT("ShaderCompiler_BuildMachine"), InSettingName, !bPlatformOnly ? IConsoleManager::Get().FindConsoleVariable(InSettingName) : nullptr)
	{
		check(SettingCVar || bPlatformOnly);
	}

	bool IsEnabled(FName ShaderFormat) const
	{
		return GetBoolForPlatform(ShaderFormat);
	}

	bool GetString(FString& OutString, FName ShaderFormat) const
	{
		return GetStringForPlatform(OutString, ShaderFormat);
	}
};

inline ERHIBindlessConfiguration GetBindlessConfiguration(FName ShaderFormat, const TCHAR* SettingName, IConsoleVariable* CVar)
{
	const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormat);

	FString SettingValue;
#if WITH_EDITOR
	if (FConfigCacheIni* PlatformConfig = FShaderSettingHelper::GetPlatformConfigForFormat(ShaderFormat))
	{
		const FString ShaderFormatStr = ShaderFormat.ToString();
		SettingValue = PlatformConfig->GetStr(*ShaderFormatStr, SettingName, GEngineIni);
	}
#endif

	FString CVarString;
	if (CVar)
	{
		CVar->GetValue(CVarString);
	}

	return RHIParseBindlessConfiguration(ShaderPlatform, SettingValue, CVarString);
}

ERHIBindlessConfiguration UE::ShaderCompiler::GetBindlessResourcesConfiguration(FName ShaderFormat)
{
	static IConsoleVariable* const BindlessResourcesCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("rhi.Bindless.Resources"));
	return GetBindlessConfiguration(ShaderFormat, TEXT("BindlessResources"), BindlessResourcesCVar);
}

ERHIBindlessConfiguration UE::ShaderCompiler::GetBindlessSamplersConfiguration(FName ShaderFormat)
{
	static IConsoleVariable* const BindlessSamplersCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("rhi.Bindless.Samplers"));
	return GetBindlessConfiguration(ShaderFormat, TEXT("BindlessSamplers"), BindlessSamplersCVar);
}

bool ShouldGenerateShaderSymbols(FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper Symbols(TEXT("r.Shaders.Symbols"));
	static const FShaderSymbolSettingHelper GenerateSymbols(TEXT("r.Shaders.GenerateSymbols"), true);
	return Symbols.IsEnabled(ShaderFormat) || GenerateSymbols.IsEnabled(ShaderFormat);
}

bool ShouldGenerateShaderSymbolsInfo(FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper SymbolsInfo(TEXT("r.Shaders.SymbolsInfo"));
	return SymbolsInfo.IsEnabled(ShaderFormat);
}

bool ShouldWriteShaderSymbols(FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper Symbols(TEXT("r.Shaders.Symbols"));
	static const FShaderSymbolSettingHelper WriteSymbols(TEXT("r.Shaders.WriteSymbols"), true);
	return Symbols.IsEnabled(ShaderFormat) || WriteSymbols.IsEnabled(ShaderFormat);
}

bool ShouldAllowUniqueShaderSymbols(FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper AllowUniqueSymbols(TEXT("r.Shaders.AllowUniqueSymbols"));
	return AllowUniqueSymbols.IsEnabled(ShaderFormat);
}

bool GetShaderSymbolPathOverride(FString& OutPathOverride, FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper SymbolPathOverride(TEXT("r.Shaders.SymbolPathOverride"));
	if (SymbolPathOverride.GetString(OutPathOverride, ShaderFormat))
	{
		if (!OutPathOverride.IsEmpty())
		{
			// Allow the user to specify the location of the per-platform string.
			OutPathOverride = OutPathOverride.Replace(TEXT("{Platform}"), *ShaderFormat.ToString(), ESearchCase::IgnoreCase);
			// Allow the user to specify the location of the per-project string.
			OutPathOverride = OutPathOverride.Replace(TEXT("{ProjectDir}"), *FPaths::ProjectDir(), ESearchCase::IgnoreCase);
			// Allow the user to specify the location of the per-project saved folder string.
			OutPathOverride = OutPathOverride.Replace(TEXT("{ProjectSavedDir}"), *FPaths::ProjectSavedDir(), ESearchCase::IgnoreCase);
		}
		return !OutPathOverride.IsEmpty();
	}
	return false;
}

bool ShouldWriteShaderSymbolsAsZip(FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper WriteSymbolsZip(TEXT("r.Shaders.WriteSymbols.Zip"));
	return WriteSymbolsZip.IsEnabled(ShaderFormat);
}

bool ShouldEnableExtraShaderData(FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper ExtraData(TEXT("r.Shaders.ExtraData"));
	return ExtraData.IsEnabled(ShaderFormat);
}

bool ShouldOptimizeShaders(FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper Optimize(TEXT("r.Shaders.Optimize"));
	return Optimize.IsEnabled(ShaderFormat);
}

#ifndef UE_ALLOW_SHADER_COMPILING
// is shader compiling allowed at all? (set to 0 in a cooked editor .Target.cs if the target has no shaders available)
#define UE_ALLOW_SHADER_COMPILING 1
#endif

#ifndef UE_ALLOW_SHADER_COMPILING_BASED_ON_SHADER_DIRECTORY_EXISTENCE
// should ability to compile shaders be based on the presence of Engine/Shaders directory?
#define UE_ALLOW_SHADER_COMPILING_BASED_ON_SHADER_DIRECTORY_EXISTENCE 0
#endif

bool AllowShaderCompiling()
{
#if UE_ALLOW_SHADER_COMPILING_BASED_ON_SHADER_DIRECTORY_EXISTENCE
	static bool bShaderDirectoryExists = FPaths::DirectoryExists(FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders"), TEXT("Public")));
	// if it doesn't exist, dont allow compiling. otherwise, check the other flags to see if those have disabled it
	if (!bShaderDirectoryExists)
	{
		return false;
	}
#endif

	static const bool bNoShaderCompile = FParse::Param(FCommandLine::Get(), TEXT("NoShaderCompile")) ||
		FParse::Param(FCommandLine::Get(), TEXT("PrecompiledShadersOnly"));

	return UE_ALLOW_SHADER_COMPILING && !bNoShaderCompile;
}

// note that when UE_ALLOW_SHADER_COMPILING is false, we still need to load the global shaders, so that is the difference in these two functions
bool AllowGlobalShaderLoad()
{
	static const bool bNoShaderCompile = FParse::Param(FCommandLine::Get(), TEXT("NoShaderCompile"));

	// Commandlets and dedicated servers don't load global shaders (the cook commandlet will load for the necessary target platform(s) later).
	return !bNoShaderCompile && !IsRunningDedicatedServer() && (!IsRunningCommandlet() || IsAllowCommandletRendering());

}

TOptional<FParameterAllocation> FShaderParameterMap::FindParameterAllocation(const FString& ParameterName) const
{
	if (const FParameterAllocation* Allocation = ParameterMap.Find(ParameterName))
	{
		if (Allocation->bBound)
		{
			// Can detect copy-paste errors in binding parameters.  Need to fix all the false positives before enabling.
			//UE_LOG(LogShaders, Warning, TEXT("Parameter %s was bound multiple times. Code error?"), ParameterName);
		}

		Allocation->bBound = true;

		return TOptional<FParameterAllocation>(*Allocation);
	}

	return TOptional<FParameterAllocation>();
}

bool FShaderParameterMap::FindParameterAllocation(const TCHAR* ParameterName, uint16& OutBufferIndex, uint16& OutBaseIndex, uint16& OutSize) const
{
	if (TOptional<FParameterAllocation> Allocation = FindParameterAllocation(ParameterName))
	{
		OutBufferIndex = Allocation->BufferIndex;
		OutBaseIndex = Allocation->BaseIndex;
		OutSize = Allocation->Size;

		return true;
	}

	return false;
}

bool FShaderParameterMap::ContainsParameterAllocation(const TCHAR* ParameterName) const
{
	return ParameterMap.Find(ParameterName) != NULL;
}

void FShaderParameterMap::AddParameterAllocation(const TCHAR* ParameterName,uint16 BufferIndex,uint16 BaseIndex,uint16 Size,EShaderParameterType ParameterType)
{
	check(ParameterType < EShaderParameterType::Num);
	ParameterMap.Add(ParameterName, FParameterAllocation(BufferIndex, BaseIndex, Size, ParameterType));
}

void FShaderParameterMap::RemoveParameterAllocation(const TCHAR* ParameterName)
{
	ParameterMap.Remove(ParameterName);
}

TArray<FString> FShaderParameterMap::GetAllParameterNamesOfType(EShaderParameterType InType) const
{
	TArray<FString> Result;
	for (const TMap<FString, FParameterAllocation>::ElementType& Parameter : ParameterMap)
	{
		if (Parameter.Value.Type == InType)
		{
			Result.Emplace(Parameter.Key);
		}
	}
	return Result;
}

void FShaderResourceTableMap::Append(const FShaderResourceTableMap& Other)
{
	// Get the set of uniform buffers used by the target resource table map
	TSet<FString> UniformBufferNames;
	FStringView PreviousUniformBufferName;

	for (const FUniformResourceEntry& Resource : Resources)
	{
		// Cheaper to check if consecutive array elements are from the same uniform buffer (which is common) before adding to set,
		// which involves a more expensive hash lookup versus a string comparison.
		if (!PreviousUniformBufferName.Equals(Resource.GetUniformBufferName(), ESearchCase::CaseSensitive))
		{
			PreviousUniformBufferName = Resource.GetUniformBufferName();
			UniformBufferNames.Add(FString(PreviousUniformBufferName));
		}
	}

	// Then add any entries from "Other" that aren't from a uniform buffer we already include.
	PreviousUniformBufferName = FStringView();
	bool PreviousUniformBufferFound = false;
	for (const FUniformResourceEntry& OtherResource : Other.Resources)
	{
		if (!PreviousUniformBufferName.Equals(OtherResource.GetUniformBufferName(), ESearchCase::CaseSensitive))
		{
			PreviousUniformBufferName = OtherResource.GetUniformBufferName();
			PreviousUniformBufferFound = UniformBufferNames.Find(FString(PreviousUniformBufferName)) != nullptr;
		}

		if (!PreviousUniformBufferFound)
		{
			Resources.Add(OtherResource);
		}
	}
}

void FShaderResourceTableMap::FixupOnLoad(const TMap<FString, FUniformBufferEntry>& UniformBufferMap)
{
	// Need to fix up UniformBufferMemberName string pointers to point into the MemberNameBuffer storage in UniformBufferMap
	uint16 ResourceIndex = 0;
	for (const auto& Pair : UniformBufferMap)
	{
		const TArray<TCHAR>* MemberNameBuffer = Pair.Value.MemberNameBuffer.Get();
		if (MemberNameBuffer && MemberNameBuffer->Num())
		{
			const TCHAR* MemberNameCurrent = MemberNameBuffer->GetData();
			const TCHAR* MemberNameEnd = MemberNameCurrent + MemberNameBuffer->Num();

			for (; MemberNameCurrent < MemberNameEnd; MemberNameCurrent += FCString::Strlen(MemberNameCurrent) + 1)
			{
				Resources[ResourceIndex++].UniformBufferMemberName = MemberNameCurrent;
			}
		}
	}
}

FShaderCompilerEnvironment::FShaderCompilerEnvironment()
{
	// Enable initial defines in FShaderCompilerEnvironment to improve performance (helpful here, but not for defines declared in various shader compiler backends).
	const bool bIncludeInitialDefines = true;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	Definitions = MakePimpl<FShaderCompilerDefinitions, EPimplPtrMode::DeepCopy>(bIncludeInitialDefines);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Presize to reduce re-hashing while building shader jobs
	IncludeVirtualPathToContentsMap.Empty(15);
}

/** Initialization constructor. */
PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
FShaderCompilerEnvironment::FShaderCompilerEnvironment(const FShaderCompilerDefinitions& InDefinitions)
{
	Definitions = MakePimpl<FShaderCompilerDefinitions, EPimplPtrMode::DeepCopy>(InDefinitions);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FShaderCompilerEnvironment::Merge(const FShaderCompilerEnvironment& Other)
{
	// Merge the include maps
	// Merge the values of any existing keys
	for (TMap<FString, FString>::TConstIterator It(Other.IncludeVirtualPathToContentsMap); It; ++It)
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

	check(Other.IncludeVirtualPathToSharedContentsMap.Num() == 0);

	CompilerFlags.Append(Other.CompilerFlags);
	ResourceTableMap.Append(Other.ResourceTableMap);
	UniformBufferMap.Append(Other.UniformBufferMap);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS	// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	Definitions->Merge(*Other.Definitions);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	CompileArgs.Append(Other.CompileArgs);
	RenderTargetOutputFormatsMap.Append(Other.RenderTargetOutputFormatsMap);
	FullPrecisionInPS |= Other.FullPrecisionInPS;
}

FString FShaderCompilerEnvironment::GetDefinitionsAsCommentedCode() const
{
	TArray<FString> DefinesLines;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	DefinesLines.Reserve(Definitions->Num());
	for (FShaderCompilerDefinitions::FConstIterator DefineIt(*Definitions); DefineIt; ++DefineIt)
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
		DefinesLines.Add(FString::Printf(TEXT("// #define %s %s\n"), DefineIt.Key(), DefineIt.Value()));
	}
	DefinesLines.Sort();

	FString Defines;
	for (const FString& DefineLine : DefinesLines)
	{
		Defines += DefineLine;
	}

	return MakeInjectedShaderCodeBlock(TEXT("DumpShaderDefinesAsCommentedCode"), Defines);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then

// Pass through functions to definitions
void FShaderCompilerEnvironment::SetDefine(const TCHAR* Name, const TCHAR* Value)	{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(const TCHAR* Name, const FString& Value)	{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(const TCHAR* Name, uint32 Value)			{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(const TCHAR* Name, int32 Value)			{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(const TCHAR* Name, bool Value)			{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(const TCHAR* Name, float Value)			{ Definitions->SetDefine(Name, Value); }

void FShaderCompilerEnvironment::SetDefine(FName Name, const TCHAR* Value)		{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FName Name, const FString& Value)	{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FName Name, uint32 Value)			{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FName Name, int32 Value)				{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FName Name, bool Value)				{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FName Name, float Value)				{ Definitions->SetDefine(Name, Value); }

void FShaderCompilerEnvironment::SetDefine(FShaderCompilerDefineNameCache& Name, const TCHAR* Value)	{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FShaderCompilerDefineNameCache& Name, const FString& Value)	{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FShaderCompilerDefineNameCache& Name, uint32 Value)			{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FShaderCompilerDefineNameCache& Name, int32 Value)			{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FShaderCompilerDefineNameCache& Name, bool Value)			{ Definitions->SetDefine(Name, Value); }
void FShaderCompilerEnvironment::SetDefine(FShaderCompilerDefineNameCache& Name, float Value)			{ Definitions->SetDefine(Name, Value); }

int32 FShaderCompilerEnvironment::GetIntegerValue(FName Name) const
{
	return Definitions->GetIntegerValue(Name);
}

int32 FShaderCompilerEnvironment::GetIntegerValue(FShaderCompilerDefineNameCache& NameCache, int32 ResultIfNotFound) const
{
	return Definitions->GetIntegerValue(NameCache, ResultIfNotFound);
}

bool FShaderCompilerEnvironment::ContainsDefinition(FName Name) const
{
	return Definitions->Contains(Name);
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

/** This "core" serialization is also used for the hashing the compiler job (where files are handled differently). Should stay in sync with the ShaderCompileWorker. */
void FShaderCompilerEnvironment::SerializeEverythingButFiles(FArchive& Ar)
{
	Ar << *Definitions;
	Ar << CompileArgs;
	Ar << CompilerFlags;
	Ar << RenderTargetOutputFormatsMap;
	Ar << ResourceTableMap.Resources;
	Ar << UniformBufferMap;
	Ar << FullPrecisionInPS;
	if (Ar.IsLoading())
	{
		ResourceTableMap.FixupOnLoad(UniformBufferMap);
	}
}

// Serializes the portions of the environment that are used as input to the backend compilation process (i.e. after all preprocessing)
void FShaderCompilerEnvironment::SerializeCompilationDependencies(FArchive& Ar)
{
	Ar << CompileArgs;
	Ar << CompilerFlags;
	Ar << RenderTargetOutputFormatsMap;
	Ar << ResourceTableMap.Resources;
	Ar << UniformBufferMap;
	Ar << FullPrecisionInPS;
	if (Ar.IsLoading())
	{
		ResourceTableMap.FixupOnLoad(UniformBufferMap);
	}
}

void FShaderCompilerOutput::GenerateOutputHash()
{
	FSHA1 HashState;
	
	const TArray<uint8>& Code = ShaderCode.GetReadAccess();

	// we don't hash the optional attachments as they would prevent sharing (e.g. many materials share the same VS)
	uint32 ShaderCodeSize = ShaderCode.GetShaderCodeSize();

	// make sure we are not generating the hash on compressed data
	checkf(!ShaderCode.IsCompressed(), TEXT("Attempting to generate the output hash of a compressed code"));

	HashState.Update(Code.GetData(), ShaderCodeSize * Code.GetTypeSize());
	ParameterMap.UpdateHash(HashState);
	HashState.Final();
	HashState.GetHash(&OutputHash.Hash[0]);
}

void FShaderCompilerOutput::CompressOutput(FName ShaderCompressionFormat, FOodleDataCompression::ECompressor OodleCompressor, FOodleDataCompression::ECompressionLevel OodleLevel)
{
	// make sure the hash has been generated
	checkf(OutputHash != FSHAHash(), TEXT("Output hash must be generated before compressing the shader code."));
	checkf(ShaderCompressionFormat != NAME_None, TEXT("Compression format should be valid"));
	ShaderCode.Compress(ShaderCompressionFormat, OodleCompressor, OodleLevel);
}

void FShaderCompilerOutput::SerializeShaderCodeValidation()
{
	if (ParametersStrideToValidate.Num() > 0 || ParametersSRVTypeToValidate.Num() > 0 ||
		ParametersUAVTypeToValidate.Num() > 0 || ParametersUBSizeToValidate.Num() > 0)
	{
		FShaderCodeValidationExtension ShaderCodeValidationExtension;

		ShaderCodeValidationExtension.ShaderCodeValidationStride.Append(ParametersStrideToValidate);
		ShaderCodeValidationExtension.ShaderCodeValidationStride.Sort([](const FShaderCodeValidationStride& lhs, const FShaderCodeValidationStride& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; });

		ShaderCodeValidationExtension.ShaderCodeValidationSRVType.Append(ParametersSRVTypeToValidate);
		ShaderCodeValidationExtension.ShaderCodeValidationSRVType.Sort([](const FShaderCodeValidationType& lhs, const FShaderCodeValidationType& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; });

		ShaderCodeValidationExtension.ShaderCodeValidationUAVType.Append(ParametersUAVTypeToValidate);
		ShaderCodeValidationExtension.ShaderCodeValidationUAVType.Sort([](const FShaderCodeValidationType& lhs, const FShaderCodeValidationType& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; });

		ShaderCodeValidationExtension.ShaderCodeValidationUBSize.Append(ParametersUBSizeToValidate);
		ShaderCodeValidationExtension.ShaderCodeValidationUBSize.Sort([](const FShaderCodeValidationUBSize& lhs, const FShaderCodeValidationUBSize& rhs) -> bool { return lhs.BindPoint < rhs.BindPoint; });

		TArray<uint8> WriterBytes;
		FMemoryWriter Writer(WriterBytes);
		Writer << ShaderCodeValidationExtension;

		ShaderCode.AddOptionalData(FShaderCodeValidationExtension::Key, WriterBytes.GetData(), WriterBytes.Num());
	}
}

void FShaderCompilerOutput::SerializeShaderDiagnosticData()
{
	if (ShaderDiagnosticDatas.Num() > 0)
	{
		FShaderDiagnosticExtension ShaderDiagnosticExtension;
		ShaderDiagnosticExtension.ShaderDiagnosticDatas = ShaderDiagnosticDatas;

		TArray<uint8> WriterBytes;
		FMemoryWriter Writer(WriterBytes);
		Writer << ShaderDiagnosticExtension;
		ShaderCode.AddOptionalData(FShaderDiagnosticExtension::Key, WriterBytes.GetData(), WriterBytes.Num());
	}
}

static void ReportVirtualShaderFilePathError(TArray<FShaderCompilerError>* CompileErrors, FString ErrorString)
{
	if (CompileErrors)
	{
		CompileErrors->Add(FShaderCompilerError(*ErrorString));
	}

	UE_LOG(LogShaders, Error, TEXT("%s"), *ErrorString);
}


static bool Contains(FStringView View, FStringView Search)
{
	return UE::String::FindFirst(View, Search) != INDEX_NONE;
}

bool CheckVirtualShaderFilePath(FStringView VirtualFilePath, TArray<FShaderCompilerError>* CompileErrors /*= nullptr*/)
{
	bool bSuccess = true;

	if (!VirtualFilePath.StartsWith('/'))
	{
		FString Error = FString::Printf(TEXT("Virtual shader source file name \"%s\" should be absolute from the virtual root directory \"/\"."), *FString(VirtualFilePath));
		ReportVirtualShaderFilePathError(CompileErrors, Error);
		bSuccess = false;
	}

	if (Contains(VirtualFilePath, TEXTVIEW("..")))
	{
		FString Error = FString::Printf(TEXT("Virtual shader source file name \"%s\" should have relative directories (\"../\") collapsed."), *FString(VirtualFilePath));
		ReportVirtualShaderFilePathError(CompileErrors, Error);
		bSuccess = false;
	}

	if (Contains(VirtualFilePath, TEXTVIEW("\\")))
	{
		FString Error = FString::Printf(TEXT("Backslashes are not permitted in virtual shader source file name \"%s\""), *FString(VirtualFilePath));
		ReportVirtualShaderFilePathError(CompileErrors, Error);
		bSuccess = false;
	}

	FStringView Extension = FPathViews::GetExtension(VirtualFilePath);
	if (VirtualFilePath.StartsWith(TEXT("/Engine/Shared/")))
	{
		if ((Extension != TEXTVIEW("h")))
		{
			FString Error = FString::Printf(TEXT("Extension on virtual shader source file name \"%s\" is wrong. Only .h is allowed for shared headers that are shared between C++ and shader code."), *FString(VirtualFilePath));
			ReportVirtualShaderFilePathError(CompileErrors, Error);
			bSuccess = false;
		}	
	}
	else if (VirtualFilePath.StartsWith(TEXT("/ThirdParty/")))
	{
		// Third party includes don't have naming convention restrictions
	}
	else
	{
		if ((Extension != TEXT("usf") && Extension != TEXT("ush")) || VirtualFilePath.EndsWith(TEXT(".usf.usf")))
		{
			FString Error = FString::Printf(TEXT("Extension on virtual shader source file name \"%s\" is wrong. Only .usf or .ush allowed."), *FString(VirtualFilePath));
			ReportVirtualShaderFilePathError(CompileErrors, Error);
			bSuccess = false;
		}
	}

	return bSuccess;
}

const IShaderFormat* FindShaderFormat(FName Format, const TArray<const IShaderFormat*>& ShaderFormats)
{
	for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
	{
		TArray<FName> Formats;

		ShaderFormats[Index]->GetSupportedFormats(Formats);

		for (int32 FormatIndex = 0; FormatIndex < Formats.Num(); FormatIndex++)
		{
			if (Formats[FormatIndex] == Format)
			{
				return ShaderFormats[Index];
			}
		}
	}

	return nullptr;
}

#if PLATFORM_WINDOWS
static bool ExceptionCodeToString(DWORD ExceptionCode, FString& OutStr)
{
#define EXCEPTION_CODE_CASE_STR(CODE) case CODE: OutStr = TEXT(#CODE); return true;
	const DWORD CPlusPlusExceptionCode = 0xE06D7363;
	switch (ExceptionCode)
	{
		EXCEPTION_CODE_CASE_STR(EXCEPTION_ACCESS_VIOLATION);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_BREAKPOINT);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_DATATYPE_MISALIGNMENT);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_FLT_DENORMAL_OPERAND);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_FLT_DIVIDE_BY_ZERO);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_FLT_INEXACT_RESULT);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_FLT_INVALID_OPERATION);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_FLT_OVERFLOW);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_FLT_STACK_CHECK);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_FLT_UNDERFLOW);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_GUARD_PAGE);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_ILLEGAL_INSTRUCTION);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_IN_PAGE_ERROR);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_INT_DIVIDE_BY_ZERO);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_INT_OVERFLOW);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_INVALID_DISPOSITION);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_INVALID_HANDLE);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_NONCONTINUABLE_EXCEPTION);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_PRIV_INSTRUCTION);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_SINGLE_STEP);
		EXCEPTION_CODE_CASE_STR(EXCEPTION_STACK_OVERFLOW);
		EXCEPTION_CODE_CASE_STR(STATUS_UNWIND_CONSOLIDATE);
		case CPlusPlusExceptionCode: OutStr = TEXT("CPP_EXCEPTION"); return true;
		default: return false;
	}
#undef EXCEPTION_CODE_CASE_STR
}

int HandleShaderCompileException(Windows::LPEXCEPTION_POINTERS Info, FString& OutExMsg, FString& OutCallStack)
{
	const DWORD AssertExceptionCode = 0x00004000;
	FString ExCodeStr;
	OutCallStack = "";
	if (Info->ExceptionRecord->ExceptionCode == AssertExceptionCode)
	{
		// In the case of an assert the assert handler populates the GErrorHist global.
		// This contains a readable assert message that may be followed by a callstack; so we can use that to populate
		// our message/callstack and save some time as well as getting the properly formatted assert message.
		const TCHAR* CallstackStart = FCString::Strfind(GErrorHist, TEXT("0x"));
		if (CallstackStart && CallstackStart > GErrorHist)
		{
			OutExMsg = FString(CallstackStart - GErrorHist, GErrorHist);
			OutCallStack = CallstackStart;
		}
		else
		{
			OutExMsg = GErrorHist;
		}
	}
	else
	{
		if (ExceptionCodeToString(Info->ExceptionRecord->ExceptionCode, ExCodeStr))
		{
			OutExMsg = FString::Printf(
				TEXT("Exception: %s, address=0x%016x\n"),
				*ExCodeStr,
				(uint64)Info->ExceptionRecord->ExceptionAddress);
		}
		else
		{
			OutExMsg = FString::Printf(
				TEXT("Exception code: 0x%08x, address=0x%016x\n"),
				Info->ExceptionRecord->ExceptionCode,
				(uint64)Info->ExceptionRecord->ExceptionAddress);
		}
	}

	if (OutCallStack.Len() == 0)
	{
		ANSICHAR CallStack[32768];
		FMemory::Memzero(CallStack);
		FPlatformStackWalk::StackWalkAndDump(CallStack, ARRAYSIZE(CallStack), Info->ExceptionRecord->ExceptionAddress);
		OutCallStack = ANSI_TO_TCHAR(CallStack);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif


class FInternalShaderCompilerFunctions
{
public:

	static bool InvokePreprocess(
		const IShaderFormat* Backend,
		const FShaderCompilerInput& Input,
		const FShaderCompilerEnvironment& Environment,
		FShaderPreprocessOutput& Output,
		FString& OutExceptionCallstack,
		FString& OutExceptionMsg)
	{
		return Backend->PreprocessShader(Input, Environment, Output);
	}

	static void PreprocessShaderInternal(
		const IShaderFormat* Backend, 
		FShaderCompileJob& Job, 
		const FShaderCompilerEnvironment& Environment,
		FString& OutExceptionCallstack,
		FString& OutExceptionMsg)
	{
		Job.PreprocessOutput.bSucceeded = InvokePreprocess(Backend, Job.Input, Environment, Job.PreprocessOutput, OutExceptionCallstack, OutExceptionMsg);
		if (Job.PreprocessOutput.bSucceeded && Backend->RequiresSecondaryCompile(Job.Input, Environment, Job.PreprocessOutput))
		{
			Job.SecondaryPreprocessOutput = MakeUnique<FShaderPreprocessOutput>();
			Job.SecondaryPreprocessOutput->bIsSecondary = true;
			Job.PreprocessOutput.bSucceeded &= InvokePreprocess(Backend, Job.Input, Environment, *Job.SecondaryPreprocessOutput, OutExceptionCallstack, OutExceptionMsg);
		}
	}

	static bool PreprocessShaderInternal(const IShaderFormat* Backend, FShaderCompileJob& Job)
	{
		const double StartPreprocessTime = FPlatformTime::Seconds();

		FString ExceptionCallstack;
		FString ExceptionMsg;

		// if preprocessed cache is disabled, this function is executed as part of the compile process
		// which will have merged the environment inline prior to calling this, so we can skip merging here 
		if (Job.Input.bCachePreprocessed && IsValidRef(Job.Input.SharedEnvironment))
		{
			// only create new environment & merge if necessary, save some allocs
			// (need a copy here as we don't want to merge the environment in place like we do in the compile path
			// and affect what is passed to the workers)
			FShaderCompilerEnvironment MergedEnvironment = Job.Input.Environment;
			MergedEnvironment.Merge(*Job.Input.SharedEnvironment);
			PreprocessShaderInternal(Backend, Job, MergedEnvironment, ExceptionCallstack, ExceptionMsg);
		}
		else
		{
			PreprocessShaderInternal(Backend, Job, Job.Input.Environment, ExceptionCallstack, ExceptionMsg);
		}

		if (!Job.PreprocessOutput.bSucceeded && (!ExceptionMsg.IsEmpty() || !ExceptionCallstack.IsEmpty()))
		{
			FString StrippedErrorMessage = FString::Printf(
				TEXT("Exception encountered in platform compiler: %s\nException Callstack:\n%s"),
				*ExceptionMsg,
				*ExceptionCallstack);
			Job.PreprocessOutput.LogError(MoveTemp(StrippedErrorMessage));
		}

		if (Job.PreprocessOutput.bSucceeded && Job.Input.bCachePreprocessed)
		{
			// if the preprocessed job cache is enabled we need to strip the preprocessed code, this removes comments, line directives
			// and blank lines to improve deduplication (and populates data required to remap diagnostic messages to correct line numbers)
			Job.PreprocessOutput.StripCode(Job.Input.NeedsOriginalShaderSource());

			// always compress the code after stripping to minimize memory footprint
			Job.PreprocessOutput.CompressCode();
		}

		Job.PreprocessOutput.ElapsedTime = FPlatformTime::Seconds() - StartPreprocessTime;
		Job.Output.PreprocessTime = Job.PreprocessOutput.ElapsedTime;
		Job.Output.ShaderDiagnosticDatas = Job.PreprocessOutput.GetDiagnosticDatas();
		Job.Output.Errors.Append(Job.PreprocessOutput.Errors);
		return Job.PreprocessOutput.bSucceeded;
	}

	static void CombineOutputs(const IShaderFormat* Compiler, FShaderCompileJob& Job)
	{
		// Pack shader code results together
		// [int32 key][uint32 primary length][uint32 secondary length][full primary shader code][full secondary shader code]
		TArray<uint8> CombinedSource;

		int32 PackedShaderKey = Compiler->GetPackedShaderKey();
		CombinedSource.Append(reinterpret_cast<const uint8*>(&PackedShaderKey), sizeof(PackedShaderKey));

		const uint32 PrimaryLength = Job.Output.ShaderCode.GetReadAccess().Num();
		CombinedSource.Append(reinterpret_cast<const uint8*>(&PrimaryLength), sizeof(PrimaryLength));

		const uint32 SecondaryLength = Job.SecondaryOutput->ShaderCode.GetReadAccess().Num();
		CombinedSource.Append(reinterpret_cast<const uint8*>(&SecondaryLength), sizeof(SecondaryLength));

		CombinedSource.Append(Job.Output.ShaderCode.GetReadAccess());
		CombinedSource.Append(Job.SecondaryOutput->ShaderCode.GetReadAccess());

		// Replace Output shader code with the combined result
		Job.Output.ShaderCode = {};
		TArray<uint8>& FinalShaderCode = Job.Output.ShaderCode.GetWriteAccess();
		FinalShaderCode.Append(CombinedSource);
		Job.Output.ShaderCode.FinalizeShaderCode();
	}

	static void InvokeCompile(const IShaderFormat* Compiler, FShaderCompileJob& Job, const FString& WorkingDirectory, FString& OutExceptionMsg, FString& OutExceptionCallstack)
	{
#if PLATFORM_WINDOWS
		__try
#endif
		{
			if (Job.SecondaryOutput.IsValid())
			{
				check(Job.SecondaryPreprocessOutput.IsValid());
				Compiler->CompilePreprocessedShader(Job.Input, Job.PreprocessOutput, *Job.SecondaryPreprocessOutput, Job.Output, *Job.SecondaryOutput, WorkingDirectory);
			}
			else
			{
				Compiler->CompilePreprocessedShader(Job.Input, Job.PreprocessOutput, Job.Output, WorkingDirectory);
			}
		}
#if PLATFORM_WINDOWS
		__except(HandleShaderCompileException(GetExceptionInformation(), OutExceptionMsg, OutExceptionCallstack))
		{
			Job.Output.bSucceeded = false;
		}
#endif
	}

	static void CompileShaderInternal(const IShaderFormat* Compiler, FShaderCompileJob& Job, const FString& WorkingDirectory, FString& OutExceptionMsg, FString& OutExceptionCallstack, int32* CompileCount)
	{
		double TimeStart = FPlatformTime::Seconds();
		if (!Job.Input.bCachePreprocessed)
		{
			PreprocessShaderInternal(Compiler, Job);
		}

		// decompress if necessary; this is a no-op if source is not compressed.
		Job.PreprocessOutput.DecompressCode();

		if (Job.PreprocessOutput.bSucceeded)
		{
			if (Job.SecondaryPreprocessOutput.IsValid())
			{
				Job.SecondaryOutput = MakeUnique<FShaderCompilerOutput>();
			}
			InvokeCompile(Compiler, Job, WorkingDirectory, OutExceptionMsg, OutExceptionCallstack);
			if (Job.SecondaryOutput.IsValid())
			{
				Job.Output.bSucceeded = Job.Output.bSucceeded && Job.SecondaryOutput->bSucceeded;
				if (Job.Output.bSucceeded)
				{
					Job.SecondaryOutput->GenerateOutputHash();
				}
				CombineOutputs(Compiler, Job);
			}
		}
		else
		{
			Job.Output.bSucceeded = false;
		}

		if (Job.Output.bSucceeded)
		{
			Job.Output.GenerateOutputHash();

			if (Job.Input.CompressionFormat != NAME_None)
			{
				Job.Output.CompressOutput(Job.Input.CompressionFormat, Job.Input.OodleCompressor, Job.Input.OodleLevel);
			}
		}
		Job.Output.CompileTime = FPlatformTime::Seconds() - TimeStart;

		if (CompileCount)
		{
			++(*CompileCount);
		}
	}
};

bool ConditionalPreprocessShader(FShaderCommonCompileJob* Job)
{
	static ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
	if (FShaderCompileJob* SingleJob = Job->GetSingleShaderJob())
	{
		if (SingleJob->Input.bCachePreprocessed)
		{
			const IShaderFormat* ShaderFormat = TargetPlatformManager.FindShaderFormat(SingleJob->Input.ShaderFormat);
			return FInternalShaderCompilerFunctions::PreprocessShaderInternal(ShaderFormat, *SingleJob);
		}
		return true;

	}
	else if (FShaderPipelineCompileJob* PipelineJob = Job->GetShaderPipelineJob())
	{
		bool bAnyFailed = false;
		for (FShaderCompileJob* StageJob : PipelineJob->StageJobs)
		{
			if (StageJob->Input.bCachePreprocessed)
			{
				const IShaderFormat* ShaderFormat = TargetPlatformManager.FindShaderFormat(StageJob->Input.ShaderFormat);

				if (!bAnyFailed)
				{
					bAnyFailed |= !FInternalShaderCompilerFunctions::PreprocessShaderInternal(ShaderFormat, *StageJob);
				}
				else
				{
					// skip subsequent stage preprocessing if a prior stage failed to avoid unnecessary work, but log an error to indicate this
					FString Error = FString::Printf(
						TEXT("Preprocessing %s stage skipped due to earlier stage preprocessing failure."),
						GetShaderFrequencyString(StageJob->Input.Target.GetFrequency()));
					StageJob->Output.Errors.Add(FShaderCompilerError(*Error));
				}
			}
		}
		return !bAnyFailed;
	}

	checkf(0, TEXT("Unknown shader compile job type or bad job pointer"));
	return false;
}

void CompileShader(const TArray<const IShaderFormat*>& ShaderFormats, FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FString& WorkingDirectory, int32* CompileCount)
{
	FShaderCompileJob TempJob;
	TempJob.Input = Input;
	
	CompileShader(ShaderFormats, TempJob, WorkingDirectory, CompileCount);
	Output = TempJob.Output;
}

void CompileShader(const TArray<const IShaderFormat*>& ShaderFormats, FShaderCompileJob& Job, const FString& WorkingDirectory, int32* CompileCount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CompileShader);

	const IShaderFormat* Compiler = FindShaderFormat(Job.Input.ShaderFormat, ShaderFormats);
	if (!Compiler)
	{
		UE_LOG(LogShaders, Fatal, TEXT("Can't compile shaders for format %s, couldn't load compiler dll"), *Job.Input.ShaderFormat.ToString());
	}

	if (IsValidRef(Job.Input.SharedEnvironment))
	{
		Job.Input.Environment.Merge(*Job.Input.SharedEnvironment);
	}
	FString ExceptionMsg;
	FString ExceptionCallstack;
	FInternalShaderCompilerFunctions::CompileShaderInternal(Compiler, Job, WorkingDirectory, ExceptionMsg, ExceptionCallstack, CompileCount);
	if (!Job.Output.bSucceeded && (!ExceptionMsg.IsEmpty() || !ExceptionCallstack.IsEmpty()))
	{
		FShaderCompilerError Error;
		Error.StrippedErrorMessage = FString::Printf(
			TEXT("Exception encountered in platform compiler: %s\nException Callstack:\n%s"), 
			*ExceptionMsg, 
			*ExceptionCallstack);
		Job.Output.Errors.Add(Error);
	}

	Job.bSucceeded = Job.Output.bSucceeded;
	if (Job.Input.DumpDebugInfoEnabled())
	{
		// if the preprocessed cache is disabled, dump debug output here, since we don't serialize preprocess output back to the cooker from SCW
		// (if enabled this will occur in the job OnComplete callback)
		if (!Job.Input.bCachePreprocessed)
		{
			if (Job.SecondaryPreprocessOutput.IsValid() && Job.SecondaryOutput.IsValid())
			{
				Compiler->OutputDebugData(Job.Input, Job.PreprocessOutput, *Job.SecondaryPreprocessOutput, Job.Output, *Job.SecondaryOutput);
			}
			else
			{
				Compiler->OutputDebugData(Job.Input, Job.PreprocessOutput, Job.Output);
			}
		}
	}
}

void CompileShaderPipeline(const TArray<const IShaderFormat*>& ShaderFormats, FShaderPipelineCompileJob* PipelineJob, const FString& Dir, int32* CompileCount)
{
	checkf(PipelineJob->StageJobs.Num() > 0, TEXT("Pipeline %s has zero jobs!"), PipelineJob->Key.ShaderPipeline->GetName());
	FShaderCompileJob* CurrentJob = PipelineJob->StageJobs[0]->GetSingleShaderJob();

	// Flag should be set on the first job when the FShaderPipelineCompileJob was constructed, to ensure the flag is included when computing the input hash.
	check(CurrentJob->Input.bCompilingForShaderPipeline == true);

	// First job doesn't have to trim outputs
	CurrentJob->Input.bIncludeUsedOutputs = false;

	// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
	CompileShader(ShaderFormats, *CurrentJob, Dir, CompileCount);

	CurrentJob->bSucceeded = CurrentJob->Output.bSucceeded;
	if (!CurrentJob->Output.bSucceeded)
	{
		// Can't carry on compiling the pipeline
		return;
	}

	// This tells the shader compiler we do want to remove unused outputs
	bool bEnableRemovingUnused = true;

	//#todo-rco: Currently only removes for pure VS & PS stages
	for (int32 Index = 0; Index < PipelineJob->StageJobs.Num(); ++Index)
	{
		auto Stage = PipelineJob->StageJobs[Index]->GetSingleShaderJob()->Input.Target.Frequency;
		if (Stage != SF_Vertex && Stage != SF_Pixel)
		{
			bEnableRemovingUnused = false;
			break;
		}
	}

	for (int32 Index = 1; Index < PipelineJob->StageJobs.Num(); ++Index)
	{
		auto* PreviousJob = CurrentJob;
		CurrentJob = PipelineJob->StageJobs[Index]->GetSingleShaderJob();
		bEnableRemovingUnused = bEnableRemovingUnused && PreviousJob->Output.bSupportsQueryingUsedAttributes;
		if (bEnableRemovingUnused)
		{
			CurrentJob->Input.bIncludeUsedOutputs = true;
			CurrentJob->Input.bCompilingForShaderPipeline = true;
			CurrentJob->Input.UsedOutputs = PreviousJob->Output.UsedAttributes;
		}

		// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
		CompileShader(ShaderFormats, *CurrentJob, Dir, CompileCount);

		CurrentJob->bSucceeded = CurrentJob->Output.bSucceeded;
		if (!CurrentJob->Output.bSucceeded)
		{
			// Can't carry on compiling the pipeline
			return;
		}
	}

	PipelineJob->bSucceeded = true;
}

static void InternalGetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, bool AddToIncludeFile, const FName* ShaderPlatformName, bool bPreprocessDependencies);

/**
* Add a new entry to the list of shader source files
* Only unique entries which can be loaded are added as well as their #include files
*
* @param OutVirtualFilePaths - [out] list of shader source files to add to
* @param ShaderFilename - shader file to add
*/
void AddShaderSourceFileEntry(TArray<FString>& OutVirtualFilePaths, FString VirtualFilePath, EShaderPlatform ShaderPlatform, const FName* ShaderPlatformName)
{
	check(CheckVirtualShaderFilePath(VirtualFilePath));
	if (!OutVirtualFilePaths.Contains(VirtualFilePath))
	{
		OutVirtualFilePaths.Add(VirtualFilePath);

		TArray<FString> ShaderIncludes;

		const uint32 DepthLimit = 100;
		const bool bPreprocessDependencies = true;
		InternalGetShaderIncludes(*VirtualFilePath, *VirtualFilePath, OutVirtualFilePaths, ShaderPlatform, DepthLimit, false, ShaderPlatformName, bPreprocessDependencies);
		for( int32 IncludeIdx=0; IncludeIdx < ShaderIncludes.Num(); IncludeIdx++ )
		{
			OutVirtualFilePaths.AddUnique(ShaderIncludes[IncludeIdx]);
		}
	}
}

/**
* Generates a list of virtual paths of all shader source that engine needs to load.
*
* @param OutVirtualFilePaths - [out] list of shader source files to add to
*/
void GetAllVirtualShaderSourcePaths(TArray<FString>& OutVirtualFilePaths, EShaderPlatform ShaderPlatform, const FName* ShaderPlatformName)
{
	// add all shader source files for hashing
	for( TLinkedList<FVertexFactoryType*>::TIterator FactoryIt(FVertexFactoryType::GetTypeList()); FactoryIt; FactoryIt.Next() )
	{
		FVertexFactoryType* VertexFactoryType = *FactoryIt;
		if( VertexFactoryType )
		{
			FString ShaderFilename(VertexFactoryType->GetShaderFilename());
			AddShaderSourceFileEntry(OutVirtualFilePaths, ShaderFilename, ShaderPlatform, ShaderPlatformName);
		}
	}
	for( TLinkedList<FShaderType*>::TIterator ShaderIt(FShaderType::GetTypeList()); ShaderIt; ShaderIt.Next() )
	{
		FShaderType* ShaderType = *ShaderIt;
		if(ShaderType)
		{
			FString ShaderFilename(ShaderType->GetShaderFilename());
			AddShaderSourceFileEntry(OutVirtualFilePaths, ShaderFilename, ShaderPlatform, ShaderPlatformName);
		}
	}

	//#todo-rco: No need to loop through Shader Pipeline Types (yet)

	// Always add ShaderVersion.ush, so if shader forgets to include it, it will still won't break DDC.
	AddShaderSourceFileEntry(OutVirtualFilePaths, FString(TEXT("/Engine/Public/ShaderVersion.ush")), ShaderPlatform, ShaderPlatformName);
	AddShaderSourceFileEntry(OutVirtualFilePaths, FString(TEXT("/Engine/Private/MaterialTemplate.ush")), ShaderPlatform, ShaderPlatformName);
	AddShaderSourceFileEntry(OutVirtualFilePaths, FString(TEXT("/Engine/Private/Common.ush")), ShaderPlatform, ShaderPlatformName);
	AddShaderSourceFileEntry(OutVirtualFilePaths, FString(TEXT("/Engine/Private/Definitions.usf")), ShaderPlatform, ShaderPlatformName);
}

/**
* Kick off SHA verification for all shader source files
*/
void VerifyShaderSourceFiles(EShaderPlatform ShaderPlatform)
{
#if WITH_EDITORONLY_DATA
	if (!FPlatformProperties::RequiresCookedData() && AllowShaderCompiling())
	{
		// get the list of shader files that can be used
		TArray<FString> VirtualShaderSourcePaths;
		GetAllVirtualShaderSourcePaths(VirtualShaderSourcePaths, ShaderPlatform);
		FScopedSlowTask SlowTask((float)VirtualShaderSourcePaths.Num());
		for( int32 ShaderFileIdx=0; ShaderFileIdx < VirtualShaderSourcePaths.Num(); ShaderFileIdx++ )
		{
			SlowTask.EnterProgressFrame(1);
			// load each shader source file. This will cache the shader source data after it has been verified
			LoadShaderSourceFile(*VirtualShaderSourcePaths[ShaderFileIdx], ShaderPlatform, nullptr, nullptr);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

static void LogShaderSourceDirectoryMappings()
{
	for (const auto& Iter : GShaderSourceDirectoryMappings)
	{
		UE_LOG(LogShaders, Log, TEXT("Shader directory mapping %s -> %s"), *Iter.Key, *Iter.Value);
	}
}

FString GetShaderSourceFilePath(const FString& VirtualFilePath, TArray<FShaderCompilerError>* CompileErrors)
{
	// Make sure the .usf extension is correctly set.
	if (!CheckVirtualShaderFilePath(VirtualFilePath, CompileErrors))
	{
		return FString();
	}

	// We don't cache the output of this function because only used in LoadShaderSourceFile that is cached, or when there
	// is shader compilation errors.
	
	FString RealFilePath;

	// Look if this virtual shader source file match any directory mapping.
	const TMap<FString, FString>& ShaderSourceDirectoryMappings = GShaderSourceDirectoryMappings;
	FString ParentVirtualDirectoryPath = FPaths::GetPath(VirtualFilePath);
	FString RelativeVirtualDirectoryPath = FPaths::GetCleanFilename(VirtualFilePath);
	while (!ParentVirtualDirectoryPath.IsEmpty())
	{
		if (ShaderSourceDirectoryMappings.Contains(ParentVirtualDirectoryPath))
		{
			RealFilePath = FPaths::Combine(
				*ShaderSourceDirectoryMappings.Find(ParentVirtualDirectoryPath),
				RelativeVirtualDirectoryPath);
			break;
		}

		RelativeVirtualDirectoryPath = FPaths::GetCleanFilename(ParentVirtualDirectoryPath) / RelativeVirtualDirectoryPath;
		ParentVirtualDirectoryPath = FPaths::GetPath(ParentVirtualDirectoryPath);
	}

	// Make sure a directory mapping has matched.
	if (RealFilePath.IsEmpty())
	{
		FString Error = FString::Printf(TEXT("Can't map virtual shader source path \"%s\"."), *VirtualFilePath);
		Error += TEXT("\nDirectory mappings are:");
		for (const auto& Iter : ShaderSourceDirectoryMappings)
		{
			Error += FString::Printf(TEXT("\n  %s -> %s"), *Iter.Key, *Iter.Value);
		}

		ReportVirtualShaderFilePathError(CompileErrors, Error);
	}
	
	return RealFilePath;
}

FString ParseVirtualShaderFilename(const FString& InFilename)
{
	FString ShaderDir = FString(FPlatformProcess::ShaderDir());
	ShaderDir.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	int32 CharIndex = ShaderDir.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, ShaderDir.Len() - 1);
	if (CharIndex != INDEX_NONE)
	{
		ShaderDir.RightInline(ShaderDir.Len() - CharIndex, EAllowShrinking::No);
	}

	FString RelativeFilename = InFilename.Replace(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	// remove leading "/" because this makes path absolute on Linux (and Mac).
	if (RelativeFilename.Len() > 0 && RelativeFilename[0] == TEXT('/'))
	{
		RelativeFilename.RightInline(RelativeFilename.Len() - 1, EAllowShrinking::No);
	}
	RelativeFilename = IFileManager::Get().ConvertToRelativePath(*RelativeFilename);
	CharIndex = RelativeFilename.Find(ShaderDir);
	if (CharIndex != INDEX_NONE)
	{
		CharIndex += ShaderDir.Len();
		if (RelativeFilename.GetCharArray()[CharIndex] == TEXT('/'))
		{
			CharIndex++;
		}
		if (RelativeFilename.Contains(TEXT("WorkingDirectory")))
		{
			const int32 NumDirsToSkip = 3;
			int32 NumDirsSkipped = 0;
			int32 NewCharIndex = CharIndex;

			do
			{
				NewCharIndex = RelativeFilename.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CharIndex);
				CharIndex = (NewCharIndex == INDEX_NONE) ? CharIndex : NewCharIndex + 1;
			}
			while (NewCharIndex != INDEX_NONE && ++NumDirsSkipped < NumDirsToSkip);
		}
		RelativeFilename.MidInline(CharIndex, RelativeFilename.Len() - CharIndex, EAllowShrinking::No);
	}

	// add leading "/" to the relative filename because that's what virtual shader path expects
	FString OutputFilename;
	if (RelativeFilename.Len() > 0 && RelativeFilename[0] != TEXT('/'))
	{
		OutputFilename = TEXT("/") + RelativeFilename;
	}
	else
	{
		OutputFilename = RelativeFilename;
	}
	check(CheckVirtualShaderFilePath(OutputFilename));
	return OutputFilename;
}

bool ReplaceVirtualFilePathForShaderPlatform(FString& InOutVirtualFilePath, EShaderPlatform ShaderPlatform)
{
	// as of 2021-03-01, it'd be safe to access just the include directory without the lock... but the lock (and copy) is here for the consistency's and future-proofness' sake
	const FString PlatformIncludeDirectory( [ShaderPlatform]()
		{
			FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_ReadOnly);
			return GShaderHashCache.GetPlatformIncludeDirectory(ShaderPlatform);
		}()
	);

	if (PlatformIncludeDirectory.IsEmpty())
	{
		return false;
	}

	struct FEntry
	{
		const TCHAR* Prefix;
		const TCHAR* Visibility;
	};
	const FEntry VirtualPlatformPrefixes[] =
	{
		{TEXT("/Platform/Private/"), TEXT("Private")},
		{TEXT("/Platform/Public/"),  TEXT("Public")}
	};

	for (const FEntry& Entry : VirtualPlatformPrefixes)
	{
		if (InOutVirtualFilePath.StartsWith(Entry.Prefix))
		{
			// PlatformIncludeDirectory already contains leading and trailing slash (which we need to remove)
			FString CandidatePath = FString::Printf(TEXT("/Platform%s"), *PlatformIncludeDirectory);
			CandidatePath.RemoveFromEnd(TEXT("/"));

			// If a directory mapping exists for the candidate path, then commit the replacement
			if (GShaderSourceDirectoryMappings.Contains(CandidatePath))
			{
				InOutVirtualFilePath = FString::Printf(TEXT("/Platform%s%s/%s"),
					*PlatformIncludeDirectory, // This already contains leading and trailing slash
					Entry.Visibility,
					*InOutVirtualFilePath.RightChop(FCString::Strlen(Entry.Prefix)));

				return true;
			}
		}
	}

	return false;
}

bool ReplaceVirtualFilePathForShaderAutogen(FString& InOutVirtualFilePath, EShaderPlatform ShaderPlatform, const FName* InShaderPlatformName)
{
	const FStringView ShaderAutogenStem = TEXTVIEW("/Engine/Generated/ShaderAutogen/");

	// Tweak the autogen path
	// for examples, if it starts with "/Engine/Generated/ShaderAutogen/" change it to "ShaderAutogen/PCD3D_SM5/"
	if (!FCString::Strnicmp(*InOutVirtualFilePath, ShaderAutogenStem.GetData(), ShaderAutogenStem.Len()))
	{
		check(FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform) || InShaderPlatformName != nullptr);
		FName ShaderPlatformName = FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform) ? FDataDrivenShaderPlatformInfo::GetName(ShaderPlatform) : *InShaderPlatformName;
		TStringBuilder<1024> OutputShaderName;

		// Append the prefix.
		OutputShaderName.Append(TEXTVIEW("/ShaderAutogen/"));

		// Append the platform name.

		FString PlatformNameString = ShaderPlatformName.ToString();
		OutputShaderName.Append(*PlatformNameString, PlatformNameString.Len());

		OutputShaderName.AppendChar(TEXT('/'));

		// Append the relative name (the substring after "/Engine/Generated/ShaderAutogen/").
		const TCHAR* RelativeShaderName = *InOutVirtualFilePath + ShaderAutogenStem.Len();
		OutputShaderName.Append(RelativeShaderName, InOutVirtualFilePath.Len() - ShaderAutogenStem.Len());

		InOutVirtualFilePath = OutputShaderName;
		return true;
	}

	return false;
}

void FixupShaderFilePath(FString& VirtualFilePath, EShaderPlatform ShaderPlatform, const FName* ShaderPlatformName)
{
	// Always substitute virtual platform path before accessing GShaderFileCache to get platform-specific file.
	ReplaceVirtualFilePathForShaderPlatform(VirtualFilePath, ShaderPlatform);

	// Fixup autogen file
	ReplaceVirtualFilePathForShaderAutogen(VirtualFilePath, ShaderPlatform, ShaderPlatformName);
}

inline bool IsEndOfLine(TCHAR C)
{
	return C == TEXT('\r') || C == TEXT('\n');
}

inline bool CommentStripNeedsHandling(TCHAR C)
{
	return IsEndOfLine(C) || C == TEXT('/') || C == 0;
}

inline int NewlineCharCount(TCHAR First, TCHAR Second)
{
	return ((First + Second) == TEXT('\r') + TEXT('\n')) ? 2 : 1;
}

// Given an FString containing the contents of a shader source file, populates the given array with contents of
// that source file with all comments stripped. This is needed since the STB preprocessor itself does not strip 
// comments.
void ShaderConvertAndStripComments(const FString& ShaderSource, TArray<ANSICHAR>& OutStripped)
{
	// STB preprocessor does not strip comments, so we do so here before returning the loaded source
	// Doing so is barely more costly than the memcopy we require anyways so has negligible overhead.
	// Reserve worst case (i.e. assuming there are no comments at all) to avoid reallocation
	int32 BufferSize = ShaderSource.Len() + 16;		// need extra for null terminator plus padding for SSE read operations at the end of the buffer
	OutStripped.SetNumUninitialized(BufferSize);

	ANSICHAR* CurrentOut = OutStripped.GetData();

	const TCHAR* const Start = ShaderSource.GetCharArray().GetData();
	const TCHAR* const End = Start + ShaderSource.Len();

	// We rely on null termination to avoid the need to check Current < End in some cases
	check(*End == TEXT('\0'));

	const TCHAR* Current = Start;

#if PLATFORM_ALWAYS_HAS_SSE4_2
	__m128i CharCR = _mm_set1_epi8('\r');			// Carriage return
	__m128i CharLF = _mm_set1_epi8('\n');			// Line feed (newline)
	__m128i CharSlash = _mm_set1_epi8('/');
	__m128i CharStar = _mm_set1_epi8('*');

	// We process 15 characters at a time, so we can find comment starts (needs access to pairs of characters)
	const TCHAR* EndSse = End - 16;
	for (; Current < EndSse; )
	{
		__m128i First8 = _mm_loadu_si128((const __m128i*)Current);
		__m128i Second8 = _mm_loadu_si128((const __m128i*)(Current + 8));
		__m128i CurrentWord = _mm_packus_epi16(First8, Second8);

		int32 CRMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharCR));
		int32 SlashMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharSlash));
		int32 StarMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharStar));

		// If we encounter a carriage return, fall back to slower single character path that handles CR/LF combos
		if (CRMask)
		{
			// Go back one character if first character in current word is CR, and previous character was LF, so
			// the single character parser can treat it as a newline pair.
			if ((CRMask & 1) && (Current > Start) && *(Current - 1) == '\n')
			{
				Current--;
				CurrentOut--;
			}
			break;
		}

		// Echo the current word
		_mm_storeu_si128((__m128i*)CurrentOut, CurrentWord);

		// Check if there is a comment start, meaning a slash followed by slash or star, which we can detect by shifting right
		// a mask containing both slash and star, and seeing if that overlaps with a slash.
		int32 CommentStartMask = SlashMask & ((SlashMask | StarMask) >> 1);
		if (!CommentStartMask)
		{
			// If no potential comment start, advance 15 characters and parse again
			CurrentOut += 15;
			Current += 15;
			continue;
		}

		// Advance input to contents of comment, output to end of non-comment characters
		int32 CommentOffset = _tzcnt_u32(CommentStartMask);
		Current += CommentOffset + 2;
		CurrentOut += CommentOffset;

		if (*(Current - 1) == '/')
		{
			// Single line comment, advance to newline
			bool bFoundNewline = false;

			for (; Current < EndSse;)
			{
				First8 = _mm_loadu_si128((const __m128i*)Current);
				Second8 = _mm_loadu_si128((const __m128i*)(Current + 8));
				CurrentWord = _mm_packus_epi16(First8, Second8);

				CRMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharCR));
				int32 LFMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharLF));
				int32 EitherMask = CRMask | LFMask;
				if (EitherMask)
				{
					int32 NewlineOffset = _tzcnt_u32(EitherMask);
					Current += NewlineOffset;
					bFoundNewline = true;
					break;
				}
				else
				{
					Current += 16;
				}
			}

			if (!bFoundNewline)
			{
				// Ran out of input buffer we can safely scan with SSE -- resume comment parsing in single character parser.
				goto SingleLineCommentParse;
			}
			if (CRMask)
			{
				// Hit a CR.  Stop and fall back to single character parser.  Note that we don't need to worry about rewinding for
				// a newline pair here, because we stop on either that's encountered first, so we haven't emitted a newline yet.
				break;
			}
		}
		else
		{
			// Multi line comment, skip to end of comment, writing newlines
			bool bFoundEnd = false;

			for (; Current < EndSse;)
			{
				First8 = _mm_loadu_si128((const __m128i*)Current);
				Second8 = _mm_loadu_si128((const __m128i*)(Current + 8));
				CurrentWord = _mm_packus_epi16(First8, Second8);

				// Fall back to single character parsing if we hit a CR
				CRMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharCR));
				if (CRMask)
				{
					// Go back one character if this is the first CR, and previous character was LF
					if ((CRMask & 1) && (Current > Start) && *(Current - 1) == '\n')
					{
						Current--;
						CurrentOut--;
					}
					goto MultiLineCommentParse;
				}

				StarMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharStar));
				SlashMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharSlash));
				int32 LFMask = _mm_movemask_epi8(_mm_cmpeq_epi8(CurrentWord, CharLF));

				int32 CommentEndMask = StarMask & (SlashMask >> 1);
				if (CommentEndMask)
				{
					// Process any newlines before the comment end
					int32 CommentEndOffset = _tzcnt_u32(CommentEndMask);
					LFMask &= (0xffff >> (16 - CommentEndOffset));
					if (LFMask)
					{
						_mm_storeu_si128((__m128i*)CurrentOut, CharLF);
						CurrentOut += _mm_popcnt_u32(LFMask);
					}
					Current += CommentEndOffset + 2;
					bFoundEnd = true;
					break;
				}
				else
				{
					// No comment end -- process any newlines in the first 15 characters and continue
					LFMask &= 0x7fff;
					if (LFMask)
					{
						_mm_storeu_si128((__m128i*)CurrentOut, CharLF);
						CurrentOut += _mm_popcnt_u32(LFMask);
					}
					Current += 15;
				}
			}

			if (!bFoundEnd)
			{
				// Ran out of input buffer we can safely scan with SSE -- resume comment parsing in single character parser.
				goto MultiLineCommentParse;
			}
		}
	}
#endif	// PLATFORM_ALWAYS_HAS_SSE4_2

	for (; Current < End;)
	{
		// sanity check that we're not overrunning the buffer
		check(CurrentOut < (OutStripped.GetData() + BufferSize));
		// CommentStripNeedsHandling returns true when *Current == '\0';
		while (!CommentStripNeedsHandling(*Current))
		{
			// straight cast to ansichar; since this is a character in hlsl source that's not in a comment
			// we assume that it must be valid to do so. if this assumption is not valid the shader source was
			// broken/corrupt anyways.
			*CurrentOut++ = (ANSICHAR)(*Current++);
		}

		if (IsEndOfLine(*Current))
		{
			*CurrentOut++ = '\n';
			Current += NewlineCharCount(Current[0], Current[1]);
		}
		else if (Current[0] == '/')
		{
			if (Current[1] == '/')
			{
#if PLATFORM_ALWAYS_HAS_SSE4_2
				SingleLineCommentParse:
#endif
				while (!IsEndOfLine(*Current) && Current < End)
				{
					++Current;
				}
			}
			else if (Current[1] == '*')
			{
				Current += 2;
				while (Current < End)
				{
#if PLATFORM_ALWAYS_HAS_SSE4_2
					MultiLineCommentParse:
#endif
					if (Current[0] == '*' && Current[1] == '/')
					{
						Current += 2;
						break;
					}
					else if (IsEndOfLine(*Current))
					{
						*CurrentOut++ = '\n';
						Current += NewlineCharCount(Current[0], Current[1]);
					}
					else
					{
						++Current;
					}
				}
			}
			else
			{
				*CurrentOut++ = (ANSICHAR)(*Current++);
			}
		}
	}
	// Null terminate after comment-stripped copy, plus 15 zero padding characters for SSE safe reads
	check(CurrentOut + 16 <= (OutStripped.GetData() + BufferSize));
	for (int32 TerminateAndPadIndex = 0; TerminateAndPadIndex < 16; TerminateAndPadIndex++)
	{
		*CurrentOut++ = 0;
	}

	// Set correct length after stripping but don't bother shrinking/reallocating, minor memory overhead to save time
	OutStripped.SetNum(CurrentOut - OutStripped.GetData(), EAllowShrinking::No);
}

bool LoadShaderSourceFile(const TCHAR* InVirtualFilePath, EShaderPlatform ShaderPlatform, FString* OutFileContents, TArray<FShaderCompilerError>* OutCompileErrors, const FName* ShaderPlatformName, FShaderSharedAnsiStringPtr* OutStrippedContents) // TODO: const FString&
{
#if WITH_EDITORONLY_DATA
	// it's not expected that cooked platforms get here, but if they do, this is the final out
	if (FPlatformProperties::RequiresCookedData())
	{
		return false;
	}

	bool bResult = false;

	STAT(double ShaderFileLoadingTime = 0);

	{
		SCOPE_SECONDS_COUNTER(ShaderFileLoadingTime);

		FString VirtualFilePath(InVirtualFilePath);
		FixupShaderFilePath(VirtualFilePath, ShaderPlatform, ShaderPlatformName);

		FShaderFileCacheEntry* CachedFile = nullptr;

		// First try a shared lock and only acquire exclusive access if element is not found in cache
		uint32 CurrentHash = GetTypeHash(VirtualFilePath);
		FShaderFileCache& ShaderFileCache = GShaderFileCache[CurrentHash % GSHADERFILECACHE_BUCKETS];
		{
			FRWScopeLock ScopeLock(ShaderFileCache.Lock, SLT_ReadOnly);
			CachedFile = ShaderFileCache.Map.FindByHash(CurrentHash, VirtualFilePath);
		}

		if (CachedFile)
		{
			if (OutFileContents)
			{
				*OutFileContents = CachedFile->Source;
			}
			if (OutStrippedContents)
			{
				*OutStrippedContents = CachedFile->StrippedSource;
			}
			bResult = true;
		}
		else 
		{
			FRWScopeLock ScopeLock(ShaderFileCache.Lock, SLT_Write);

			// Double-check the cache while holding exclusive lock as another thread may have added the item we're looking for
			CachedFile = ShaderFileCache.Map.FindByHash(CurrentHash, VirtualFilePath);

			// if this file has already been loaded and cached, use that
			if (CachedFile)
			{
				if (OutFileContents)
				{
					*OutFileContents = CachedFile->Source;
				}
				if (OutStrippedContents)
				{
					*OutStrippedContents = CachedFile->StrippedSource;
				}
				bResult = true;
			}
			else
			{
				FString ShaderFilePath = GetShaderSourceFilePath(VirtualFilePath, OutCompileErrors);

				// verify SHA hash of shader files on load. missing entries trigger an error
				FShaderFileCacheEntry FileContents;
				if (!ShaderFilePath.IsEmpty() && FFileHelper::LoadFileToString(FileContents.Source, *ShaderFilePath, FFileHelper::EHashOptions::EnableVerify|FFileHelper::EHashOptions::ErrorMissingHash) )
				{
					TArray<ANSICHAR>* StrippedSource = new TArray<ANSICHAR>;
					ShaderConvertAndStripComments(FileContents.Source, *StrippedSource);
					FileContents.StrippedSource = MakeShareable(StrippedSource);

					//update the shader file cache
					ShaderFileCache.Map.AddByHash(CurrentHash, VirtualFilePath, FileContents);

					if (OutFileContents)
					{
						*OutFileContents = FileContents.Source;
					}
					if (OutStrippedContents)
					{
						*OutStrippedContents = FileContents.StrippedSource;
					}
					bResult = true;
				}
			}
		}
	}

	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_LoadingShaderFiles,(float)ShaderFileLoadingTime);

	return bResult;
#else
	return false;
#endif // WITH_EDITORONLY_DATA
}

void LoadShaderSourceFileChecked(const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform, FString& OutFileContents, const FName* ShaderPlatformName)
{
	if (!LoadShaderSourceFile(VirtualFilePath, ShaderPlatform, &OutFileContents, nullptr, ShaderPlatformName))
	{
		UE_LOG(LogShaders, Fatal, TEXT("Couldn't find source file of virtual shader path \'%s\'"), VirtualFilePath);
	}
}

/**
 * Walks InStr until we find either an end-of-line or TargetChar.
 */
const TCHAR* SkipToCharOnCurrentLine(const TCHAR* InStr, TCHAR TargetChar)
{
	const TCHAR* Str = InStr;
	if (Str)
	{
		while (*Str && *Str != TargetChar && *Str != TEXT('\n'))
		{
			++Str;
		}
		if (*Str != TargetChar)
		{
			Str = NULL;
		}
	}
	return Str;
}

/**
* Find the first valid preprocessor include directive in the given text.
* @return Pointer to start of first include directive if found.  nullptr otherwise.
*/
static const TCHAR* FindFirstInclude(const TCHAR* Text)
{
	const TCHAR *IncludeToken = TEXT("include");
	const uint32 IncludeTokenLength = FCString::Strlen(IncludeToken);
	const TCHAR* PreprocessorDirectiveStart = FCString::Strstr(Text, TEXT("#"));
	while (PreprocessorDirectiveStart)
	{
		// Eat any whitespace between # and the next token.
		const TCHAR* ParseHead = PreprocessorDirectiveStart + 1;
		while (*ParseHead == TEXT(' ') || *ParseHead == TEXT('\t'))
		{
			++ParseHead;
		}
		// Check for "include" token.
		if (FCString::Strnicmp(ParseHead, IncludeToken, IncludeTokenLength) == 0)
		{
			ParseHead += IncludeTokenLength;
		}
		// Need a trailing whitespace character to make a valid include directive.
		if (*ParseHead == TEXT(' ') || *ParseHead == TEXT('\t'))
		{
			return PreprocessorDirectiveStart;
		}
		// Look for the next preprocess directive.
		PreprocessorDirectiveStart = *PreprocessorDirectiveStart ? FCString::Strstr(PreprocessorDirectiveStart + 1, TEXT("#")) : nullptr;
	}
	return nullptr;
}

static void StringCopyToAnsiCharArray(const TCHAR* Text, int32 TextLen, TArray<ANSICHAR>& Out)
{
	Out.SetNumUninitialized(TextLen + 1);
	ANSICHAR* OutData = Out.GetData();
	for (int32 CharIndex = 0; CharIndex < TextLen; CharIndex++, OutData++, Text++)
	{
		*OutData = (ANSICHAR)*Text;
	}
	*OutData = 0;
}

// Allocates structure and adds root file dependency
static FShaderPreprocessDependencies* ShaderPreprocessDependenciesBegin(const TCHAR* VirtualFilePath)
{
	FShaderPreprocessDependencies* PreprocessDependencies = new FShaderPreprocessDependencies();

	PreprocessDependencies->Dependencies.AddDefaulted();
	StringCopyToAnsiCharArray(VirtualFilePath, FCString::Strlen(VirtualFilePath), PreprocessDependencies->Dependencies[0].ResultPath);
	PreprocessDependencies->Dependencies[0].ResultPathHash = FCrc::Strihash_DEPRECATED(VirtualFilePath);

	return PreprocessDependencies;
}

// Adds finished dependencies to the cache
static void ShaderPreprocessDependenciesEnd(const TCHAR* VirtualFilePath, FShaderPreprocessDependencies* PreprocessDependencies, EShaderPlatform Platform)
{
	uint32 CurrentHash = FCrc::Strihash_DEPRECATED(VirtualFilePath);
	FShaderFileCache& ShaderFileCache = GShaderFileCache[CurrentHash % GSHADERFILECACHE_BUCKETS];
	{
		FRWScopeLock ScopeLock(ShaderFileCache.Lock, SLT_Write);
		FShaderFileCacheEntry* CachedFile = ShaderFileCache.Map.FindByHash(CurrentHash, VirtualFilePath);
		if (CachedFile)
		{
			// Another thread could have finished the job...  If not, set the dependencies.
			if (!CachedFile->Dependencies.IsValid())
			{
				CachedFile->Dependencies = MakeShareable(PreprocessDependencies);
			}
			else
			{
				delete PreprocessDependencies;
			}
		}
	}
}

static void AddPreprocessDependency(FShaderPreprocessDependencies& Dependencies, const FShaderPreprocessDependency& Dependency)
{
	check(Dependency.StrippedSource.IsValid());

	// First, check if the dependency already exists
	for (uint32 HashIndex = Dependencies.BySource.First(GetTypeHash(Dependency.PathInSourceHash)); Dependencies.BySource.IsValid(HashIndex); HashIndex = Dependencies.BySource.Next(HashIndex))
	{
		FShaderPreprocessDependency& TestDependency = Dependencies.Dependencies[HashIndex];

		// Subtract one from PathInSource.Num() to get length minus null terminator
		if (TestDependency.EqualsPathInSource(Dependency.PathInSource.GetData(), Dependency.PathInSource.Num() - 1, Dependency.PathInSourceHash, Dependency.ParentPath.GetData()))
		{
			// The result path better be the same for both
			check(!FCStringAnsi::Stricmp(TestDependency.ResultPath.GetData(), Dependency.ResultPath.GetData()));
			return;
		}
	}

	// Add the dependency
	int32 AddedIndex = Dependencies.Dependencies.Add(Dependency);
	Dependencies.BySource.Add(GetTypeHash(Dependency.PathInSourceHash), (uint32)AddedIndex);

	// Then check if the result path already exists, so we can point ResultPathUniqueIndex at the first instance of the result path
	uint32 ExistingResultIndex;
	for (ExistingResultIndex = Dependencies.ByResult.First(Dependency.ResultPathHash); Dependencies.ByResult.IsValid(ExistingResultIndex); ExistingResultIndex = Dependencies.ByResult.Next(ExistingResultIndex))
	{
		FShaderPreprocessDependency& TestDependency = Dependencies.Dependencies[ExistingResultIndex];
		if (TestDependency.EqualsResultPath(Dependency.ResultPath.GetData(), Dependency.ResultPathHash))
		{
			break;
		}
	}

	if (Dependencies.ByResult.IsValid(ExistingResultIndex))
	{
		// Reference existing result
		Dependencies.Dependencies[AddedIndex].ResultPathUniqueIndex = ExistingResultIndex;
	}
	else
	{
		// Add new result
		Dependencies.Dependencies[AddedIndex].ResultPathUniqueIndex = (uint32)AddedIndex;
		Dependencies.ByResult.Add(Dependency.ResultPathHash, (uint32)AddedIndex);
	}
}

/**
 * Recursively populates IncludeFilenames with the unique include filenames found in the shader file named Filename.
 */
static void InternalGetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, const FString& FileContents, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, bool AddToIncludeFile, const FName* ShaderPlatformName, FShaderPreprocessDependencies* OutDependencies)
{
	//avoid an infinite loop with a 0 length string
	if (FileContents.Len() > 0)
	{
		if (AddToIncludeFile)
		{
			IncludeVirtualFilePaths.Add(VirtualFilePath);
		}

		//find the first include directive
		const TCHAR* IncludeBegin = FindFirstInclude(*FileContents);

		uint32 SearchCount = 0;
		const uint32 MaxSearchCount = 200;
		//keep searching for includes as long as we are finding new ones and haven't exceeded the fixed limit
		while (IncludeBegin != NULL && SearchCount < MaxSearchCount && DepthLimit > 0)
		{
			//find the first double quotation after the include directive
			const TCHAR* IncludeFilenameBegin = SkipToCharOnCurrentLine(IncludeBegin, TEXT('\"'));

			if (IncludeFilenameBegin)
			{
				//find the trailing double quotation
				const TCHAR* IncludeFilenameEnd = SkipToCharOnCurrentLine(IncludeFilenameBegin + 1, TEXT('\"'));

				if (IncludeFilenameEnd)
				{
					//construct a string between the double quotations
					FString ExtractedIncludeFilename(FString((int32)(IncludeFilenameEnd - IncludeFilenameBegin - 1), IncludeFilenameBegin + 1));

					// If the include is relative, then it must be relative to the current virtual file path.
					if (!ExtractedIncludeFilename.StartsWith(TEXT("/")))
					{
						ExtractedIncludeFilename = FPaths::GetPath(VirtualFilePath) / ExtractedIncludeFilename;

						// Collapse any relative directories to allow #include "../MyFile.ush"
						FPaths::CollapseRelativeDirectories(ExtractedIncludeFilename);
					}

					//CRC the template, not the filled out version so that this shader's CRC will be independent of which material references it.
					const TCHAR* MaterialTemplateName = TEXT("/Engine/Private/MaterialTemplate.ush");
					const TCHAR* MaterialGeneratedName = TEXT("/Engine/Generated/Material.ush");

					bool bIsMaterialTemplate = false;
					if (ExtractedIncludeFilename == MaterialGeneratedName)
					{
						ExtractedIncludeFilename = MaterialTemplateName;
						bIsMaterialTemplate = true;
					}

					bool bIsPlatformFile = ReplaceVirtualFilePathForShaderPlatform(ExtractedIncludeFilename, ShaderPlatform);

					// Fixup autogen file
					bIsPlatformFile |= ReplaceVirtualFilePathForShaderAutogen(ExtractedIncludeFilename, ShaderPlatform, ShaderPlatformName);

					// Ignore uniform buffer, vertex factory and instanced stereo includes
					bool bIgnoreInclude = ExtractedIncludeFilename.StartsWith(TEXT("/Engine/Generated/"));

					// Check virtual.
					bIgnoreInclude |= !CheckVirtualShaderFilePath(ExtractedIncludeFilename);

					// Include only platform specific files, which will be used by the target platform.
					{
						FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_ReadOnly);
						bIgnoreInclude = bIgnoreInclude || GShaderHashCache.ShouldIgnoreInclude(ExtractedIncludeFilename, ShaderPlatform);
					}

					bIsPlatformFile |= FShaderHashCache::IsPlatformInclude(ExtractedIncludeFilename);

					//vertex factories need to be handled separately
					if (!bIgnoreInclude)
					{
						int32 SeenFilenameIndex = IncludeVirtualFilePaths.Find(ExtractedIncludeFilename);
						if (SeenFilenameIndex == INDEX_NONE)
						{
							// Preprocess dependencies don't include platform files.
							FShaderPreprocessDependencies* ExtractedIncludeDependencies = nullptr;
							if (OutDependencies && !bIsPlatformFile)
							{
								ExtractedIncludeDependencies = ShaderPreprocessDependenciesBegin(*ExtractedIncludeFilename);
							}

							// First element in Dependencies is root file, so initialize the StrippedSource pointer in it
							FString IncludedFileContents;
							LoadShaderSourceFile(*ExtractedIncludeFilename, ShaderPlatform, &IncludedFileContents, nullptr, ShaderPlatformName,
								ExtractedIncludeDependencies ? &ExtractedIncludeDependencies->Dependencies[0].StrippedSource : nullptr);

							InternalGetShaderIncludes(EntryPointVirtualFilePath, *ExtractedIncludeFilename, IncludedFileContents, IncludeVirtualFilePaths, ShaderPlatform, DepthLimit - 1, true, ShaderPlatformName, ExtractedIncludeDependencies);

							if (ExtractedIncludeDependencies)
							{
								// Some generated shaders are referenced as includes, and won't be found -- if so, just delete the dependencies
								if (ExtractedIncludeDependencies->Dependencies[0].StrippedSource.IsValid())
								{
									ShaderPreprocessDependenciesEnd(*ExtractedIncludeFilename, ExtractedIncludeDependencies, ShaderPlatform);
								}
								else
								{
									delete ExtractedIncludeDependencies;
								}
							}
						}

						if (OutDependencies)
						{
							// Preprocess dependencies don't include platform files.
							if (!bIsPlatformFile)
							{
								// The material template itself isn't added as a dependency, but child includes of it are.
								FShaderSharedAnsiStringPtr StrippedContents;
								if (!bIsMaterialTemplate && LoadShaderSourceFile(*ExtractedIncludeFilename, ShaderPlatform, nullptr, nullptr, nullptr, &StrippedContents))
								{
									// Add immediate dependency
									FShaderPreprocessDependency Dependency;
									Dependency.StrippedSource = StrippedContents;

									// If the parent is the material template, switch its name to the generated name, so include dependencies from
									// the material template to other non-procedural files can be cached.
									const TCHAR* ParentNonTemplate = VirtualFilePath == MaterialTemplateName ? MaterialGeneratedName : VirtualFilePath;

									// We want ResultPath to have consistent case, for the preprocessor which is case sensitive.  So we use the exact
									// string from the previously found array element if it exists.  If this is the first time it's encountered, it will
									// have been added to the array by the InternalGetShaderIncludes call above.
									const FString& ResultPath = SeenFilenameIndex == INDEX_NONE ? ExtractedIncludeFilename : IncludeVirtualFilePaths[SeenFilenameIndex];

									StringCopyToAnsiCharArray(IncludeFilenameBegin + 1, (int32)(IncludeFilenameEnd - IncludeFilenameBegin - 1), Dependency.PathInSource);
									StringCopyToAnsiCharArray(ParentNonTemplate, FCString::Strlen(ParentNonTemplate), Dependency.ParentPath);
									StringCopyToAnsiCharArray(*ResultPath, ResultPath.Len(), Dependency.ResultPath);
									Dependency.ResultPathHash = GetTypeHash(ResultPath);

									// Hash deliberately doesn't include null terminator, so we can generate hash from string view.  Xxhash is faster than
									// the normal case insensitive string hash, so we choose that.
									Dependency.PathInSourceHash = FXxHash64::HashBuffer(Dependency.PathInSource.GetData(), Dependency.PathInSource.Num() - 1);

									AddPreprocessDependency(*OutDependencies, Dependency);
								}

								// Add recursive dependencies from the child
								FShaderPreprocessDependenciesShared ChildDependenciesShared;
								if (GetShaderPreprocessDependencies(*ExtractedIncludeFilename, ShaderPlatform, ChildDependenciesShared))
								{
									const FShaderPreprocessDependencies& ChildDependencies = *ChildDependenciesShared;

									// Skip over first entry, which is the root file (its dependency is handled by the "add immediate dependency" code above)
									for (int32 DependencyIndex = 1; DependencyIndex < ChildDependencies.Dependencies.Num(); DependencyIndex++)
									{
										AddPreprocessDependency(*OutDependencies, ChildDependencies.Dependencies[DependencyIndex]);
									}
								}

							}  // if (!bIsPlatformFile)
						}  // if (OutDependencies)
					}  // if (!bIgnoreInclude)
				}
			}

			// Skip to the end of the line.
			IncludeBegin = SkipToCharOnCurrentLine(IncludeBegin, TEXT('\n'));
		
			//find the next include directive
			if (IncludeBegin && *IncludeBegin != 0)
			{
				IncludeBegin = FindFirstInclude(IncludeBegin + 1);
			}
			SearchCount++;
		}

		if (SearchCount == MaxSearchCount || DepthLimit == 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("GetShaderIncludes parsing terminated early to avoid infinite looping!\n Entrypoint \'%s\' CurrentInclude \'%s\' SearchCount %u Depth %u"), 
				EntryPointVirtualFilePath, 
				VirtualFilePath,
				SearchCount, 
				DepthLimit);
		}
	}
}

bool GetShaderPreprocessDependencies(const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform, FShaderPreprocessDependenciesShared& OutDependencies)
{
	// Same case insensitive hash used by FString
	uint32 CurrentHash = FCrc::Strihash_DEPRECATED(VirtualFilePath);
	FShaderFileCache& ShaderFileCache = GShaderFileCache[CurrentHash % GSHADERFILECACHE_BUCKETS];

	FRWScopeLock ScopeLock(ShaderFileCache.Lock, SLT_ReadOnly);
	FShaderFileCacheEntry* CachedFile = ShaderFileCache.Map.FindByHash(CurrentHash, VirtualFilePath);
	if (CachedFile && CachedFile->Dependencies.IsValid())
	{
		OutDependencies = CachedFile->Dependencies;
		return true;
	}

	return false;
}

/**
 * Recursively populates IncludeFilenames with the unique include filenames found in the shader file named Filename.
 */
static void InternalGetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, bool AddToIncludeFile, const FName* ShaderPlatformName, bool bPreprocessDependencies)
{
	FShaderPreprocessDependencies* PreprocessDependencies = nullptr;
	if (bPreprocessDependencies)
	{
		// Check if they've already been generated.  These are platform independent, so we only need to generate them once if multiple platforms are being cooked,
		// but in case we want to specialize them by platform in the future, the platform is passed in.
		FShaderPreprocessDependenciesShared OutDependenciesIgnored;
		if (!GetShaderPreprocessDependencies(VirtualFilePath, ShaderPlatform, OutDependenciesIgnored))
		{
			// Allocates dependency structure and adds root file element
			PreprocessDependencies = ShaderPreprocessDependenciesBegin(VirtualFilePath);
		}
	}

	// First element in Dependencies is root file, so initialize the StrippedSource pointer in it
	FString FileContents;
	LoadShaderSourceFile(VirtualFilePath, ShaderPlatform, &FileContents, nullptr, ShaderPlatformName, PreprocessDependencies ? &PreprocessDependencies->Dependencies[0].StrippedSource : nullptr);

	InternalGetShaderIncludes(EntryPointVirtualFilePath, VirtualFilePath, FileContents, IncludeVirtualFilePaths, ShaderPlatform, DepthLimit, AddToIncludeFile, ShaderPlatformName, PreprocessDependencies);

	if (PreprocessDependencies)
	{
		// Adds completed dependency structure to shader cache map entry
		ShaderPreprocessDependenciesEnd(VirtualFilePath, PreprocessDependencies, ShaderPlatform);
	}
}

void GetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, const FName* ShaderPlatformName)
{
	InternalGetShaderIncludes(EntryPointVirtualFilePath, VirtualFilePath, IncludeVirtualFilePaths, ShaderPlatform, DepthLimit, false, ShaderPlatformName, false);
}

void GetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, const FString& FileContents, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, const FName* ShaderPlatformName)
{
	InternalGetShaderIncludes(EntryPointVirtualFilePath, VirtualFilePath, FileContents, IncludeVirtualFilePaths, ShaderPlatform, DepthLimit, false, ShaderPlatformName, nullptr);
}

void HashShaderFileWithIncludes(FArchive& HashingArchive, const TCHAR* VirtualFilePath, const FString& FileContents, EShaderPlatform ShaderPlatform, bool bOnlyHashIncludedFiles)
{
	auto HashSingleFile = [](FArchive& HashingArchive, const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform, const FString& FileContents)
	{
		// first, a "soft" check
		bool bFoundInCache = false;
		{
			FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_ReadOnly);
			const FSHAHash* CachedHash = GShaderHashCache.FindHash(ShaderPlatform, VirtualFilePath);
			// If a hash for this filename has been cached, use that
			if (CachedHash)
			{
				bFoundInCache = true;
				HashingArchive << const_cast<FSHAHash&>(*CachedHash);
			}
		}

		// outside of the lock scope because we don't need the lock and hashing can take time
		if (!bFoundInCache)
		{			
			// if the file isn't generated, add it to the cache now
			bool bGenerated = FCString::Strstr(VirtualFilePath, TEXT("Generated")) != nullptr;
			if (!bGenerated)
			{
				// this function fails hard if it cannot load
				const FSHAHash& FileHash = GetShaderFileHash(VirtualFilePath, ShaderPlatform);
				HashingArchive << const_cast<FSHAHash&>(FileHash);
			}
			else
			{
				// note, it is legal for some generated files to have empty contents, so hash both the name and their contents
				HashingArchive.Serialize(reinterpret_cast<void*>(const_cast<TCHAR*>(VirtualFilePath)), FCString::Strlen(VirtualFilePath));
				HashingArchive << const_cast<FString&>(FileContents);
			}
		}
	};

	// First, always hash the file itself
	HashSingleFile(HashingArchive, VirtualFilePath, ShaderPlatform, FileContents);

	// Get the list of includes this file contains
	TArray<FString> IncludeVirtualFilePaths;
	GetShaderIncludes(VirtualFilePath, VirtualFilePath, FileContents, IncludeVirtualFilePaths, ShaderPlatform);

	for (int32 IncludeIndex = 0; IncludeIndex < IncludeVirtualFilePaths.Num(); IncludeIndex++)
	{
		// Here, we assume that all includes can be found in cache. This also means that generated files won't include other generated files.
		HashSingleFile(HashingArchive, *IncludeVirtualFilePaths[IncludeIndex], ShaderPlatform, FString());
	}
}

static void UpdateSingleShaderFilehash(FSHA1& InOutHashState, const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform)
{
	// Get the list of includes this file contains
	TArray<FString> IncludeVirtualFilePaths;
	GetShaderIncludes(VirtualFilePath, VirtualFilePath, IncludeVirtualFilePaths, ShaderPlatform);
#if WITH_EDITOR &&  !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( UE_LOG_ACTIVE(LogShaders, Verbose) )
	{
		UE_LOG(LogShaders, Verbose, TEXT("Generating hash of file %s, "), VirtualFilePath);
	}
#endif
	for (int32 IncludeIndex = 0; IncludeIndex < IncludeVirtualFilePaths.Num(); IncludeIndex++)
	{
		// Load the include file and hash it
		FString IncludeFileContents;
		LoadShaderSourceFileChecked(*IncludeVirtualFilePaths[IncludeIndex], ShaderPlatform, IncludeFileContents);
		InOutHashState.UpdateWithString(*IncludeFileContents, IncludeFileContents.Len());
#if WITH_EDITOR &&  !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (UE_LOG_ACTIVE(LogShaders, Verbose))
		{
			FSHA1 HashStateCopy = InOutHashState;
			FSHAHash IncrementalHash = HashStateCopy.Finalize();
			
			UE_LOG(LogShaders, Verbose, TEXT("Processing include file for %s, %s, %s"), VirtualFilePath, *IncludeVirtualFilePaths[IncludeIndex], *BytesToHex(IncrementalHash.Hash, 20));
		}
#endif
	}

	// Load the source file and hash it
	FString FileContents;
	LoadShaderSourceFileChecked(VirtualFilePath, ShaderPlatform, FileContents);
	InOutHashState.UpdateWithString(*FileContents, FileContents.Len());
}

/** 
* Prevents multiple threads from trying to redundantly call UpdateSingleShaderFilehash in GetShaderFileHash / GetShaderFilesHash.
* Must be used in conjunction with GShaderHashAccessRWLock, which protects actual GShaderHashCache operations.
*/
static FCriticalSection GShaderFileHashCalculationGuard;

/**
 * Calculates a Hash for the given filename and its includes if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 * @param ShaderPlatform - shader platform to Hash
 */
const FSHAHash& GetShaderFileHash(const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform)
{
	// Make sure we are only accessing GShaderHashCache from one thread
	//check(IsInGameThread() || IsAsyncLoading());
	STAT(double HashTime = 0);
	{
		SCOPE_SECONDS_COUNTER(HashTime);

		{
			FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_ReadOnly);
			const FSHAHash* CachedHash = GShaderHashCache.FindHash(ShaderPlatform, VirtualFilePath);
			// If a hash for this filename has been cached, use that
			if (CachedHash)
			{
				return *CachedHash;
			}
		}

		// We don't want UpdateSingleShaderFilehash to be called redundantly from multiple threads,
		// while minimiziong GShaderHashAccessRWLock exclusive lock time.
		// We can use a dedicated critical section around the hash calculation and cache update, 
		// while keeping the cache itself available for reading.
		FScopeLock FileHashCalculationAccessLock(&GShaderFileHashCalculationGuard);

		// Double-check the cache while holding exclusive lock as another thread may have added the item we're looking for
		const FSHAHash* CachedHash = GShaderHashCache.FindHash(ShaderPlatform, VirtualFilePath);
		if (CachedHash)
		{
			return *CachedHash;
		}

		FSHA1 HashState;
		UpdateSingleShaderFilehash(HashState, VirtualFilePath, ShaderPlatform);
		HashState.Final();

		// Update the hash cache
		FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_Write);
		FSHAHash& NewHash = GShaderHashCache.AddHash(ShaderPlatform, VirtualFilePath);
		HashState.GetHash(&NewHash.Hash[0]);

#if WITH_EDITOR &&  !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogShaders, Verbose, TEXT("Final hash for file %s, %s"), VirtualFilePath,*BytesToHex(&NewHash.Hash[0], 20));
#endif
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HashingShaderFiles, (float)HashTime);
		return NewHash;
	}
}

/**
 * Calculates a Hash for the given filename and its includes if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 * @param ShaderPlatform - shader platform to Hash
 */
const FSHAHash& GetShaderFilesHash(const TArray<FString>& VirtualFilePaths, EShaderPlatform ShaderPlatform)
{
	// Make sure we are only accessing GShaderHashCache from one thread
	//check(IsInGameThread() || IsAsyncLoading());
	STAT(double HashTime = 0);

	{
		SCOPE_SECONDS_COUNTER(HashTime);

		FString Key;
		for (const FString& Filename : VirtualFilePaths)
		{
			Key += Filename;
		}

		{
			FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_ReadOnly);
			const FSHAHash* CachedHash = GShaderHashCache.FindHash(ShaderPlatform, Key);

			// If a hash for this filename has been cached, use that
			if (CachedHash)
			{
				return *CachedHash;
			}
		}

		// We don't want UpdateSingleShaderFilehash to be called redundantly from multiple threads,
		// while minimiziong GShaderHashAccessRWLock exclusive lock time.
		// We can use a dedicated critical section around the hash calculation and cache update, 
		// while keeping the cache itself available for reading.
		FScopeLock FileHashCalculationAccessLock(&GShaderFileHashCalculationGuard);

		// Double-check the cache while holding exclusive lock as another thread may have added the item we're looking for
		const FSHAHash* CachedHash = GShaderHashCache.FindHash(ShaderPlatform, Key);
		if (CachedHash)
		{
			return *CachedHash;
		}

		FSHA1 HashState;
		for (const FString& VirtualFilePath : VirtualFilePaths)
		{
			UpdateSingleShaderFilehash(HashState, *VirtualFilePath, ShaderPlatform);
		}
		HashState.Final();

		// Update the hash cache
		FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_Write);
		FSHAHash& NewHash = GShaderHashCache.AddHash(ShaderPlatform, Key);
		HashState.GetHash(&NewHash.Hash[0]);

		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HashingShaderFiles, (float)HashTime);
		return NewHash;
	}
}

#if WITH_EDITOR
void BuildShaderFileToUniformBufferMap(TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BuildShaderFileToUniformBufferMap);

		TArray<FString> ShaderSourceFiles;
		GetAllVirtualShaderSourcePaths(ShaderSourceFiles, GMaxRHIShaderPlatform);

		FScopedSlowTask SlowTask((float)ShaderSourceFiles.Num());

		// Cache UB access strings, make it case sensitive for faster search
		struct FShaderVariable
		{
			FShaderVariable(const TCHAR* ShaderVariable) :
				OriginalShaderVariable(ShaderVariable), 
				SearchKey(FString(ShaderVariable).ToUpper() + TEXT(".")),
				// The shader preprocessor inserts a space after a #define replacement, make sure we detect the uniform buffer reference
				SearchKeyWithSpace(FString(ShaderVariable).ToUpper() + TEXT(" ."))
			{}

			bool operator<(const FShaderVariable& Other) const
			{
				return FCString::Strcmp(OriginalShaderVariable, Other.OriginalShaderVariable) < 0;
			}

			const TCHAR* OriginalShaderVariable;
			FString SearchKey;
			FString SearchKeyWithSpace;
		};
		// Cache each UB
		TArray<FShaderVariable> SearchKeys;
		for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
		{
			SearchKeys.Add(FShaderVariable(StructIt->GetShaderVariableName()));
		}

		// Sort SearchKeys for determinism in the generated ShaderFileToUniformBufferVariables maps, to improve consistency for A/B testing.
		// Order of items in FShaderParametersMetadata::GetStructList() is otherwise dependent on arbitrary startup constructor order.
		SearchKeys.Sort();

		TArray<UE::Tasks::TTask<void>> Tasks;
		Tasks.Reserve(ShaderSourceFiles.Num());

		// Just make sure that all the TArray inside the map won't move while being used by async tasks
		for (int32 FileIndex = 0; FileIndex < ShaderSourceFiles.Num(); FileIndex++)
		{
			ShaderFileToUniformBufferVariables.FindOrAdd(ShaderSourceFiles[FileIndex]);
		}

		// Find for each shader file which UBs it needs
		for (int32 FileIndex = 0; FileIndex < ShaderSourceFiles.Num(); FileIndex++)
		{
			SlowTask.EnterProgressFrame(1);

 			FString ShaderFileContents;
			LoadShaderSourceFileChecked(*ShaderSourceFiles[FileIndex], GMaxRHIShaderPlatform, ShaderFileContents);

			Tasks.Emplace(
				UE::Tasks::Launch(
					TEXT("SearchKeysInShaderContent"),
					[&SearchKeys, &ShaderSourceFiles, FileIndex, &ShaderFileToUniformBufferVariables, ShaderFileContents = MoveTemp(ShaderFileContents)]() mutable
					{
						// To allow case sensitive search which is way faster on some platforms (no need to look up locale, etc)
						ShaderFileContents.ToUpperInline();

						TArray<const TCHAR*>* ReferencedUniformBuffers = ShaderFileToUniformBufferVariables.Find(ShaderSourceFiles[FileIndex]);

						for (int32 SearchKeyIndex = 0; SearchKeyIndex < SearchKeys.Num(); ++SearchKeyIndex)
						{
							// Searching for the uniform buffer shader variable being accessed with '.'
							if (ShaderFileContents.Contains(SearchKeys[SearchKeyIndex].SearchKey, ESearchCase::CaseSensitive)
								|| ShaderFileContents.Contains(SearchKeys[SearchKeyIndex].SearchKeyWithSpace, ESearchCase::CaseSensitive))
								{
									ReferencedUniformBuffers->AddUnique(SearchKeys[SearchKeyIndex].OriginalShaderVariable);
							}
						}
					}
				)
			);
		}
		UE::Tasks::Wait(Tasks);
	}
}
#endif // WITH_EDITOR

void InitializeShaderHashCache()
{
	FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_Write);
	GShaderHashCache.Initialize();
}

void UpdateIncludeDirectoryForPreviewPlatform(EShaderPlatform PreviewPlatform, EShaderPlatform ActualPlatform)
{
	FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_Write);
	GShaderHashCache.UpdateIncludeDirectoryForPreviewPlatform(PreviewPlatform, ActualPlatform);
}

void CheckShaderHashCacheInclude(const FString& VirtualFilePath, EShaderPlatform ShaderPlatform, const FString& ShaderFormatName)
{
	FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_ReadOnly);
	bool bIgnoreInclude = GShaderHashCache.ShouldIgnoreInclude(VirtualFilePath, ShaderPlatform);

	checkf(!bIgnoreInclude,
		TEXT("Shader compiler is trying to include %s, which is not located in IShaderFormat::GetPlatformIncludeDirectory for %s."),
		*VirtualFilePath,
		*ShaderFormatName);
}

void InitializeShaderTypes()
{
	UE_LOG(LogShaders, Log, TEXT("InitializeShaderTypes() begin"));

	LogShaderSourceDirectoryMappings();

	TMap<FString, TArray<const TCHAR*> > ShaderFileToUniformBufferVariables;
#if WITH_EDITOR
	BuildShaderFileToUniformBufferMap(ShaderFileToUniformBufferVariables);
#endif // WITH_EDITOR

	FShaderType::Initialize(ShaderFileToUniformBufferVariables);
	FVertexFactoryType::Initialize(ShaderFileToUniformBufferVariables);

	FShaderPipelineType::Initialize();

	UE_LOG(LogShaders, Log, TEXT("InitializeShaderTypes() end"));
}

void UninitializeShaderTypes()
{
	UE_LOG(LogShaders, Log, TEXT("UninitializeShaderTypes() begin"));

	FShaderPipelineType::Uninitialize();

	FShaderType::Uninitialize();
	FVertexFactoryType::Uninitialize();

	UE_LOG(LogShaders, Log, TEXT("UninitializeShaderTypes() end"));
}

/**
 * Flushes the shader file and CRC cache, and regenerates the binary shader files if necessary.
 * Allows shader source files to be re-read properly even if they've been modified since startup.
 */
void FlushShaderFileCache()
{
	UE_LOG(LogShaders, Log, TEXT("FlushShaderFileCache() begin"));

	{
		FRWScopeLock ShaderHashAccessLock(GShaderHashAccessRWLock, SLT_Write);
		GShaderHashCache.Empty();
	}
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(GShaderFileCache); ++Index)
		{
			FRWScopeLock ScopeLock(GShaderFileCache[Index].Lock, SLT_Write);
			GShaderFileCache[Index].Map.Empty();
		}
	}

	UE_LOG(LogShaders, Log, TEXT("FlushShaderFileCache() end"));
}

#if WITH_EDITOR

void UpdateReferencedUniformBufferNames(
	TArrayView<const FShaderType*> OutdatedShaderTypes,
	TArrayView<const FVertexFactoryType*> OutdatedFactoryTypes,
	TArrayView<const FShaderPipelineType*> OutdatedShaderPipelineTypes)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateReferencedUniformBufferNames);

		LogShaderSourceDirectoryMappings();

		TMap<FString, TArray<const TCHAR*> > ShaderFileToUniformBufferVariables;
		BuildShaderFileToUniformBufferMap(ShaderFileToUniformBufferVariables);

		for (const FShaderPipelineType* PipelineType : OutdatedShaderPipelineTypes)
		{
			for (const FShaderType* ShaderType : PipelineType->GetStages())
			{
				const_cast<FShaderType*>(ShaderType)->UpdateReferencedUniformBufferNames(ShaderFileToUniformBufferVariables);
			}
		}

		for (const FShaderType* ShaderType : OutdatedShaderTypes)
		{
			const_cast<FShaderType*>(ShaderType)->UpdateReferencedUniformBufferNames(ShaderFileToUniformBufferVariables);
		}

		for (const FVertexFactoryType* VertexFactoryType : OutdatedFactoryTypes)
		{
			const_cast<FVertexFactoryType*>(VertexFactoryType)->UpdateReferencedUniformBufferNames(ShaderFileToUniformBufferVariables);
		}
	}
}

void GenerateReferencedUniformBufferNames(
	const TCHAR* SourceFilename,
	const TCHAR* ShaderTypeName,
	const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables,
	TSet<const TCHAR*, TStringPointerSetKeyFuncs_DEPRECATED<const TCHAR*>>& UniformBufferNames)
{
	TArray<FString> FilesToSearch;
	GetShaderIncludes(SourceFilename, SourceFilename, FilesToSearch, GMaxRHIShaderPlatform);
	FilesToSearch.Emplace(SourceFilename);

	for (const FString& FileToSearch : FilesToSearch)
	{
		const TArray<const TCHAR*>& FoundUniformBufferVariables = ShaderFileToUniformBufferVariables.FindChecked(FileToSearch);
		for (const TCHAR* UniformBufferName : FoundUniformBufferVariables)
		{
			UniformBufferNames.Emplace(UniformBufferName);
		}
	}
}

namespace {
// anonymous namespace

class FFrozenMaterialLayoutHashCache
{
public:
	FSHAHash Get(const FTypeLayoutDesc& TypeDesc, FPlatformTypeLayoutParameters LayoutParams)
	{
		{
			FReadScopeLock ReadScope(Lock);

			if (const FPlatformCache* Platform = Algo::FindBy(Platforms, LayoutParams, &FPlatformCache::Parameters))
			{
				if (const FSHAHash* Hash = Platform->Cache.Find(&TypeDesc))
				{
					return *Hash;
				}
			}
		}

		FSHAHash Hash = Freeze::HashLayout(TypeDesc, LayoutParams);

		{
			FWriteScopeLock WriteScope(Lock);

			FPlatformCache* Platform = Algo::FindBy(Platforms, LayoutParams, &FPlatformCache::Parameters);
			if (!Platform)
			{
				Platform = &Platforms.AddDefaulted_GetRef();
				Platform->Parameters = LayoutParams;
			}


			Platform->Cache.FindOrAdd(&TypeDesc, Hash);
		}

		return Hash;
	}

private:
	struct FPlatformCache
	{
		FPlatformTypeLayoutParameters Parameters;
		TMap<const FTypeLayoutDesc*, FSHAHash> Cache;
	};

	FRWLock Lock;
	TArray<FPlatformCache, TInlineAllocator<8>> Platforms;
};

} // anonymous namespace


FSHAHash GetShaderTypeLayoutHash(const FTypeLayoutDesc& TypeDesc, FPlatformTypeLayoutParameters LayoutParameters)
{
	static FFrozenMaterialLayoutHashCache GFrozenMaterialLayoutHashes;
	return GFrozenMaterialLayoutHashes.Get(TypeDesc, LayoutParameters);
}

void AppendKeyStringShaderDependencies(
	TConstArrayView<FShaderTypeDependency> ShaderTypeDependencies,
	FPlatformTypeLayoutParameters LayoutParams,
	FString& OutKeyString,
	bool bIncludeSourceHashes)
{
	// Simplified interface if we only have ShaderTypeDependencies
	AppendKeyStringShaderDependencies(
		ShaderTypeDependencies,
		TConstArrayView<FShaderPipelineTypeDependency>(),
		TConstArrayView<FVertexFactoryTypeDependency>(),
		LayoutParams,
		OutKeyString,
		bIncludeSourceHashes);
}

void AppendKeyStringShaderDependencies(
	TConstArrayView<FShaderTypeDependency> ShaderTypeDependencies,
	TConstArrayView<FShaderPipelineTypeDependency> ShaderPipelineTypeDependencies,
	TConstArrayView<FVertexFactoryTypeDependency> VertexFactoryTypeDependencies,
	FPlatformTypeLayoutParameters LayoutParams,
	FString& OutKeyString,
	bool bIncludeSourceHashes)
{
	TSet<const TCHAR*, TStringPointerSetKeyFuncs_DEPRECATED<const TCHAR*>> ReferencedUniformBufferNames;

	for (const FShaderTypeDependency& ShaderTypeDependency : ShaderTypeDependencies)
	{
		const FShaderType* ShaderType = FindShaderTypeByName(ShaderTypeDependency.ShaderTypeName);
		checkf(ShaderType != nullptr, TEXT("Failed to find FShaderType for dependency %hs (total in the NameToTypeMap: %d)"), ShaderTypeDependency.ShaderTypeName.GetDebugString().String.Get(), FShaderType::GetNameToTypeMap().Num());

		OutKeyString.AppendChar('_');
		OutKeyString.Append(ShaderType->GetName());
		OutKeyString.AppendInt(ShaderTypeDependency.PermutationId);
		OutKeyString.AppendChar('_');
		ERayTracingPayloadType RayTracingPayloadType = ShaderType->GetRayTracingPayloadType(ShaderTypeDependency.PermutationId);
		OutKeyString.AppendInt(static_cast<uint32>(RayTracingPayloadType));
		OutKeyString.AppendChar('_');
		OutKeyString.AppendInt(GetRayTracingPayloadTypeMaxSize(RayTracingPayloadType));

		if (bIncludeSourceHashes)
		{
			// Add the type's source hash so that we can invalidate cached shaders when .usf changes are made
			ShaderTypeDependency.SourceHash.AppendString(OutKeyString);
		}

		if (const FShaderParametersMetadata* ParameterStructMetadata = ShaderType->GetRootParametersMetadata())
		{
			ParameterStructMetadata->AppendKeyString(OutKeyString);
		}

		const FSHAHash LayoutHash = GetShaderTypeLayoutHash(ShaderType->GetLayout(), LayoutParams);
		LayoutHash.AppendString(OutKeyString);

		for (const TCHAR* UniformBufferName : ShaderType->GetReferencedUniformBufferNames())
		{
			ReferencedUniformBufferNames.Add(UniformBufferName);
		}
	}

	// Add the inputs for any shader pipelines that are stored inline in the shader map
	for (const FShaderPipelineTypeDependency& Dependency : ShaderPipelineTypeDependencies)
	{
		const FShaderPipelineType* ShaderPipelineType = FShaderPipelineType::GetShaderPipelineTypeByName(Dependency.ShaderPipelineTypeName);
		checkf(ShaderPipelineType != nullptr, TEXT("Failed to find FShaderPipelineType for dependency %hs (total in the NameToTypeMap: %d)"), Dependency.ShaderPipelineTypeName.GetDebugString().String.Get(), FShaderType::GetNameToTypeMap().Num());

		OutKeyString.AppendChar('_');
		OutKeyString.Append(ShaderPipelineType->GetName());

		if (bIncludeSourceHashes)
		{
			Dependency.StagesSourceHash.AppendString(OutKeyString);
		}

		for (const FShaderType* ShaderType : ShaderPipelineType->GetStages())
		{
			if (const FShaderParametersMetadata* ParameterStructMetadata = ShaderType->GetRootParametersMetadata())
			{
				ParameterStructMetadata->AppendKeyString(OutKeyString);
			}

			for (const TCHAR* UniformBufferName : ShaderType->GetReferencedUniformBufferNames())
			{
				ReferencedUniformBufferNames.Add(UniformBufferName);
			}
		}
	}

	for (const FVertexFactoryTypeDependency& VFDependency : VertexFactoryTypeDependencies)
	{
		OutKeyString.AppendChar('_');

		const FVertexFactoryType* VertexFactoryType = FVertexFactoryType::GetVFByName(VFDependency.VertexFactoryTypeName);

		OutKeyString.Append(VertexFactoryType->GetName());

		if (bIncludeSourceHashes)
		{
			VFDependency.VFSourceHash.AppendString(OutKeyString);
		}

		for (int32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
		{
			const FTypeLayoutDesc* ParameterLayout = VertexFactoryType->GetShaderParameterLayout((EShaderFrequency)Frequency);
			if (ParameterLayout)
			{
				const FSHAHash LayoutHash = GetShaderTypeLayoutHash(*ParameterLayout, LayoutParams);
				LayoutHash.AppendString(OutKeyString);
			}
		}

		for (const TCHAR* UniformBufferName : VertexFactoryType->GetReferencedUniformBufferNames())
		{
			ReferencedUniformBufferNames.Add(UniformBufferName);
		}
	}

	{
		TArray<const TCHAR*> SortedUniformBufferNames = ReferencedUniformBufferNames.Array();
		Algo::Sort(SortedUniformBufferNames, FUniformBufferNameSortOrder());

		// Save uniform buffer member info so we can detect when layout has changed
		for (const TCHAR* UniformBufferName : SortedUniformBufferNames)
		{
			FShaderParametersMetadata* UniformBufferMetadata = FindUniformBufferStructByName(UniformBufferName);
			UniformBufferMetadata->AppendKeyString(OutKeyString);
		}
	}
}

#endif // WITH_EDITOR

FString MakeInjectedShaderCodeBlock(const TCHAR* BlockName, const FString& CodeToInject)
{
	return FString::Printf(TEXT("#line 1 \"%s\"\n%s"), BlockName, *CodeToInject);
}

FString FShaderCompilerError::GetErrorStringWithSourceLocation() const
{
	if (!ErrorVirtualFilePath.IsEmpty() && !ErrorLineString.IsEmpty())
	{
		return ErrorVirtualFilePath + TEXT("(") + ErrorLineString + TEXT("): ") + StrippedErrorMessage;
	}
	else
	{
		return StrippedErrorMessage;
	}
}

FString FShaderCompilerError::GetErrorStringWithLineMarker() const
{
	if (HasLineMarker())
	{
		// Append highlighted line and its marker to the same error message with line terminators
		// to get a similar multiline error output as with DXC
		return (GetErrorStringWithSourceLocation() + LINE_TERMINATOR + TEXT("\t") + HighlightedLine + LINE_TERMINATOR + TEXT("\t") + HighlightedLineMarker);
	}
	else
	{
		return GetErrorStringWithSourceLocation();
	}
}

FString FShaderCompilerError::GetErrorString(bool bOmitLineMarker) const
{
	return bOmitLineMarker ? GetErrorStringWithSourceLocation() : GetErrorStringWithLineMarker();
}

static bool ExtractSourceLocationFromCompilerMessage(FString& CompilerMessage, FString& OutFilePath, int32& OutRow, int32& OutColumn, const TCHAR* LeftBracket, const TCHAR* MiddleBracket, const TCHAR* RightBracket)
{
	// Ignore ':' character from absolute paths in Windows format
	const int32 StartPosition = (CompilerMessage.Len() >= 3 && FChar::IsAlpha(CompilerMessage[0]) && CompilerMessage[1] == TEXT(':') && (CompilerMessage[2] == TEXT('/') || CompilerMessage[2] == TEXT('\\')) ? 3 : 0);

	const int32 LeftBracketLen = FCString::Strlen(LeftBracket);
	const int32 LeftPosition = CompilerMessage.Find(LeftBracket, ESearchCase::IgnoreCase, ESearchDir::FromStart, StartPosition);
	if (LeftPosition == INDEX_NONE || LeftPosition == StartPosition || LeftPosition + LeftBracketLen >= CompilerMessage.Len() || !FChar::IsDigit(CompilerMessage[LeftPosition + LeftBracketLen]))
	{
		return false;
	}

	const int32 MiddleBracketLen = FCString::Strlen(MiddleBracket);
	const int32 MiddlePosition = CompilerMessage.Find(MiddleBracket, ESearchCase::IgnoreCase, ESearchDir::FromStart, LeftPosition + LeftBracketLen);
	if (MiddlePosition == INDEX_NONE || MiddlePosition + MiddleBracketLen >= CompilerMessage.Len() || !FChar::IsDigit(CompilerMessage[MiddlePosition + MiddleBracketLen]))
	{
		return false;
	}

	const int32 RightBracketLen = FCString::Strlen(RightBracket);
	const int32 RightPosition = CompilerMessage.Find(RightBracket, ESearchCase::IgnoreCase, ESearchDir::FromStart, MiddlePosition + MiddleBracketLen);
	if (RightPosition == INDEX_NONE || RightPosition >= CompilerMessage.Len())
	{
		return false;
	}

	// Extract file path, row, and column from compiler message
	OutFilePath = CompilerMessage.Left(LeftPosition);
	LexFromString(OutRow, *CompilerMessage.Mid(LeftPosition + LeftBracketLen, MiddlePosition - LeftPosition - LeftBracketLen));
	LexFromString(OutColumn, *CompilerMessage.Mid(MiddlePosition + MiddleBracketLen, RightPosition - MiddlePosition - MiddleBracketLen));

	// Remove extracted information from compiler message
	CompilerMessage = CompilerMessage.Right(CompilerMessage.Len() - RightPosition - RightBracketLen);

	return true;
}

bool FShaderCompilerError::ExtractSourceLocation()
{
	// Ignore this call if a file path and line string is already provided
	if (!StrippedErrorMessage.IsEmpty() && ErrorVirtualFilePath.IsEmpty() && ErrorLineString.IsEmpty())
	{
		auto ExtractSourceLineFromStrippedErrorMessage = [this](const TCHAR* LeftBracket, const TCHAR* MiddleBracket, const TCHAR* RightBracket) -> bool
		{
			int32 Row = 0, Column = 0;
			if (ExtractSourceLocationFromCompilerMessage(StrippedErrorMessage, ErrorVirtualFilePath, Row, Column, LeftBracket, MiddleBracket, RightBracket))
			{
				// Format error line string to MSVC format to be able to jump to the source location with a double click in VisualStudio
				ErrorLineString = FString::Printf(TEXT("%d,%d"), Row, Column);
				return true;
			}
			return false;
		};

		// Extract from Clang format, e.g. "Filename:3:12: error:"
		if (ExtractSourceLineFromStrippedErrorMessage(TEXT(":"), TEXT(":"), TEXT(": ")))
		{
			return true;
		}

		// Extract from MSVC format, e.g. "Filename(3,12) : error: "
		if (ExtractSourceLineFromStrippedErrorMessage(TEXT("("), TEXT(","), TEXT(") : ")))
		{
			return true;
		}
	}
	return false;
}

FString FShaderCompilerError::GetShaderSourceFilePath() const
{
	// Always return error file path as-is if it doesn't denote a virtual path.
	// We don't wont to report errors when accessing a compile error's message.
	if (ErrorVirtualFilePath.IsEmpty() || ErrorVirtualFilePath[0] != TEXT('/'))
	{
		return ErrorVirtualFilePath;
	}
	else
	{
		return ::GetShaderSourceFilePath(ErrorVirtualFilePath, nullptr);
	}
}

const TMap<FString, FString>& AllShaderSourceDirectoryMappings()
{
	return GShaderSourceDirectoryMappings;
}

void ResetAllShaderSourceDirectoryMappings()
{
	GShaderSourceDirectoryMappings.Reset();
}

void AddShaderSourceDirectoryMapping(const FString& VirtualShaderDirectory, const FString& RealShaderDirectory)
{
	check(IsInGameThread());

	if (FPlatformProperties::RequiresCookedData() || !AllowShaderCompiling())
	{
		return;
	}

	// Do sanity checks of the virtual shader directory to map.
	checkf(
		VirtualShaderDirectory.StartsWith(TEXT("/")) &&
		!VirtualShaderDirectory.EndsWith(TEXT("/")) &&
		!VirtualShaderDirectory.Contains(FString(TEXT("."))),
		TEXT("VirtualShaderDirectory = \"%s\""),
		*VirtualShaderDirectory
	);

	// Detect collisions with any other mappings.
	check(!GShaderSourceDirectoryMappings.Contains(VirtualShaderDirectory));

	// Make sure the real directory to map exists.
	bool bDirectoryExists = FPaths::DirectoryExists(RealShaderDirectory);
	if (!bDirectoryExists)
	{
		UE_LOG(LogShaders, Log, TEXT("Directory %s"), *RealShaderDirectory);
	}
	checkf(bDirectoryExists, TEXT("FPaths::DirectoryExists(%s %s) %s"), *RealShaderDirectory, *FPaths::ConvertRelativePathToFull(RealShaderDirectory), FPlatformProcess::ComputerName());

	// Make sure the Generated directory does not exist, because is reserved for C++ generated shader source
	// by the FShaderCompilerEnvironment::IncludeVirtualPathToContentsMap member.
	checkf(!FPaths::DirectoryExists(RealShaderDirectory / TEXT("Generated")),
		TEXT("\"%s/Generated\" is not permitted to exist since C++ generated shader file would be mapped to this directory."), *RealShaderDirectory);

	UE_LOG(LogShaders, Log, TEXT("Mapping virtual shader directory %s to %s"),
		*VirtualShaderDirectory, *RealShaderDirectory);
	GShaderSourceDirectoryMappings.Add(VirtualShaderDirectory, RealShaderDirectory);
}

void FShaderCode::Compress(FName ShaderCompressionFormat, FOodleDataCompression::ECompressor InOodleCompressor, FOodleDataCompression::ECompressionLevel InOodleLevel)
{
	checkf(OptionalDataSize == -1, TEXT("FShaderCode::Compress() was called before calling FShaderCode::FinalizeShaderCode()"));

	check(ShaderCompressionFormat == NAME_Oodle); // We now force shaders to compress with oodle (even if they are uncompressed)

	TArray<uint8> Compressed;
	// conventional formats will fail if the compressed size isn't enough, Oodle needs a more precise estimate
	int32 CompressedSize = (ShaderCompressionFormat != NAME_Oodle) ? ShaderCodeWithOptionalData.Num() : FOodleDataCompression::CompressedBufferSizeNeeded(ShaderCodeWithOptionalData.Num());
	Compressed.AddUninitialized(CompressedSize);

	// non-Oodle format names use the old API, for NAME_Oodle we replace the call with the custom invocation
	bool bCompressed = false;
	if (ShaderCompressionFormat != NAME_Oodle)
	{
		bCompressed = FCompression::CompressMemory(ShaderCompressionFormat, Compressed.GetData(), CompressedSize, ShaderCodeWithOptionalData.GetData(), ShaderCodeWithOptionalData.Num(), COMPRESS_BiasSize);
	}
	else
	{
		CompressedSize = FOodleDataCompression::Compress(Compressed.GetData(), CompressedSize, ShaderCodeWithOptionalData.GetData(), ShaderCodeWithOptionalData.Num(),
			InOodleCompressor, InOodleLevel);
		bCompressed = CompressedSize != 0;
	}

	// there is code that assumes that if CompressedSize == CodeSize, the shader isn't compressed. Because of that, do not accept equal compressed size (very unlikely anyway)
	if (bCompressed && CompressedSize < ShaderCodeWithOptionalData.Num())
	{
		// cache the ShaderCodeSize since it will no longer possible to get it as the reader will fail to parse the compressed data
		FShaderCodeReader Wrapper(ShaderCodeWithOptionalData);
		ShaderCodeSize = Wrapper.GetShaderCodeSize();
		checkf(ShaderCodeSize >= 0, TEXT("Unable to determine ShaderCodeSize from uncompressed code"), ShaderCodeSize);

		// finalize the compression
		CompressionFormat = ShaderCompressionFormat;
		OodleCompressor = InOodleCompressor;
		OodleLevel = InOodleLevel;
		UncompressedSize = ShaderCodeWithOptionalData.Num();

		Compressed.SetNum(CompressedSize);
		ShaderCodeWithOptionalData = Compressed;
	}
}

FArchive& operator<<(FArchive& Ar, FShaderCode& Output)
{
	if (Ar.IsLoading())
	{
		Output.OptionalDataSize = -1;
	}
	else
	{
		Output.FinalizeShaderCode();
	}

	// Note: this serialize is used to pass between UE and the shader compile worker, recompile both when modifying
	Ar << Output.ShaderCodeWithOptionalData;
	Ar << Output.UncompressedSize;
	{
		FString CompressionFormatString(Output.CompressionFormat.ToString());
		Ar << CompressionFormatString;
		Output.CompressionFormat = FName(*CompressionFormatString);
	}
	Ar << reinterpret_cast<uint8&>(Output.OodleCompressor);
	Ar << reinterpret_cast<uint8&>(Output.OodleLevel);
	Ar << Output.ShaderCodeSize;
	checkf(Output.UncompressedSize == 0 || Output.ShaderCodeSize > 0, TEXT("FShaderCode::operator<<(): invalid shader code size for a compressed shader: ShaderCodeSize=%d, UncompressedSize=%d"), Output.ShaderCodeSize, Output.UncompressedSize);
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FShaderCompilerInput& Input)
{
	// Note: this serialize is used to pass between UE and the shader compile worker, recompile both when modifying
	Ar << Input.Target;
	{
		FString ShaderFormatString(Input.ShaderFormat.ToString());
		Ar << ShaderFormatString;
		Input.ShaderFormat = FName(*ShaderFormatString);
	}
	{
		FString CompressionFormatString(Input.CompressionFormat.ToString());
		Ar << CompressionFormatString;
		Input.CompressionFormat = FName(*CompressionFormatString);
	}
	{
		FString ShaderPlatformNameString(Input.ShaderPlatformName.ToString());
		Ar << ShaderPlatformNameString;
		Input.ShaderPlatformName = FName(*ShaderPlatformNameString);
	}
	Ar << Input.VirtualSourceFilePath;
	Ar << Input.EntryPointName;
	Ar << Input.ShaderName;
	Ar << Input.SupportedHardwareMask;
	Ar << Input.bCompilingForShaderPipeline;
	Ar << Input.bIncludeUsedOutputs;
	Ar << Input.bCachePreprocessed;
	Ar << Input.UsedOutputs;
	Ar << Input.DumpDebugInfoRootPath;
	Ar << Input.DumpDebugInfoPath;
	Ar << Input.DebugInfoFlags;
	Ar << Input.DebugExtension;
	Ar << Input.DebugGroupName;
	Ar << Input.DebugDescription;
	Ar << Input.Hash;
	if (Input.bCachePreprocessed)
	{
		Input.Environment.SerializeCompilationDependencies(Ar);
	}
	else
	{
		Ar << Input.Environment;
	}
	Ar << Input.ExtraSettings;
	Ar << reinterpret_cast<uint8&>(Input.OodleCompressor);
	Ar << reinterpret_cast<uint8&>(Input.OodleLevel);

	// Note: skipping Input.SharedEnvironment, which is handled by FShaderCompileUtilities::DoWriteTasks in order to maintain sharing

	return Ar;
}

FShaderCommonCompileJob::FInputHash FShaderPipelineCompileJob::GetInputHash()
{
	if (bInputHashSet)
	{
		return InputHash;
	}
	static_assert(sizeof(FShaderCommonCompileJob::FInputHash) == 32);
	int256 CombinedHash = 0u;
	for (int32 Index = 0; Index < StageJobs.Num(); ++Index)
	{
		if (StageJobs[Index])
		{
			const FShaderCommonCompileJob::FInputHash StageHash = StageJobs[Index]->GetInputHash();
			const FShaderCommonCompileJob::FInputHash::ByteArray& StageHashBytes = StageHash.GetBytes();
			static_assert(sizeof(StageHashBytes) == sizeof(int256));
			CombinedHash += int256(StageHashBytes, sizeof(StageHashBytes));
		}
	}

	InputHash = FShaderCommonCompileJob::FInputHash(reinterpret_cast<FShaderCommonCompileJob::FInputHash::ByteArray&>(*CombinedHash.GetBits()));
	bInputHashSet = true;
	return InputHash;
}

// The Id of 0 is reserved for global shaders
FThreadSafeCounter FShaderCommonCompileJob::JobIdCounter(2);

uint32 FShaderCommonCompileJob::GetNextJobId()
{
	uint32 Id = JobIdCounter.Increment();
	if (Id == UINT_MAX)
	{
		JobIdCounter.Set(2);
	}
	return Id;
}

FString FShaderCompileJobKey::ToString() const
{
	return FString::Printf(
		TEXT("ShaderType:%s VertexFactoryType:%s PermutationId:%d"),
		ShaderType ? ShaderType->GetName() : TEXT("None"),
		VFType ? VFType->GetName() : TEXT("None"),
		PermutationId);
}

struct FShaderVirtualFileContents
{
	const FString* Wide;
	const TArray<ANSICHAR>* Ansi;

	FShaderVirtualFileContents(const FString* InWide)
		: Wide(InWide), Ansi(nullptr)
	{}

	FShaderVirtualFileContents(const TArray<ANSICHAR>* InAnsi)
		: Wide(nullptr), Ansi(InAnsi)
	{}
};

FShaderCommonCompileJob::FInputHash FShaderCompileJob::GetInputHash()
{
	if (bInputHashSet)
	{
		return InputHash;
	}

	if (Input.bCachePreprocessed)
	{
		FMemoryHasherBlake3 Hasher;

		int32 FShaderCompilerOutputStructVersionLocal = FShaderCompilerOutputStructVersion;
		Hasher << FShaderCompilerOutputStructVersionLocal;

		uint32 FormatVersion = GetTargetPlatformManagerRef().ShaderFormatVersion(Input.ShaderFormat);
		Hasher << FormatVersion;
		
		FShaderTarget Target = Input.Target;
		Hasher << Target;
		Hasher << Input.EntryPointName;

		// Include this flag, so shader pipeline jobs get a different hash from single shader jobs, even if the preprocessed shader is otherwise the same.
		Hasher << Input.bCompilingForShaderPipeline;

		FShaderCompilerEnvironment MergedEnvironment = Input.Environment;
		if (Input.SharedEnvironment)
		{
			MergedEnvironment.Merge(*Input.SharedEnvironment);
		}
		MergedEnvironment.SerializeCompilationDependencies(Hasher);
		
		auto HashVersion = [&Hasher](const FString* VersionDirective)
		{
			check(VersionDirective && !VersionDirective->IsEmpty());
			// const_cast due to serialization API requiring non-const. better than not having const correctness in the API.
			Hasher << const_cast<FString&>(*VersionDirective);
		};

		PreprocessOutput.VisitDirectivesWithPrefix(TEXT("VERSION"), HashVersion);
		// const_cast due to serialization API requiring non-const. better than not having const correctness in the API.
		Hasher << PreprocessOutput.EditSource();
		if (SecondaryPreprocessOutput.IsValid())
		{
			Hasher << SecondaryPreprocessOutput->EditSource();
		}

		if (Input.RootParametersStructure)
		{
			FBlake3Hash LayoutSignature = Input.RootParametersStructure->GetLayoutSignature();
			Hasher << LayoutSignature;
		}

		InputHash = Hasher.Finalize();
	}
	else
	{
		auto SerializeInputs = [this](FArchive& Archive)
		{
			checkf(Archive.IsSaving() && !Archive.IsLoading(), TEXT("A loading archive is passed to FShaderCompileJob::GetInputHash(), this is not supported as it may corrupt its data"));

			int32 FShaderCompilerOutputStructVersionLocal = FShaderCompilerOutputStructVersion;
			Archive << FShaderCompilerOutputStructVersionLocal;

			uint32 FormatVersion = GetTargetPlatformManagerRef().ShaderFormatVersion(Input.ShaderFormat);
			Archive << FormatVersion;

			// Don't include debug group name in the hashing; this drastically worsens our cache hit rate
			FString DebugGroupNameTmp(MoveTemp(Input.DebugGroupName));
			Archive << Input;
			Input.DebugGroupName = MoveTemp(DebugGroupNameTmp);
			Input.Environment.SerializeEverythingButFiles(Archive);

			if (Input.RootParametersStructure)
			{
				FBlake3Hash LayoutSignature = Input.RootParametersStructure->GetLayoutSignature();
				Archive << LayoutSignature;
			}

			// hash the source file so changes to files during the development are picked up
			const FSHAHash& SourceHash = GetShaderFileHash(*Input.VirtualSourceFilePath, Input.Target.GetPlatform());
			Archive << const_cast<FSHAHash&>(SourceHash);

			// unroll the included files for the parallel processing.
			// These are temporary arrays that only exist for the ParallelFor
			TArray<const TCHAR*> IncludeVirtualPaths;
			TArray<FShaderVirtualFileContents> Contents;
			TArray<bool> OnlyHashIncludes;
			TArray<FBlake3Hash> Hashes;

			// while the contents of this is already hashed (included in Environment's operator<<()), we still need to account for includes in the generated files and hash them, too
			for (TMap<FString, FString>::TConstIterator It(Input.Environment.IncludeVirtualPathToContentsMap); It; ++It)
			{
				const FString& VirtualPath = It.Key();
				IncludeVirtualPaths.Add(*VirtualPath);
				Contents.Add(&It.Value());
				OnlyHashIncludes.Add(true);	// not hashing contents of the file itself, as it was included in Environment's operator<<()
				Hashes.AddDefaulted();
			}

			for (TMap<FString, FThreadSafeSharedAnsiStringPtr>::TConstIterator It(Input.Environment.IncludeVirtualPathToSharedContentsMap); It; ++It)
			{
				const FString& VirtualPath = It.Key();
				IncludeVirtualPaths.Add(*VirtualPath);
				check(It.Value());
				Contents.Add(&(*It.Value()));
				OnlyHashIncludes.Add(false);
				Hashes.AddDefaulted();
			}

			if (Input.SharedEnvironment)
			{
				Input.SharedEnvironment->SerializeEverythingButFiles(Archive);

				for (TMap<FString, FString>::TConstIterator It(Input.SharedEnvironment->IncludeVirtualPathToContentsMap); It; ++It)
				{
					const FString& VirtualPath = It.Key();
					IncludeVirtualPaths.Add(*VirtualPath);
					Contents.Add(&It.Value());
					OnlyHashIncludes.Add(true);	// not hashing contents of the file itself, as it was included in Environment's operator<<()
					Hashes.AddDefaulted();
				}

				for (TMap<FString, FThreadSafeSharedAnsiStringPtr>::TConstIterator It(Input.SharedEnvironment->IncludeVirtualPathToSharedContentsMap); It; ++It)
				{
					const FString& VirtualPath = It.Key();
					IncludeVirtualPaths.Add(*VirtualPath);
					check(It.Value());
					Contents.Add(&(*It.Value()));
					OnlyHashIncludes.Add(false);
					Hashes.AddDefaulted();
				}
			}

			check(IncludeVirtualPaths.Num() == Contents.Num());
			check(Contents.Num() == OnlyHashIncludes.Num());
			check(OnlyHashIncludes.Num() == Hashes.Num());

			EShaderPlatform Platform = Input.Target.GetPlatform();
			ParallelFor(Contents.Num(), [&IncludeVirtualPaths, &Contents, &OnlyHashIncludes, &Hashes, &Platform](int32 FileIndex)
				{
					FMemoryHasherBlake3 MemHasher;
					if (Contents[FileIndex].Wide)
					{
						HashShaderFileWithIncludes(MemHasher, IncludeVirtualPaths[FileIndex], *Contents[FileIndex].Wide, Platform, OnlyHashIncludes[FileIndex]);
					}
					else
					{
						// ANSI files are shared uniform buffer struct declarations (or generated stereo code), and never have includes, so we just need to hash the
						// single file contents.  Make sure that assumption hasn't been violated (this test costs less than 0.1% of GetInputHash, so might as well).
						check(FCStringAnsi::Strstr(Contents[FileIndex].Ansi->GetData(), "#include") == nullptr);

						MemHasher.Serialize(reinterpret_cast<void*>(const_cast<TCHAR*>(IncludeVirtualPaths[FileIndex])), FCString::Strlen(IncludeVirtualPaths[FileIndex]));
						MemHasher << const_cast<TArray<ANSICHAR>&>(*Contents[FileIndex].Ansi);
					}
					Hashes[FileIndex] = MemHasher.Finalize();
				},
				EParallelForFlags::Unbalanced
					);

			// include the hashes in the main hash (consider sorting them if includes are found to have a random order)
			for (int32 HashIndex = 0, NumHashes = Hashes.Num(); HashIndex < NumHashes; ++HashIndex)
			{
				Archive << Hashes[HashIndex];
			}
		};

		// use faster hasher that doesn't allocate memory
		FMemoryHasherBlake3 MemHasher;
		SerializeInputs(MemHasher);
		InputHash = MemHasher.Finalize();

		if (GShaderCompilerDumpCompileJobInputs)
		{
			TArray<uint8> MemoryBlob;
			FMemoryWriter MemWriter(MemoryBlob);

			SerializeInputs(MemWriter);

			FString IntermediateFormatPath = FPaths::ProjectSavedDir() / TEXT("ShaderJobInputs");
#if UE_BUILD_DEBUG
			FString TempPath = IntermediateFormatPath / TEXT("DebugEditor");
#else
			FString TempPath = IntermediateFormatPath / TEXT("DevelopmentEditor");
#endif
			IFileManager::Get().MakeDirectory(*TempPath, true);

			static int32 InputHashID = 0;
			FString FileName = Input.DebugGroupName.Replace(TEXT("/"), TEXT("_")).Replace(TEXT("<"), TEXT("_")).Replace(TEXT(">"), TEXT("_")).Replace(TEXT(":"), TEXT("_")).Replace(TEXT("|"), TEXT("_"))
				+ TEXT("-") + Input.EntryPointName;
			FString TempFile = TempPath / FString::Printf(TEXT("%s-%d.bin"), *FileName, InputHashID++);

			TUniquePtr<FArchive> DumpAr(IFileManager::Get().CreateFileWriter(*TempFile));
			DumpAr->Serialize(MemoryBlob.GetData(), MemoryBlob.Num());

			// as an additional debugging feature, make sure that the hash is the same as calculated by the memhasher
			FBlake3Hash Check = FBlake3::HashBuffer(MemoryBlob.GetData(), MemoryBlob.Num());
			if (Check != InputHash)
			{
				UE_LOG(LogShaders, Error, TEXT("Job input hash disagrees between FMemoryHasherSHA1 (%s) and FMemoryWriter + FSHA1 (%s, which was dumped to disk)"), *LexToString(InputHash), *LexToString(Check));
			}
		}
	}

	bInputHashSet = true;
	Input.Hash = InputHash;
	return InputHash;
}

void FShaderCompileJob::SerializeOutput(FArchive& Ar)
{
	double ActualCompileTime = 0.0;
	// Save the preprocess time as set in the job regardless of whether saving or loading - if loading from the cache and the preprocessed job
	// cache is enabled this job will have already run its own preprocessing and we want to track/aggregate this time properly.
	double ActualPreprocessTime = Output.PreprocessTime;
	if (Ar.IsSaving())
	{
		// Clear preprocess time and compile time when storing a job in the cache. This reduces storage requirements since these objects are
		// deduplicated based on a hash (and otherwise duplicate jobs will still differ in these values).
		ActualCompileTime = Output.CompileTime;
		Output.CompileTime = 0.0;
		Output.PreprocessTime = 0.0;
	}

	Ar << Output;
	// output hash is now serialized as part of the output, as the shader code is compressed in SCWs
	checkf(!Output.bSucceeded || Output.OutputHash != FSHAHash(), TEXT("Successful compile job does not have an OutputHash generated."));

	if (Ar.IsLoading())
	{
		bFinalized = true;
		bSucceeded = Output.bSucceeded;
	}
	else
	{
		// Restore the compile time for this job if we're saving to the cache.
		// Jobs that will be deserialized from the cache will have a compile time of 0.0
		Output.CompileTime = ActualCompileTime;
	}

	// Unconditionally restore the preprocess time for this job after saving to or loading from the cache.
	Output.PreprocessTime = ActualPreprocessTime;
}

void FShaderCompileJob::OnComplete()
{
	const IShaderFormat* ShaderFormat = GetTargetPlatformManagerRef().FindShaderFormat(Input.ShaderFormat);
	// For jobs using the preprocessed cache, we need to remap error messages whether or not the job was actually the one that ran
	// the compilation step. In addition since we always run preprocessing we set the total preprocess time accordingly.
	if (Input.bCachePreprocessed)
	{
		PreprocessOutput.RemapErrors(Output);
		Output.PreprocessTime = PreprocessOutput.ElapsedTime;
	}

	if (Input.NeedsOriginalShaderSource())
	{
		// Decompress the code if needed by debug info or source extraction
		PreprocessOutput.DecompressCode();
	}

	// dump debug info for the job at this point if the preprocessed cache is enabled
	// this ensures we get debug output for all jobs, including those that were found in the job cache,
	// or matched another in-flight job's hash and so could share its results
	if (Input.bCachePreprocessed 
		&& Input.DumpDebugInfoEnabled()
		// if we only want debug info for jobs which actually compiled, check the CompileTime
		// (jobs deserialized from the cache/wait list/ddc will have a compiletime of 0.0)
		&& (CVarDumpDebugInfoForCacheHits.GetValueOnAnyThread() || Output.CompileTime > 0.0f))
	{
		if (SecondaryPreprocessOutput.IsValid() && SecondaryOutput.IsValid())
		{
			ShaderFormat->OutputDebugData(Input, PreprocessOutput, *SecondaryPreprocessOutput, Output, *SecondaryOutput);
		}
		else
		{
			ShaderFormat->OutputDebugData(Input, PreprocessOutput, Output);
		}
	}
}

void FShaderCompileJob::AppendDebugName(FStringBuilderBase& OutName) const
{
	OutName << (Input.DumpDebugInfoPath.IsEmpty() ? Input.DebugGroupName : Input.DumpDebugInfoPath);
}

void FShaderCompileJob::SerializeWorkerOutput(FArchive& Ar)
{
	// Only serialize the modified source/entry point if either:
	// (a) this job is using the preprocessed cache and requires debug output (since this is done in the cooker process the modified source is needed outside of SCW)
	// or (b) the "extract shader source" setting is enabled (i.e. something upstream explicitly wants the final source passed to the compiler).
	Output.bSerializeModifiedSource = (Input.bCachePreprocessed && Input.DumpDebugInfoEnabled()) || Input.ExtraSettings.bExtractShaderSource;


	Ar << Output;

	bool bSecondaryOutput = SecondaryOutput.IsValid();
	Ar << bSecondaryOutput;

	if (bSecondaryOutput)
	{
		if (Ar.IsLoading())
		{
			SecondaryOutput = MakeUnique<FShaderCompilerOutput>();
		}
		Ar << *SecondaryOutput;
	}

	// edge case for backends which have implemented independent preprocessing API when the preprocessed cache is not enabled.
	// if no modifications have occurred as part of the compile step, we still need a copy of the source back in the cooker
	// if bExtractShaderSource is set, so explicitly serialize just that portion of the preprocess output struct here.
	if (Input.ExtraSettings.bExtractShaderSource && !Input.bCachePreprocessed && Output.ModifiedShaderSource.IsEmpty())
	{
		Ar << PreprocessOutput.EditSource();
	}

	bool bSucceededTemp = (bool)bSucceeded;
	Ar << bSucceededTemp;
	bSucceeded = bSucceededTemp;
}

void FShaderCompileJob::SerializeWorkerInput(FArchive& Ar)
{
	Ar << Input;

	if (Input.bCachePreprocessed)
	{
		Ar << PreprocessOutput;

		bool bSecondaryPreprocessOutput = SecondaryPreprocessOutput.IsValid();
		Ar << bSecondaryPreprocessOutput;

		if (bSecondaryPreprocessOutput)
		{
			if (Ar.IsLoading())
			{
				SecondaryPreprocessOutput = MakeUnique<FShaderPreprocessOutput>();
			}
			Ar << *SecondaryPreprocessOutput;
		}
	}
}

FStringView FShaderCompileJob::GetFinalSourceView() const
{
	 // any modifications to the source done as part of the compile step will be written to the "ModifiedShaderSource" field
	// always return empty string if source extraction was not requested; this will prevent bloat of material DDC data in the case where debug info is enabled 
	// or Output.ModifiedShaderSource is unset (since the preprocess output unstripped source will always be set)
	if (Input.ExtraSettings.bExtractShaderSource)
	{
		// if there are no such modifications, return the "unstripped" version of the source code (with comments & line directives maintained),
		// otherwise return whatever the final modified source is as input to the compiler by the backend.
		return Output.ModifiedShaderSource.IsEmpty() ? PreprocessOutput.GetUnstrippedSourceView() : FStringView(Output.ModifiedShaderSource);
	}
	else
	{
		return FStringView();
	}
}

FShaderPipelineCompileJob::FShaderPipelineCompileJob(int32 NumStages)
	: FShaderCommonCompileJob(Type, 0u, 0u, EShaderCompileJobPriority::Num)
{
	StageJobs.Empty(NumStages);
	for (int32 StageIndex = 0; StageIndex < NumStages; ++StageIndex)
	{
		StageJobs.Add(new FShaderCompileJob());
	}

	if (StageJobs.Num())
	{
		// Set this flag on first job in constructor, so it's included during input hash computation.  Flag is set conditionally for other stage jobs in CompileShaderPipeline.
		StageJobs[0]->Input.bCompilingForShaderPipeline = true;
	}
}

FShaderPipelineCompileJob::FShaderPipelineCompileJob(uint32 InHash, uint32 InId, EShaderCompileJobPriority InPriroity, const FShaderPipelineCompileJobKey& InKey) :
	FShaderCommonCompileJob(Type, InHash, InId, InPriroity),
	Key(InKey)
{
	const auto& Stages = InKey.ShaderPipeline->GetStages();
	StageJobs.Empty(Stages.Num());
	for (const FShaderType* ShaderType : Stages)
	{
		const FShaderCompileJobKey StageKey(ShaderType, InKey.VFType, InKey.PermutationId);
		StageJobs.Add(new FShaderCompileJob(StageKey.MakeHash(InId), InId, InPriroity, StageKey));
	}
	
	if (StageJobs.Num())
	{
		// Set this flag on first job in constructor, so it's included during input hash computation.  Flag is set conditionally for other stage jobs in CompileShaderPipeline.
		StageJobs[0]->Input.bCompilingForShaderPipeline = true;
	}
}

void FShaderPipelineCompileJob::SerializeOutput(FArchive& Ar)
{
	bool bAllStagesSucceeded = true;
	for (int32 Index = 0, Num = StageJobs.Num(); Index < Num; ++Index)
	{
		StageJobs[Index]->SerializeOutput(Ar);
		bAllStagesSucceeded = bAllStagesSucceeded && StageJobs[Index]->bSucceeded;
	}

	if (Ar.IsLoading())
	{
		bFinalized = true;
		bSucceeded = bAllStagesSucceeded;
	}
}

void FShaderPipelineCompileJob::OnComplete()
{
	for (int32 Index = 0, Num = StageJobs.Num(); Index < Num; ++Index)
	{
		StageJobs[Index]->OnComplete();
	}
}

void FShaderPipelineCompileJob::AppendDebugName(FStringBuilderBase& OutName) const
{
	for (int32 Index = 0, Num = StageJobs.Num(); Index < Num; ++Index)
	{
		StageJobs[Index]->AppendDebugName(OutName);
		OutName << TEXT("\n");
	}
}
