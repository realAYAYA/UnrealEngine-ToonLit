// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCore.cpp: Shader core module implementation.
=============================================================================*/

#include "ShaderCore.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/StringBuilder.h"
#include "Shader.h"
#include "ShaderCompilerCore.h"
#include "String/Find.h"
#include "VertexFactory.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "RHIShaderFormatDefinitions.inl"
#include "Compression/OodleDataCompression.h"
#if WITH_EDITOR
#include "Misc/CoreMisc.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/StringBuilder.h"
#endif

static TAutoConsoleVariable<int32> CVarShaderDevelopmentMode(
	TEXT("r.ShaderDevelopmentMode"),
	0,
	TEXT("0: Default, 1: Enable various shader development utilities, such as the ability to retry on failed shader compile, and extra logging as shaders are compiled."),
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

/** Protects GShaderFileCache from simultaneous access by multiple threads. */
FCriticalSection FileCacheCriticalSection;

/** The shader file cache, used to minimize shader file reads */
TMap<FString, FString> GShaderFileCache;

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

		bInitialized = true;
	}

	void UpdateIncludeDirectoryForPreviewPlatform(EShaderPlatform PreviewShaderPlatform, EShaderPlatform ParentShaderPlatform)
	{
		Platforms[PreviewShaderPlatform].IncludeDirectory = Platforms[ParentShaderPlatform].IncludeDirectory;
	}

	FSHAHash* FindHash(EShaderPlatform ShaderPlatform, const FString& VirtualFilePath)
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

	bool ShouldIgnoreInclude(const FString& VirtualFilePath, EShaderPlatform ShaderPlatform)
	{
		// Ignore only platform specific files, which won't be used by the target platform.
		if (VirtualFilePath.StartsWith(TEXT("/Engine/Private/Platform/"))
			|| VirtualFilePath.StartsWith(TEXT("/Engine/Public/Platform/"))
			|| VirtualFilePath.StartsWith(TEXT("/Platform/")))
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

	const FString& GetPlatformIncludeDirectory(EShaderPlatform ShaderPlatform)
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

/** Protects GShaderHashCache from simultaneous modification by multiple threads. Note that it can cover more than one method of the class, e.g. a block of code doing Find() then Add() can be guarded */
FCriticalSection GShaderHashAccessGuard;

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

static bool IsShaderCompilerConfigEnabledForPlatform(const IConsoleVariable* GlobalCvar, const TCHAR* Key, FName ShaderFormat)
{
	bool bEnabled = false;

	// First check the global cvar
	if (GlobalCvar && GlobalCvar->GetInt())
	{
		bEnabled = true;
	}
#if WITH_EDITOR
	else
	{
		// Then check the per platform settings.
		ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatformWithSupport(TEXT("ShaderFormat"), ShaderFormat);
		if (TargetPlatform)
		{
			if (FConfigCacheIni* PlatformConfig = TargetPlatform->GetConfigSystem())
			{
				// Override with a build machine specific setting, if present.
				bool QueryBaseConfig = GIsBuildMachine ? !PlatformConfig->GetBool(TEXT("ShaderCompiler_BuildMachine"), Key, bEnabled, GEngineIni) : true;
				if (QueryBaseConfig)
				{
					PlatformConfig->GetBool(TEXT("ShaderCompiler"), Key, bEnabled, GEngineIni);
				}
			}
		}
	}
#endif

	return bEnabled;
}

static bool GetShaderCompilerStringForPlatform(FString& OutputString, const IConsoleVariable* GlobalCvar, const TCHAR* Key, FName ShaderFormat)
{
	// First check the global cvar
	if (GlobalCvar)
	{
		OutputString = GlobalCvar->GetString();
	}

#if WITH_EDITOR
	if (OutputString.IsEmpty())
	{
		// Then check the per platform settings.
		ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatformWithSupport(TEXT("ShaderFormat"), ShaderFormat);
		if (TargetPlatform)
		{
			if (FConfigCacheIni* PlatformConfig = TargetPlatform->GetConfigSystem())
			{
				// Override with a build machine specific setting, if present.
				if (GIsBuildMachine)
				{
					OutputString = PlatformConfig->GetStr(TEXT("ShaderCompiler_BuildMachine"), Key, GEngineIni);
				}

				if (OutputString.IsEmpty())
				{
					OutputString = PlatformConfig->GetStr(TEXT("ShaderCompiler"), Key, GEngineIni);
				}
			}
		}
	}
#endif

	return !OutputString.IsEmpty();
}

struct FShaderSymbolSettingHelper
{
	const TCHAR* const SettingName;
	const IConsoleVariable* const SettingCVar;

	const bool bEnableLegacySettings = true;
public:
	FShaderSymbolSettingHelper(const TCHAR* InSettingName, bool bPlatformOnly = false)
		: SettingName(InSettingName)
		, SettingCVar(!bPlatformOnly ? IConsoleManager::Get().FindConsoleVariable(InSettingName) : nullptr)
	{
		check(SettingCVar || bPlatformOnly);
	}

	bool IsEnabled(FName ShaderFormat) const
	{
		return IsShaderCompilerConfigEnabledForPlatform(SettingCVar, SettingName, ShaderFormat);
	}

	bool GetString(FString& OutString, FName ShaderFormat) const
	{
		return GetShaderCompilerStringForPlatform(OutString, SettingCVar, SettingName, ShaderFormat);
	}
};

bool ShouldGenerateShaderSymbols(FName ShaderFormat)
{
	static const FShaderSymbolSettingHelper Symbols(TEXT("r.Shaders.Symbols"));
	static const FShaderSymbolSettingHelper GenerateSymbols(TEXT("r.Shaders.GenerateSymbols"), true);
	return Symbols.IsEnabled(ShaderFormat) || GenerateSymbols.IsEnabled(ShaderFormat);
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

void FShaderCompilerDefinitions::SetFloatDefine(const TCHAR* Name, float Value)
{
	// Make sure the printed value perfectly matches the given number
	FString Define = FString::Printf(TEXT("%#.9gf"), Value);
	Definitions.Add(Name, MoveTemp(Define));
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
		GetShaderIncludes(*VirtualFilePath, *VirtualFilePath, OutVirtualFilePaths, ShaderPlatform, DepthLimit, ShaderPlatformName);
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
		ShaderDir.RightInline(ShaderDir.Len() - CharIndex, false);
	}

	FString RelativeFilename = InFilename.Replace(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	// remove leading "/" because this makes path absolute on Linux (and Mac).
	if (RelativeFilename.Len() > 0 && RelativeFilename[0] == TEXT('/'))
	{
		RelativeFilename.RightInline(RelativeFilename.Len() - 1, false);
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
		RelativeFilename.MidInline(CharIndex, RelativeFilename.Len() - CharIndex, false);
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
			FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
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

bool LoadShaderSourceFile(const TCHAR* InVirtualFilePath, EShaderPlatform ShaderPlatform, FString* OutFileContents, TArray<FShaderCompilerError>* OutCompileErrors, const FName* ShaderPlatformName) // TODO: const FString&
{
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

		// Always substitute virtual platform path before accessing GShaderFileCache to get platform-specific file.
		ReplaceVirtualFilePathForShaderPlatform(VirtualFilePath, ShaderPlatform);

		// Fixup autogen file
		ReplaceVirtualFilePathForShaderAutogen(VirtualFilePath, ShaderPlatform, ShaderPlatformName);

		// Protect GShaderFileCache from simultaneous access by multiple threads
		FScopeLock ScopeLock(&FileCacheCriticalSection);

		FString* CachedFile = GShaderFileCache.Find(VirtualFilePath);

		//if this file has already been loaded and cached, use that
		if (CachedFile)
		{
			if (OutFileContents)
			{
				*OutFileContents = *CachedFile;
			}
			bResult = true;
		}
		else
		{
			FString ShaderFilePath = GetShaderSourceFilePath(VirtualFilePath, OutCompileErrors);

			// verify SHA hash of shader files on load. missing entries trigger an error
			FString FileContents;
			if (!ShaderFilePath.IsEmpty() && FFileHelper::LoadFileToString(FileContents, *ShaderFilePath, FFileHelper::EHashOptions::EnableVerify|FFileHelper::EHashOptions::ErrorMissingHash) )
			{
				//update the shader file cache
				GShaderFileCache.Add(VirtualFilePath, FileContents);

				if (OutFileContents)
				{
					*OutFileContents = MoveTemp(FileContents);
				}
				bResult = true;
			}
		}
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_LoadingShaderFiles,(float)ShaderFileLoadingTime);

	return bResult;
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

/**
 * Recursively populates IncludeFilenames with the unique include filenames found in the shader file named Filename.
 */
static void InternalGetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, const FString& FileContents, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, bool AddToIncludeFile, const FName* ShaderPlatformName)
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
					if (ExtractedIncludeFilename == TEXT("/Engine/Generated/Material.ush"))
					{
						ExtractedIncludeFilename = TEXT("/Engine/Private/MaterialTemplate.ush");
					}

					ReplaceVirtualFilePathForShaderPlatform(ExtractedIncludeFilename, ShaderPlatform);

					// Fixup autogen file
					ReplaceVirtualFilePathForShaderAutogen(ExtractedIncludeFilename, ShaderPlatform, ShaderPlatformName);

					// Ignore uniform buffer, vertex factory and instanced stereo includes
					bool bIgnoreInclude = ExtractedIncludeFilename.StartsWith(TEXT("/Engine/Generated/"));

					// Check virtual.
					bIgnoreInclude |= !CheckVirtualShaderFilePath(ExtractedIncludeFilename);

					// Include only platform specific files, which will be used by the target platform.
					{
						FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
						bIgnoreInclude = bIgnoreInclude || GShaderHashCache.ShouldIgnoreInclude(ExtractedIncludeFilename, ShaderPlatform);
					}


					//vertex factories need to be handled separately
					if (!bIgnoreInclude)
					{
						if (!IncludeVirtualFilePaths.Contains(ExtractedIncludeFilename))
						{
							FString IncludedFileContents;
							LoadShaderSourceFile(*ExtractedIncludeFilename, ShaderPlatform, &IncludedFileContents, nullptr, ShaderPlatformName);
							InternalGetShaderIncludes(EntryPointVirtualFilePath, *ExtractedIncludeFilename, IncludedFileContents, IncludeVirtualFilePaths, ShaderPlatform, DepthLimit - 1, true, ShaderPlatformName);
						}
					}
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

/**
 * Recursively populates IncludeFilenames with the unique include filenames found in the shader file named Filename.
 */
static void InternalGetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, bool AddToIncludeFile, const FName* ShaderPlatformName)
{
	FString FileContents;
	LoadShaderSourceFile(VirtualFilePath, ShaderPlatform, &FileContents, nullptr, ShaderPlatformName);

	InternalGetShaderIncludes(EntryPointVirtualFilePath, VirtualFilePath, FileContents, IncludeVirtualFilePaths, ShaderPlatform, DepthLimit, AddToIncludeFile, ShaderPlatformName);
}

void GetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, const FName* ShaderPlatformName)
{
	InternalGetShaderIncludes(EntryPointVirtualFilePath, VirtualFilePath, IncludeVirtualFilePaths, ShaderPlatform, DepthLimit, false, ShaderPlatformName);
}

void GetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, const FString& FileContents, TArray<FString>& IncludeVirtualFilePaths, EShaderPlatform ShaderPlatform, uint32 DepthLimit, const FName* ShaderPlatformName)
{
	InternalGetShaderIncludes(EntryPointVirtualFilePath, VirtualFilePath, FileContents, IncludeVirtualFilePaths, ShaderPlatform, DepthLimit, false, ShaderPlatformName);
}

void HashShaderFileWithIncludes(FArchive& HashingArchive, const TCHAR* VirtualFilePath, const FString& FileContents, EShaderPlatform ShaderPlatform, bool bOnlyHashIncludedFiles)
{
	auto HashSingleFile = [](FArchive& HashingArchive, const TCHAR* VirtualFilePath, EShaderPlatform ShaderPlatform, const FString& FileContents)
	{
		// first, a "soft" check
		bool bFoundInCache = false;
		{
			FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
			FSHAHash* CachedHash = GShaderHashCache.FindHash(ShaderPlatform, VirtualFilePath);
			// If a hash for this filename has been cached, use that
			if (CachedHash)
			{
				bFoundInCache = true;
				HashingArchive << *CachedHash;
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
	if ( UE_LOG_ACTIVE(LogTemp, Verbose) )
	{
		UE_LOG(LogTemp, Verbose, TEXT("Generating hash of file %s, "), VirtualFilePath);
	}
#endif
	for (int32 IncludeIndex = 0; IncludeIndex < IncludeVirtualFilePaths.Num(); IncludeIndex++)
	{
		// Load the include file and hash it
		FString IncludeFileContents;
		LoadShaderSourceFileChecked(*IncludeVirtualFilePaths[IncludeIndex], ShaderPlatform, IncludeFileContents);
		InOutHashState.UpdateWithString(*IncludeFileContents, IncludeFileContents.Len());
#if WITH_EDITOR &&  !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (UE_LOG_ACTIVE(LogTemp, Verbose))
		{
			uint8 HashBytes[20];
			FSHA1::HashBuffer(&InOutHashState, sizeof(FSHA1), HashBytes);
			
			UE_LOG(LogTemp, Verbose, TEXT("Processing include file for %s, %s, %s"), VirtualFilePath, *IncludeVirtualFilePaths[IncludeIndex], *BytesToHex(HashBytes, 20));
		}
#endif
	}

	// Load the source file and hash it
	FString FileContents;
	LoadShaderSourceFileChecked(VirtualFilePath, ShaderPlatform, FileContents);
	InOutHashState.UpdateWithString(*FileContents, FileContents.Len());
}

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
	FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
	{
		SCOPE_SECONDS_COUNTER(HashTime);

		FSHAHash* CachedHash = GShaderHashCache.FindHash(ShaderPlatform, VirtualFilePath);

		// If a hash for this filename has been cached, use that
		if (CachedHash)
		{
			return *CachedHash;
		}

		FSHA1 HashState;
		UpdateSingleShaderFilehash(HashState, VirtualFilePath, ShaderPlatform);
		HashState.Final();

		// Update the hash cache
		FSHAHash& NewHash = GShaderHashCache.AddHash(ShaderPlatform, VirtualFilePath);
		HashState.GetHash(&NewHash.Hash[0]);

#if WITH_EDITOR &&  !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogTemp, Verbose, TEXT("Final hash for file %s, %s"), VirtualFilePath,*BytesToHex(&NewHash.Hash[0], 20));
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
	FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
	{
		SCOPE_SECONDS_COUNTER(HashTime);

		FString Key;
		for (const FString& Filename : VirtualFilePaths)
		{
			Key += Filename;
		}

		FSHAHash* CachedHash = GShaderHashCache.FindHash(ShaderPlatform, Key);

		// If a hash for this filename has been cached, use that
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
		FSHAHash& NewHash = GShaderHashCache.AddHash(ShaderPlatform, Key);
		HashState.GetHash(&NewHash.Hash[0]);

		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HashingShaderFiles, (float)HashTime);
		return NewHash;
	}
}

void BuildShaderFileToUniformBufferMap(TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables, const FName* ShaderPlatformName)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		TArray<FString> ShaderSourceFiles;
		GetAllVirtualShaderSourcePaths(ShaderSourceFiles, GMaxRHIShaderPlatform, ShaderPlatformName);

		FScopedSlowTask SlowTask((float)ShaderSourceFiles.Num());

		// Cache UB access strings, make it case sensitive for faster search
		struct FShaderVariable
		{
			FShaderVariable(const TCHAR* ShaderVariable) :
				OriginalShaderVariable(ShaderVariable), 
				SearchKey(FString(ShaderVariable).ToUpper() + TEXT(".")),
				// MCPP inserts a space after a #define replacement, make sure we detect the uniform buffer reference
				SearchKeyWithSpace(FString(ShaderVariable).ToUpper() + TEXT(" ."))
			{}

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

		// Find for each shader file which UBs it needs
		for (int32 FileIndex = 0; FileIndex < ShaderSourceFiles.Num(); FileIndex++)
		{
			SlowTask.EnterProgressFrame(1);

 			FString ShaderFileContents;
			LoadShaderSourceFileChecked(*ShaderSourceFiles[FileIndex], GMaxRHIShaderPlatform, ShaderFileContents, ShaderPlatformName);

			// To allow case sensitive search which is way faster on some platforms (no need to look up locale, etc)
			ShaderFileContents.ToUpperInline();

			TArray<const TCHAR*>& ReferencedUniformBuffers = ShaderFileToUniformBufferVariables.FindOrAdd(ShaderSourceFiles[FileIndex]);

			for (int32 SearchKeyIndex = 0; SearchKeyIndex < SearchKeys.Num(); ++SearchKeyIndex)
			{
				// Searching for the uniform buffer shader variable being accessed with '.'
				if (ShaderFileContents.Contains(SearchKeys[SearchKeyIndex].SearchKey, ESearchCase::CaseSensitive)
					|| ShaderFileContents.Contains(SearchKeys[SearchKeyIndex].SearchKeyWithSpace, ESearchCase::CaseSensitive))
				{
					ReferencedUniformBuffers.AddUnique(SearchKeys[SearchKeyIndex].OriginalShaderVariable);
				}
			}
		}
	}
}

void InitializeShaderHashCache()
{
	FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
	GShaderHashCache.Initialize();
}

void UpdateIncludeDirectoryForPreviewPlatform(EShaderPlatform PreviewPlatform, EShaderPlatform ActualPlatform)
{
	FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
	GShaderHashCache.UpdateIncludeDirectoryForPreviewPlatform(PreviewPlatform, ActualPlatform);
}

void CheckShaderHashCacheInclude(const FString& VirtualFilePath, EShaderPlatform ShaderPlatform)
{
	FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
	bool bIgnoreInclude = GShaderHashCache.ShouldIgnoreInclude(VirtualFilePath, ShaderPlatform);

	checkf(!bIgnoreInclude,
		TEXT("Shader compiler is trying to include %s, which is not located in IShaderFormat::GetPlatformIncludeDirectory for %s."),
		*VirtualFilePath,
		*ShaderPlatformToShaderFormatName(ShaderPlatform).ToString());
}

void InitializeShaderTypes()
{
	UE_LOG(LogShaders, Log, TEXT("InitializeShaderTypes() begin"));

	LogShaderSourceDirectoryMappings();

	TMap<FString, TArray<const TCHAR*> > ShaderFileToUniformBufferVariables;
	BuildShaderFileToUniformBufferMap(ShaderFileToUniformBufferVariables);

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
void FlushShaderFileCache(const FName* ShaderPlatformName)
{
	UE_LOG(LogShaders, Log, TEXT("FlushShaderFileCache() begin"));

	{
		FScopeLock ShaderHashAccessLock(&GShaderHashAccessGuard);
		GShaderHashCache.Empty();
	}
	{
		FScopeLock ScopeLock(&FileCacheCriticalSection);
		GShaderFileCache.Empty();
	}

	if (!FPlatformProperties::RequiresCookedData())
	{
		LogShaderSourceDirectoryMappings();

		TMap<FString, TArray<const TCHAR*> > ShaderFileToUniformBufferVariables;
		BuildShaderFileToUniformBufferMap(ShaderFileToUniformBufferVariables, ShaderPlatformName);

		for (TLinkedList<FShaderPipelineType*>::TConstIterator It(FShaderPipelineType::GetTypeList()); It; It.Next())
		{
			const auto& Stages = It->GetStages();
			for (const FShaderType* ShaderType : Stages)
			{
				((FShaderType*)ShaderType)->FlushShaderFileCache(ShaderFileToUniformBufferVariables);
			}
		}

		for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
		{
			It->FlushShaderFileCache(ShaderFileToUniformBufferVariables);
		}

		for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
		{
			It->FlushShaderFileCache(ShaderFileToUniformBufferVariables);
		}
	}

	UE_LOG(LogShaders, Log, TEXT("FlushShaderFileCache() end"));
}

void GenerateReferencedUniformBuffers(
	const TCHAR* SourceFilename, 
	const TCHAR* ShaderTypeName, 
	const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables,
	TMap<const TCHAR*,FCachedUniformBufferDeclaration>& UniformBufferEntries)
{
	TArray<FString> FilesToSearch;
	GetShaderIncludes(SourceFilename, SourceFilename, FilesToSearch, GMaxRHIShaderPlatform);
	FilesToSearch.Add(SourceFilename);

	for (int32 FileIndex = 0; FileIndex < FilesToSearch.Num(); FileIndex++)
	{
		const TArray<const TCHAR*>& FoundUniformBufferVariables = ShaderFileToUniformBufferVariables.FindChecked(FilesToSearch[FileIndex]);

		for (int32 VariableIndex = 0; VariableIndex < FoundUniformBufferVariables.Num(); VariableIndex++)
		{
			UniformBufferEntries.Add(FoundUniformBufferVariables[VariableIndex], FCachedUniformBufferDeclaration());
		}
	}
}


/** Efficient lookup to find FShaderParametersMetadata members by name pointer */
class FShaderParameterMemberLookup 
{
public:
	FShaderParameterMemberLookup(TLinkedList<FShaderParametersMetadata*>& ShaderParameters)
	{
		for (FShaderParametersMetadata* Struct : ShaderParameters)
		{
			Map.Add(Struct->GetShaderVariableName(), Struct->GetMembers());
		}
	}

	const TConstArrayView<FShaderParametersMetadata::FMember>* FindMembersByPointer(const TCHAR* ShaderVariableName) const
	{
		return Map.Find(ShaderVariableName);
	}

private:
	TMap<const void*, TConstArrayView<FShaderParametersMetadata::FMember>> Map;
};

/** Cache providing a FShaderParameterMemberLookup for the current FShaderParametersMetadata::GetStructList */
class FFShaderParameterPointerLookupCache
{
public:
	TSharedPtr<const FShaderParameterMemberLookup> Get()
	{
		TLinkedList<FShaderParametersMetadata*>* CurrentHead = FShaderParametersMetadata::GetStructList();
		check(CurrentHead);

		{
			FReadScopeLock ReadScope(Lock);
			if (CurrentHead == CachedHead)
			{
				return CachedLookup;
			}
		}

		TSharedPtr<FShaderParameterMemberLookup> NewLookup = MakeShared<FShaderParameterMemberLookup>(*CurrentHead);

		FWriteScopeLock WriteScope(Lock);
		CachedHead = CurrentHead;
		CachedLookup = NewLookup;

		return NewLookup;
	}

private:
	FRWLock Lock;
	TLinkedList<FShaderParametersMetadata*>* CachedHead = nullptr;
	TSharedPtr<const FShaderParameterMemberLookup> CachedLookup;
};

static FFShaderParameterPointerLookupCache GShaderParameterMemberLookupCache;

void SerializeUniformBufferInfo(FShaderSaveArchive& Ar, const TSortedMap<const TCHAR*, FCachedUniformBufferDeclaration, FDefaultAllocator, FUniformBufferNameSortOrder>& UniformBufferEntries)
{
	TSharedPtr<const FShaderParameterMemberLookup> ShaderParameterMembers = GShaderParameterMemberLookupCache.Get();

	for (const TPair<const TCHAR*, FCachedUniformBufferDeclaration>& Entry : UniformBufferEntries)
	{
		if (const TConstArrayView<FShaderParametersMetadata::FMember>* Members = ShaderParameterMembers->FindMembersByPointer(Entry.Key))
		{
			// Serialize information about the struct layout so we can detect when it changes
			int32 NumMembers = Members->Num();
			// Serializing with NULL so that FShaderSaveArchive will record the length without causing an actual data serialization
			Ar.Serialize(nullptr, NumMembers);

			for (const FShaderParametersMetadata::FMember& Member : *Members)
			{
				// Note: Only comparing number of floats used by each member and type, so this can be tricked (eg. swapping two equal size and type members)
				int32 MemberSize = Member.GetNumColumns() * Member.GetNumRows();
				Ar.Serialize(nullptr, MemberSize);
				int32 MemberType = (int32)Member.GetBaseType();
				Ar.Serialize(nullptr, MemberType);
			}
		}
	}
}

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
	if (IFileManager::Get().FileExists(*ErrorVirtualFilePath))
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
	Ar << Input.SourceFilePrefix;
	Ar << Input.VirtualSourceFilePath;
	Ar << Input.EntryPointName;
	Ar << Input.ShaderName;
	Ar << Input.bSkipPreprocessedCache;
	Ar << Input.bCompilingForShaderPipeline;
	Ar << Input.bGenerateDirectCompileFile;
	Ar << Input.bIncludeUsedOutputs;
	Ar << Input.UsedOutputs;
	Ar << Input.DumpDebugInfoRootPath;
	Ar << Input.DumpDebugInfoPath;
	Ar << Input.DebugExtension;
	Ar << Input.DebugGroupName;
	Ar << Input.DebugDescription;
	Ar << Input.Environment;
	Ar << Input.ExtraSettings;
	Ar << reinterpret_cast<uint8&>(Input.OodleCompressor);
	Ar << reinterpret_cast<uint8&>(Input.OodleLevel);

	// Note: skipping Input.SharedEnvironment, which is handled by FShaderCompileUtilities::DoWriteTasks in order to maintain sharing

	return Ar;
}
