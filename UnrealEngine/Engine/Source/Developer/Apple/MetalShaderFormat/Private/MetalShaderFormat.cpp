// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalShaderFormat.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "ShaderCore.h"
#include "ShaderCodeArchive.h"
#include "ShaderPreprocessTypes.h"
#include "hlslcc.h"
#include "MetalShaderResources.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/Archive.h"
#include "Misc/ConfigCacheIni.h"
#include "MetalBackend.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "FileUtilities/ZipArchiveWriter.h"
#include "MetalShaderCompiler.h"
#include "DataDrivenShaderPlatformInfo.h"

DEFINE_LOG_CATEGORY(LogMetalCompilerSetup)
DEFINE_LOG_CATEGORY(LogMetalShaderCompiler)

#define WRITE_METAL_SHADER_SOURCE_ARCHIVE 0

// Set this define to get additional logging information about Metal toolchain setup.
#define CHECK_METAL_COMPILER_TOOLCHAIN_SETUP 0

extern bool PreprocessMetalShader(const FShaderCompilerInput& Input, const FShaderCompilerEnvironment& Environment, FShaderPreprocessOutput& PreprocessOutput);
extern void CompileMetalShader(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& InPreprocessOutput, FShaderCompilerOutput& Output);
extern void OutputMetalDebugData(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& PreprocessOutput, const FShaderCompilerOutput& Output);

extern bool StripShader_Metal(TArray<uint8>& Code, class FString const& DebugPath, bool const bNative);
extern uint64 AppendShader_Metal(class FString const& ArchivePath, const FSHAHash& Hash, TArray<uint8>& Code);
extern bool FinalizeLibrary_Metal(class FName const& Format, class FString const& ArchivePath, class FString const& LibraryPath, TSet<uint64> const& Shaders, class FString const& DebugOutputDir);

/** Version for shader format, this becomes part of the DDC key. */
static const FGuid UE_SHADER_METAL_VER = FGuid("282D7BF5-C5F8-47B7-B065-E165D60FC17B");

class FMetalShaderFormat : public IShaderFormat
{
public:
	FMetalShaderFormat()
	{
		FMetalCompilerToolchain::CreateAndInit();
	}
	virtual ~FMetalShaderFormat()
	{
		FMetalCompilerToolchain::Destroy();
	}
	virtual uint32 GetVersion(FName Format) const override final
	{
		// If there's no compiler on this machine, this is irrelevant so just return 0
		if (!FMetalCompilerToolchain::Get()->IsCompilerAvailable())
		{
			return 0;
		}

		bool bUseFullMetalVersion = false;
		EShaderPlatform ShaderPlatform = FMetalCompilerToolchain::MetalShaderFormatToLegacyShaderPlatform(Format);

		if (FMetalCompilerToolchain::IsMobile(ShaderPlatform))
		{
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("UseFullMetalVersionInShaderVersion"), bUseFullMetalVersion, GEngineIni);
		}
		else
		{
			GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("UseFullMetalVersionInShaderVersion"), bUseFullMetalVersion, GEngineIni);
		}

		FMetalCompilerToolchain::PackedVersion MetalVersionNumber = FMetalCompilerToolchain::Get()->GetCompilerVersion(ShaderPlatform);
		uint16 HashValue = MetalVersionNumber.Major;

		if (bUseFullMetalVersion)
		{
			// Use entire Metal version if .ini settings instruct us to do so (e.g. p4 dev build)
			HashValue ^= MetalVersionNumber.Minor;
			HashValue ^= MetalVersionNumber.Patch;
		}
		else
		{
			// Only use Metal major version (e.g. Installed build)
			// Since Metal minor/patch version changes every Xcode minor version, we don't want users to rebuild shaders for every minor version update
		}

		uint32 Result = GetTypeHash(HashValue);

		Result = HashCombine(Result, GetTypeHash(HLSLCC_VersionMinor));
		Result = HashCombine(Result, GetTypeHash(UE_SHADER_METAL_VER));

		return Result;
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override final
	{
		OutFormats.Add(NAME_SF_METAL);
		OutFormats.Add(NAME_SF_METAL_MRT);
		OutFormats.Add(NAME_SF_METAL_TVOS);
		OutFormats.Add(NAME_SF_METAL_MRT_TVOS);
		OutFormats.Add(NAME_SF_METAL_SM5);
		OutFormats.Add(NAME_SF_METAL_SM6);
		OutFormats.Add(NAME_SF_METAL_SIM);
		OutFormats.Add(NAME_SF_METAL_MACES3_1);
		OutFormats.Add(NAME_SF_METAL_MRT_MAC);
	}

	void CheckShaderFormat(FName Format) const
	{
		check(Format == NAME_SF_METAL
			|| Format == NAME_SF_METAL_MRT
			|| Format == NAME_SF_METAL_TVOS
			|| Format == NAME_SF_METAL_MRT_TVOS
			|| Format == NAME_SF_METAL_SM5
			|| Format == NAME_SF_METAL_SM6
			|| Format == NAME_SF_METAL_SIM
			|| Format == NAME_SF_METAL_MACES3_1
			|| Format == NAME_SF_METAL_MRT_MAC);
	}

	virtual bool PreprocessShader(
		const FShaderCompilerInput& Input,
		const FShaderCompilerEnvironment& Environment,
		FShaderPreprocessOutput& PreprocessOutput) const override final
	{
		CheckShaderFormat(Input.ShaderFormat);
		return PreprocessMetalShader(Input, Environment, PreprocessOutput);
	}

	virtual void CompilePreprocessedShader(
		const FShaderCompilerInput& Input,
		const FShaderPreprocessOutput& PreprocessOutput,
		FShaderCompilerOutput& Output,
		const FString& WorkingDirectory) const override final
	{
		CheckShaderFormat(Input.ShaderFormat);
		CompileMetalShader(Input, PreprocessOutput, Output);
	}

	virtual void OutputDebugData(
		const FShaderCompilerInput& Input,
		const FShaderPreprocessOutput& PreprocessOutput,
		const FShaderCompilerOutput& Output) const override final
	{
		OutputMetalDebugData(Input, PreprocessOutput, Output);
	}

	virtual bool CanStripShaderCode(bool const bNativeFormat) const override final
	{
		return CanCompileBinaryShaders() && bNativeFormat;
	}

	virtual bool StripShaderCode( TArray<uint8>& Code, FString const& DebugOutputDir, bool const bNative ) const override final
	{
		return StripShader_Metal(Code, DebugOutputDir, bNative);
    }

	virtual bool SupportsShaderArchives() const override 
	{ 
		return CanCompileBinaryShaders();
	}

	virtual bool CreateShaderArchive(FString const& LibraryName,
		FName ShaderFormatAndShaderPlatformName,
		const FString& WorkingDirectory,
		const FString& OutputDir,
		const FString& DebugOutputDir,
		const FSerializedShaderArchive& InSerializedShaders,
		const TArray<TArray<uint8>>& ShaderCode,
		TArray<FString>* OutputFiles) const override final
	{
		const int32 NumShadersPerLibrary = 10000;
		check(LibraryName.Len() > 0);

		TArray<FString> Components;
		FString ShaderPlatform = ShaderFormatAndShaderPlatformName.ToString();
		ShaderPlatform.ParseIntoArray(Components, TEXT("-"));
		check(Components.Num() == 2);
		FName ShaderFormatName(Components[0]);

		check(ShaderFormatName == NAME_SF_METAL || ShaderFormatName == NAME_SF_METAL_MRT || ShaderFormatName == NAME_SF_METAL_TVOS || ShaderFormatName == NAME_SF_METAL_MRT_TVOS || ShaderFormatName == NAME_SF_METAL_SM5 || ShaderFormatName == NAME_SF_METAL_SM6 || ShaderFormatName == NAME_SF_METAL_SIM || ShaderFormatName == NAME_SF_METAL_MACES3_1 || ShaderFormatName == NAME_SF_METAL_MRT_MAC);

		const FString ArchivePath = (WorkingDirectory / ShaderFormatAndShaderPlatformName.GetPlainNameString());
		IFileManager::Get().DeleteDirectory(*ArchivePath, false, true);
		IFileManager::Get().MakeDirectory(*ArchivePath);

		FSerializedShaderArchive SerializedShaders(InSerializedShaders);
		check(SerializedShaders.GetNumShaders() == ShaderCode.Num());

		TArray<uint8> StrippedShaderCode;
		TArray<uint8> TempShaderCode;

		TArray<TSet<uint64>> SubLibraries;

		for (int32 ShaderIndex = 0; ShaderIndex < SerializedShaders.GetNumShaders(); ++ShaderIndex)
		{
			SerializedShaders.DecompressShader(ShaderIndex, ShaderCode, TempShaderCode);
			StripShader_Metal(TempShaderCode, DebugOutputDir, true);

			uint64 ShaderId = AppendShader_Metal(ArchivePath, SerializedShaders.ShaderHashes[ShaderIndex], TempShaderCode);
			uint32 LibraryIndex = ShaderIndex / NumShadersPerLibrary;

			if (ShaderId)
			{
				if (SubLibraries.Num() <= (int32)LibraryIndex)
				{
					SubLibraries.Add(TSet<uint64>());
				}
				SubLibraries[LibraryIndex].Add(ShaderId);
			}

			FShaderCodeEntry& ShaderEntry = SerializedShaders.ShaderEntries[ShaderIndex];
			ShaderEntry.Size = TempShaderCode.Num();
			ShaderEntry.UncompressedSize = TempShaderCode.Num();

			StrippedShaderCode.Append(TempShaderCode);
		}

		SerializedShaders.Finalize();

		bool bOK = false;
		FString LibraryPlatformName = FString::Printf(TEXT("%s_%s"), *LibraryName, *ShaderFormatAndShaderPlatformName.GetPlainNameString());
		LibraryPlatformName.ToLowerInline();
		volatile int32 CompiledLibraries = 0;
		TArray<FGraphEventRef> Tasks;

		for (uint32 Index = 0; Index < (uint32)SubLibraries.Num(); Index++)
		{
			TSet<uint64>& PartialShaders = SubLibraries[Index];

			FString LibraryPath = (OutputDir / LibraryPlatformName) + FString::Printf(TEXT(".%d"), Index) + FMetalCompilerToolchain::MetalLibraryExtension;
			if (OutputFiles)
			{
				OutputFiles->Add(LibraryPath);
			}

			// Enqueue the library compilation as a task so we can go wide
			FGraphEventRef CompletionFence = FFunctionGraphTask::CreateAndDispatchWhenReady([ShaderFormatName, ArchivePath, LibraryPath, PartialShaders, DebugOutputDir, &CompiledLibraries]()
				{
					if (FinalizeLibrary_Metal(ShaderFormatName, ArchivePath, LibraryPath, PartialShaders, DebugOutputDir))
					{
						FPlatformAtomics::InterlockedIncrement(&CompiledLibraries);
					}
				}, TStatId(), NULL, ENamedThreads::AnyThread);

			Tasks.Add(CompletionFence);
		}

#if WITH_ENGINE
		FGraphEventRef DebugDataCompletionFence = FFunctionGraphTask::CreateAndDispatchWhenReady([ShaderFormatAndShaderPlatformName, OutputDir, LibraryPlatformName, DebugOutputDir]()
			{
				//TODO add a check in here - this will only work if we have shader archiving with debug info set.

				//We want to archive all the metal shader source files so that they can be unarchived into a debug location
				//This allows the debugging of optimised metal shaders within the xcode tool set
				//Currently using the 'tar' system tool to create a compressed tape archive

				//Place the archive in the same position as the .metallib file
				FString CompressedDir = (OutputDir / TEXT("../MetaData/ShaderDebug/"));
				IFileManager::Get().MakeDirectory(*CompressedDir, true);

				FString CompressedPath = (CompressedDir / LibraryPlatformName) + TEXT(".zip");

				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				IFileHandle* ZipFile = PlatformFile.OpenWrite(*CompressedPath);
				if (ZipFile)
				{
					FZipArchiveWriter* ZipWriter = new FZipArchiveWriter(ZipFile);

					//Find the metal source files
					TArray<FString> FilesToArchive;
					IFileManager::Get().FindFilesRecursive(FilesToArchive, *DebugOutputDir, TEXT("*.metal"), true, false, false);

					//Write the local file names into the target file
					const FString DebugDir = DebugOutputDir / *ShaderFormatAndShaderPlatformName.GetPlainNameString();

					for (FString FileName : FilesToArchive)
					{
						TArray<uint8> FileData;
						FFileHelper::LoadFileToArray(FileData, *FileName);
						FPaths::MakePathRelativeTo(FileName, *DebugDir);

						ZipWriter->AddFile(FileName, FileData, FDateTime::Now());
					}

					delete ZipWriter;
					ZipWriter = nullptr;
				}
				else
				{
					UE_LOG(LogShaders, Error, TEXT("Failed to create Metal debug .zip output file \"%s\". Debug .zip export will be disabled."), *CompressedPath);
				}
			}, TStatId(), NULL, ENamedThreads::AnyThread);
		Tasks.Add(DebugDataCompletionFence);
#endif // WITH_ENGINE

		// Wait for tasks
		for (auto& Task : Tasks)
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
		}

		if (CompiledLibraries == SubLibraries.Num())
		{
			FString BinaryShaderFile = (OutputDir / LibraryPlatformName) + FMetalCompilerToolchain::MetalMapExtension;
			BinaryShaderFile.ToLowerInline();
			FArchive* BinaryShaderAr = IFileManager::Get().CreateFileWriter(*BinaryShaderFile);
			if (BinaryShaderAr != NULL)
			{
				FMetalShaderLibraryHeader Header;
				Header.Format = ShaderFormatName.GetPlainNameString();
				Header.NumLibraries = SubLibraries.Num();
				Header.NumShadersPerLibrary = NumShadersPerLibrary;

				*BinaryShaderAr << Header;
				*BinaryShaderAr << SerializedShaders;
				*BinaryShaderAr << StrippedShaderCode;

				BinaryShaderAr->Flush();
				delete BinaryShaderAr;

				if (OutputFiles)
				{
					OutputFiles->Add(BinaryShaderFile);
				}

				bOK = true;
			}
		}

		return bOK;

		//Map.Format = Format.GetPlainNameString();
	}

	virtual void ModifyShaderCompilerInput(FShaderCompilerInput& Input) const override
	{
		// Work out which standard we need, this is dependent on the shader platform.
		// TODO: Read from toolchain class
		const bool bIsMobile = FMetalCompilerToolchain::Get()->IsMobile((EShaderPlatform)Input.Target.Platform);
		if (bIsMobile)
		{
			Input.Environment.SetDefine(TEXT("IOS"), 1);
		}
		else
		{
			Input.Environment.SetDefine(TEXT("MAC"), 1);
		}

		Input.Environment.SetDefine(TEXT("COMPILER_METAL"), 1);

		if (Input.ShaderFormat == NAME_SF_METAL || Input.ShaderFormat == NAME_SF_METAL_TVOS)
		{
			Input.Environment.SetDefine(TEXT("METAL_PROFILE"), 1);
		}
		else if (Input.ShaderFormat == NAME_SF_METAL_SIM)
		{
			Input.Environment.SetDefine(TEXT("METAL_PROFILE"), 1);
		}
		else if (Input.ShaderFormat == NAME_SF_METAL_MRT || Input.ShaderFormat == NAME_SF_METAL_MRT_TVOS)
		{
			Input.Environment.SetDefine(TEXT("METAL_MRT_PROFILE"), 1);
		}
		else if (Input.ShaderFormat == NAME_SF_METAL_MACES3_1)
		{
			Input.Environment.SetDefine(TEXT("METAL_PROFILE"), 1);
		}
		else if (Input.ShaderFormat == NAME_SF_METAL_SM5)
		{
			Input.Environment.SetDefine(TEXT("METAL_SM5_PROFILE"), 1);
			Input.Environment.SetDefine(TEXT("USING_VERTEX_SHADER_LAYER"), 1);
		}
		else if (Input.ShaderFormat == NAME_SF_METAL_SM6)
		{
			Input.Environment.SetDefine(TEXT("METAL_SM6_PROFILE"), 1);
			Input.Environment.SetDefine(TEXT("USING_VERTEX_SHADER_LAYER"), 1);
		}
		else if (Input.ShaderFormat == NAME_SF_METAL_MRT_MAC)
		{
			Input.Environment.SetDefine(TEXT("METAL_MRT_PROFILE"), 1);
		}

		Input.Environment.SetDefine(TEXT("COMPILER_HLSLCC"), 2);
#if UE_METAL_USE_METAL_SHADER_CONVERTER
		const bool bUseMetalShaderConverter = Input.Target.GetPlatform() == EShaderPlatform::SP_METAL_SM6
		&& RHIGetBindlessSupport(EShaderPlatform::SP_METAL_SM6) != ERHIBindlessSupport::Unsupported;
		
		if (bUseMetalShaderConverter)
		{
			Input.Environment.SetDefine(TEXT("COMPILER_METAL_SHADER_CONVERTER"), 1);
		}
#endif
		
#if !UE_METAL_USE_METAL_SHADER_CONVERTER
		if (Input.Environment.FullPrecisionInPS || (IsValidRef(Input.SharedEnvironment) && Input.SharedEnvironment->FullPrecisionInPS))
		{
			Input.Environment.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
		}
#else
		// We can use 16bits types with Msc (since we do not use the frontend).
		if (Input.Environment.CompilerFlags.Contains(CFLAG_AllowRealTypes))
		{
			Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_REAL_TYPES"), 1);
		}
#endif
		if (Input.Environment.CompilerFlags.Contains(CFLAG_AvoidFlowControl)
			|| Input.Environment.CompilerFlags.Contains(CFLAG_PreferFlowControl))
		{
			Input.Environment.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)0);
		}
		else
		{
			Input.Environment.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);
		}

		bool bUsesInlineRayTracing = Input.Environment.CompilerFlags.Contains(CFLAG_InlineRayTracing);
		if (bUsesInlineRayTracing)
		{
			Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_INLINE_RAY_TRACING"), 1);
		}

		Input.Environment.SetDefine(TEXT("COMPILER_SUPPORTS_DUAL_SOURCE_BLENDING_SLOT_DECORATION"), (uint32)1);
	}
	
	virtual bool CanCompileBinaryShaders() const override final
	{
#if PLATFORM_MAC
		return FPlatformMisc::IsSupportedXcodeVersionInstalled();
#else
		return FMetalCompilerToolchain::Get()->IsCompilerAvailable();
#endif
	}
	virtual const TCHAR* GetPlatformIncludeDirectory() const
	{
		return TEXT("Metal");
	}
};

/**
 * Module for Metal shaders
 */

static IShaderFormat* Singleton = nullptr;

class FMetalShaderFormatModule : public IShaderFormatModule
{
public:
	virtual ~FMetalShaderFormatModule()
	{
		
		Singleton = nullptr;
	}

	virtual IShaderFormat* GetShaderFormat()
	{
		return Singleton;
	}

	virtual void StartupModule() override
	{
		Singleton = new FMetalShaderFormat();
	}

	virtual void ShutdownModule() override
	{
		delete Singleton;
		Singleton = nullptr;
	}
};

IMPLEMENT_MODULE(FMetalShaderFormatModule, MetalShaderFormat);

static FMetalCompilerToolchain::EMetalToolchainStatus ParseCompilerVersionAndTarget(const FString& OutputOfMetalDashV, FString& VersionString, FMetalCompilerToolchain::PackedVersion& PackedVersionNumber, FMetalCompilerToolchain::PackedVersion& PackedTargetNumber)
{
	/*
		Output of metal -v might look like this:
		Apple LLVM version 902.11 (metalfe-902.11.1)
		Target: air64-apple-darwin19.5.0
		Thread model: posix
		InstalledDir: C:\Program Files\Metal Developer Tools\ios\bin
	*/

	TArray<FString> Lines;
	OutputOfMetalDashV.ParseIntoArrayLines(Lines);
	int32 VersionLineIndex = 0;
	{
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].StartsWith(TEXT("Apple ")) && Lines[Index].Contains(TEXT(" version ")) && Lines[Index].EndsWith(TEXT(")")))
			{
				VersionLineIndex = Index;
				break;
			}
		}

		VersionString = Lines[VersionLineIndex];
		FString& Version = Lines[VersionLineIndex];
		check(!Version.IsEmpty());

		int32 Major = 0, Minor = 0;
		int32 NumResults = 0;
#if !PLATFORM_WINDOWS
		char AppleToolName[PATH_MAX] = { '\0' };
		char SupplementaryVersionName[PATH_MAX] = { '\0' };
		NumResults = sscanf(TCHAR_TO_ANSI(*Version), "Apple %s version %d.%d (metalfe-%s)", AppleToolName, &Major, &Minor, SupplementaryVersionName);
#else
		TCHAR AppleToolName[WINDOWS_MAX_PATH] = { '\0' };
		TCHAR SupplementaryVersionName[WINDOWS_MAX_PATH] = { '\0' };
		NumResults = swscanf_s(*Version, TEXT("Apple %ls version %d.%d (metalfe-%ls)"), AppleToolName, WINDOWS_MAX_PATH, &Major, &Minor, SupplementaryVersionName, WINDOWS_MAX_PATH);
#endif
		if (NumResults != 4)
		{
			UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Metal version string format unrecoginzed"));
			UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Expecting: Apple LLVM version 902.11 (metalfe-902.11.1)"));
			UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Obtained: %s"), *Version);
		}
		
		PackedVersionNumber.Major = Major;
		PackedVersionNumber.Minor = Minor;
		// The version name in brackets is too irregular to extract a useful patch version
		// Sometimes (metalfe-31001.667.2), sometimes (metalfe-31001.643.2.1), sometimes (metalfe-31001.362-windows)
		PackedVersionNumber.Patch = 0;
	}

	if (PackedVersionNumber.Version == 0)
	{
		return FMetalCompilerToolchain::EMetalToolchainStatus::CouldNotParseCompilerVersion;
	}

	{
		FString& FormatVersion = Lines[VersionLineIndex + 1];
		int32 Major = 0, Minor = 0, Patch = 0;
		int32 NumResults = 0;
#if !PLATFORM_WINDOWS
		NumResults = sscanf(TCHAR_TO_ANSI(*FormatVersion), "Target: air64-apple-darwin%d.%d.%d", &Major, &Minor, &Patch);
#else
		NumResults = swscanf_s(*FormatVersion, TEXT("Target: air64-apple-darwin%d.%d.%d"), &Major, &Minor, &Patch);
#endif
		PackedTargetNumber.Major = Major;
		PackedTargetNumber.Minor = Minor;
		PackedTargetNumber.Patch = Patch;
	}

	if (PackedTargetNumber.Version == 0)
	{
		return FMetalCompilerToolchain::EMetalToolchainStatus::CouldNotParseTargetVersion;
	}

	return FMetalCompilerToolchain::EMetalToolchainStatus::Success;
}

static FMetalCompilerToolchain::EMetalToolchainStatus ParseLibraryToolpath(const FString& OutputOfMetalSearchDirs, FString& LibraryPath)
{
	static FString LibraryPrefix(TEXT("libraries: =%s"));

	TArray<FString> Lines;
	OutputOfMetalSearchDirs.ParseIntoArrayLines(Lines);
	{
		int32 LibrariesLineIndex = 0;
		
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].StartsWith(TEXT("libraries: =")))
			{
				LibrariesLineIndex = Index;
				break;
			}
		}

		FString& LibraryLine = Lines[LibrariesLineIndex];

		LibraryPath = LibraryLine.RightChop(LibraryPrefix.Len());

		if (!FPaths::DirectoryExists(LibraryPath))
		{
			return FMetalCompilerToolchain::EMetalToolchainStatus::CouldNotFindMetalStdLib;
		}

		FPaths::Combine(LibraryPath, TEXT("include"), TEXT("metal"));

		if (!FPaths::DirectoryExists(LibraryPath))
		{
			return FMetalCompilerToolchain::EMetalToolchainStatus::CouldNotFindMetalStdLib;
		}
	}

	return FMetalCompilerToolchain::EMetalToolchainStatus::Success;
}

FMetalCompilerToolchain* FMetalCompilerToolchain::Singleton = nullptr;
FString FMetalCompilerToolchain::MetalExtention(TEXT(".metal"));
FString FMetalCompilerToolchain::MetalLibraryExtension(TEXT(".metallib"));
FString FMetalCompilerToolchain::MetalObjectExtension(TEXT(".air"));
#if PLATFORM_WINDOWS
FString FMetalCompilerToolchain::MetalFrontendBinary(TEXT("metal.exe"));
FString FMetalCompilerToolchain::MetalArBinary(TEXT("metal-ar.exe"));
FString FMetalCompilerToolchain::MetalLibraryBinary(TEXT("metallib.exe"));
FString FMetalCompilerToolchain::AirPackBinary(TEXT("air-pack.exe"));
#else
FString FMetalCompilerToolchain::MetalFrontendBinary(TEXT("metal"));
FString FMetalCompilerToolchain::MetalArBinary(TEXT("metal-ar"));
FString FMetalCompilerToolchain::MetalLibraryBinary(TEXT("metallib"));
FString FMetalCompilerToolchain::AirPackBinary(TEXT("air-pack"));
#endif

FString FMetalCompilerToolchain::MetalMapExtension(TEXT(".metalmap"));

FString FMetalCompilerToolchain::XcrunPath(TEXT("/usr/bin/xcrun"));
FString FMetalCompilerToolchain::MetalMacSDK(TEXT("macosx"));
FString FMetalCompilerToolchain::MetalMobileSDK(TEXT("iphoneos"));

FString FMetalCompilerToolchain::DefaultWindowsToolchainPath(TEXT("c:/Program Files/Metal Developer Tools"));

// Static methods

void FMetalCompilerToolchain::CreateAndInit()
{
	Singleton = new FMetalCompilerToolchain;
	Singleton->Init();
}

void FMetalCompilerToolchain::Destroy()
{
	Singleton->Teardown();
	delete Singleton;
	Singleton = nullptr;
}

EShaderPlatform FMetalCompilerToolchain::MetalShaderFormatToLegacyShaderPlatform(FName ShaderFormat)
{
	if (ShaderFormat == NAME_SF_METAL)				return SP_METAL;
	if (ShaderFormat == NAME_SF_METAL_MRT)			return SP_METAL_MRT;
	if (ShaderFormat == NAME_SF_METAL_TVOS)			return SP_METAL_TVOS;
	if (ShaderFormat == NAME_SF_METAL_MRT_TVOS)		return SP_METAL_MRT_TVOS;
	if (ShaderFormat == NAME_SF_METAL_MRT_MAC)		return SP_METAL_MRT_MAC;
	if (ShaderFormat == NAME_SF_METAL_SM5)			return SP_METAL_SM5;
    if (ShaderFormat == NAME_SF_METAL_SM6)          return SP_METAL_SM6;
    if (ShaderFormat == NAME_SF_METAL_SIM)          return SP_METAL_SIM;
	if (ShaderFormat == NAME_SF_METAL_MACES3_1)		return SP_METAL_MACES3_1;

	return SP_NumPlatforms;
}

// Instance methods
FMetalCompilerToolchain::PackedVersion FMetalCompilerToolchain::GetCompilerVersion(EShaderPlatform Platform) const
{
	if (this->IsMobile(Platform))
	{
		return this->MetalCompilerVersion[AppleSDKMobile];
	}

	return this->MetalCompilerVersion[AppleSDKMac];
}

FMetalCompilerToolchain::PackedVersion FMetalCompilerToolchain::GetTargetVersion(EShaderPlatform Platform) const
{
	if (this->IsMobile(Platform))
	{
		return this->MetalTargetVersion[AppleSDKMobile];
	}

	return this->MetalTargetVersion[AppleSDKMac];
}

const FString& FMetalCompilerToolchain::GetCompilerVersionString(EShaderPlatform Platform) const
{
	if (this->IsMobile(Platform))
	{
		return this->MetalCompilerVersionString[AppleSDKMobile];
	}
	return this->MetalCompilerVersionString[AppleSDKMac];
}

void FMetalCompilerToolchain::Init()
{
	bToolchainAvailable = false;
	bToolchainBinariesPresent = false;
	bSkipPCH = true;

#if PLATFORM_MAC
	EMetalToolchainStatus Result = DoMacNativeSetup();
#else
	EMetalToolchainStatus Result = DoWindowsSetup();
#endif

	if (Result != EMetalToolchainStatus::Success)
	{
#if CHECK_METAL_COMPILER_TOOLCHAIN_SETUP
		UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Metal compiler not found. Shaders will be stored as text."));
#endif
		bToolchainAvailable = false;
	}
	else
	{
		Result = FetchCompilerVersion();

		if (Result != EMetalToolchainStatus::Success)
		{
			UE_LOG(LogMetalCompilerSetup, Log, TEXT("Could not parse compiler version."));
		}

		Result = FetchMetalStandardLibraryPath();

		if (Result != EMetalToolchainStatus::Success)
		{
			UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Could not parse metal_stdlib path. Will not use PCH."));
			bSkipPCH = true;
			// This is not really an error since we can compile without the PCH just fine.
			Result = EMetalToolchainStatus::Success;
		}
		else
		{
			// This is forced off for now. If we wish to re-enable it a lot of testing should be done.
			//bSkipPCH = false;
		}

		bToolchainAvailable = true;
	}

#if CHECK_METAL_COMPILER_TOOLCHAIN_SETUP
	if (Result == EMetalToolchainStatus::Success)
	{
		check(IsCompilerAvailable());
		UE_LOG(LogMetalCompilerSetup, Log, TEXT("Metal toolchain setup complete."));
		UE_LOG(LogMetalCompilerSetup, Log, TEXT("Using Local Metal compiler"));
		if (!MetalFrontendBinaryCommand[AppleSDKMac].IsEmpty())
		{
			UE_LOG(LogMetalCompilerSetup, Log, TEXT("Mac metalfe found at %s"), *MetalFrontendBinaryCommand[AppleSDKMac]);
		}
		if (!MetalFrontendBinaryCommand[AppleSDKMobile].IsEmpty())
		{
			UE_LOG(LogMetalCompilerSetup, Log, TEXT("Mobile metalfe found at %s"), *MetalFrontendBinaryCommand[AppleSDKMobile]);
		}
		UE_LOG(LogMetalCompilerSetup, Log, TEXT("Mac metalfe version %s"), *MetalCompilerVersionString[AppleSDKMac]);
		UE_LOG(LogMetalCompilerSetup, Log, TEXT("Mobile metalfe version %s"), *MetalCompilerVersionString[AppleSDKMobile]);
	}
	else
	{
		 UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Failed to set up Metal toolchain. See log above. Shaders will not be compiled offline."));
	}
#endif
}

void FMetalCompilerToolchain::Teardown()
{
	// remove temporaries
	if (this->LocalTempFolder.IsEmpty())
	{
		return;
	}

	if (!FPaths::DirectoryExists(this->LocalTempFolder))
	{
		return;
	}
	
	bool bSuccess = IFileManager::Get().DeleteDirectory(*this->LocalTempFolder, false, true);
	if (!bSuccess)
	{
		UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Could not delete temporary %s"), *this->LocalTempFolder);
	}
}

FMetalCompilerToolchain::EMetalToolchainStatus FMetalCompilerToolchain::FetchCompilerVersion()
{
	EMetalToolchainStatus Result = EMetalToolchainStatus::Success;
	{
		int32 ReturnCode = 0;
		FString StdOut;
		// metal -v writes its output to stderr
		// But the underlying (windows) implementation of CreateProc puts everything into one pipe, which is written to StdOut.
		bool bResult = this->ExecMetalFrontend(AppleSDKMac, TEXT("-v --target=air64-apple-darwin18.7.0"), &ReturnCode, &StdOut, &StdOut);
		check(bResult);
		if (ReturnCode > 0)
		{
			return EMetalToolchainStatus::CouldNotParseCompilerVersion;
		}
		
		Result = ParseCompilerVersionAndTarget(StdOut, this->MetalCompilerVersionString[AppleSDKMac], this->MetalCompilerVersion[AppleSDKMac], this->MetalTargetVersion[AppleSDKMac]);

		if (Result != EMetalToolchainStatus::Success)
		{
			return Result;
		}
	}

	{
		int32 ReturnCode = 0;
		FString StdOut;
		// metal -v writes its output to stderr
		bool bResult = this->ExecMetalFrontend(AppleSDKMobile, TEXT("-v --target=air64-apple-darwin18.7.0"), &ReturnCode, &StdOut, &StdOut);
		check(bResult);
		if (ReturnCode > 0)
		{
			return EMetalToolchainStatus::CouldNotParseCompilerVersion;
		}
		
		Result = ParseCompilerVersionAndTarget(StdOut, this->MetalCompilerVersionString[AppleSDKMobile], this->MetalCompilerVersion[AppleSDKMobile], this->MetalTargetVersion[AppleSDKMobile]);
	}

	return Result;
}

FMetalCompilerToolchain::EMetalToolchainStatus FMetalCompilerToolchain::FetchMetalStandardLibraryPath()
{
	// if we've already decided to skip compiling a PCH we don't need this path at all.
	if (this->bSkipPCH)
	{
		return EMetalToolchainStatus::Success;
	}

	EMetalToolchainStatus Result = EMetalToolchainStatus::Success;
	{
		int32 ReturnCode = 0;
		FString StdOut, StdErr;
		bool bResult = this->ExecMetalFrontend(AppleSDKMac, TEXT("--print-search-dirs"), &ReturnCode, &StdOut, &StdErr);
		check(bResult);
		Result = ParseLibraryToolpath(StdOut, this->MetalStandardLibraryPath[AppleSDKMac]);

		if (Result != EMetalToolchainStatus::Success)
		{
			return Result;
		}
	}

	{
		int32 ReturnCode = 0;
		FString StdOut, StdErr;
		bool bResult = this->ExecMetalFrontend(AppleSDKMobile, TEXT("--print-search-dirs"), &ReturnCode, &StdOut, &StdErr);
		check(bResult);
		Result = ParseLibraryToolpath(StdOut, this->MetalStandardLibraryPath[AppleSDKMobile]);
	}

	return Result;
}

#if PLATFORM_MAC
FMetalCompilerToolchain::EMetalToolchainStatus FMetalCompilerToolchain::DoMacNativeSetup()
{
	FString ToolchainBase;
	if (FParse::Value(FCommandLine::Get(), TEXT("-MetalToolchainOverride="), ToolchainBase))
	{
		const bool bUseOverride = (!ToolchainBase.IsEmpty() && FPaths::DirectoryExists(ToolchainBase));
		if (bUseOverride)
		{
			MetalFrontendBinaryCommand[AppleSDKMac] = ToolchainBase / TEXT("macos") / TEXT("bin") / MetalFrontendBinary;
			MetalFrontendBinaryCommand[AppleSDKMobile] = ToolchainBase / TEXT("ios") / TEXT("bin") / MetalFrontendBinary;

			const bool bIsFrontendPresent = FPaths::FileExists(MetalFrontendBinaryCommand[AppleSDKMac]) && FPaths::FileExists(MetalFrontendBinaryCommand[AppleSDKMobile]);
			if (!bIsFrontendPresent)
			{
				UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Missing Metal frontend in %s."), *ToolchainBase);
				return EMetalToolchainStatus::ToolchainNotFound;
			}

			MetalArBinaryCommand[AppleSDKMac] = ToolchainBase / TEXT("macos") / TEXT("bin") / MetalArBinary;
			MetalArBinaryCommand[AppleSDKMobile] = ToolchainBase / TEXT("ios") / TEXT("bin") / MetalArBinary;

			MetalLibBinaryCommand[AppleSDKMac] = ToolchainBase / TEXT("macos") / TEXT("bin") / MetalLibraryBinary;
			MetalLibBinaryCommand[AppleSDKMobile] = ToolchainBase / TEXT("ios") / TEXT("bin") / MetalLibraryBinary;

			AirPackBinaryCommand[AppleSDKMac] = ToolchainBase / TEXT("macos") / TEXT("bin") / AirPackBinary;
            AirPackBinaryCommand[AppleSDKMobile] = ToolchainBase / TEXT("ios") / TEXT("bin") / AirPackBinary;

			if (!FPaths::FileExists(MetalArBinaryCommand[AppleSDKMac]) ||
				!FPaths::FileExists(MetalArBinaryCommand[AppleSDKMobile]) ||
				!FPaths::FileExists(MetalLibBinaryCommand[AppleSDKMac]) ||
				!FPaths::FileExists(MetalLibBinaryCommand[AppleSDKMobile]) ||
				!FPaths::FileExists(MetalLibBinaryCommand[AppleSDKMobile]) ||
                !FPaths::FileExists(AirPackBinaryCommand[AppleSDKMac]) ||
                !FPaths::FileExists(AirPackBinaryCommand[AppleSDKMobile]))
			{
				UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Missing toolchain binaries in %s."), *ToolchainBase);
				return EMetalToolchainStatus::ToolchainNotFound;
			}

			this->bToolchainBinariesPresent = true;

			return EMetalToolchainStatus::Success;
		}
	}

	int32 ReturnCode = 0;
	FString StdOut, StdErr;
	bool bSuccess = this->ExecGenericCommand(*XcrunPath, *FString::Printf(TEXT("--sdk %s --find %s"), *this->MetalMacSDK, *this->MetalFrontendBinary), &ReturnCode, &StdOut, &StdErr);
	bSuccess |= FPaths::FileExists(StdOut);
	if(!bSuccess || ReturnCode > 0)
	{
		UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Missing Mac Metal toolchain (macos SDK not found)."));
		return EMetalToolchainStatus::ToolchainNotFound;
	}
	
	bSuccess = this->ExecGenericCommand(*XcrunPath, *FString::Printf(TEXT("--sdk %s --find %s"), *this->MetalMobileSDK, *this->MetalFrontendBinary), &ReturnCode, &StdOut, &StdErr);
	bSuccess |= FPaths::FileExists(StdOut);
	if(!bSuccess || ReturnCode > 0)
	{
		UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Missing Mobile Metal toolchain (iphoneos SDK not found)."));
		return EMetalToolchainStatus::ToolchainNotFound;
	}

	this->bToolchainBinariesPresent = true;

	return EMetalToolchainStatus::Success;
}
#endif

#if PLATFORM_WINDOWS
FMetalCompilerToolchain::EMetalToolchainStatus FMetalCompilerToolchain::DoWindowsSetup()
{
	int32 Result = 0;
	
	FString ToolchainBase;
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("WindowsMetalToolchainOverride"), ToolchainBase, GEngineIni);

	const bool bUseOverride = (!ToolchainBase.IsEmpty() && FPaths::DirectoryExists(ToolchainBase));
	if (!bUseOverride)
	{
		ToolchainBase = DefaultWindowsToolchainPath;
	}

	// Look for the windows native toolchain
	MetalFrontendBinaryCommand[AppleSDKMac] = ToolchainBase / TEXT("macos") / TEXT("bin") / MetalFrontendBinary;
	MetalFrontendBinaryCommand[AppleSDKMobile] = ToolchainBase / TEXT("ios") / TEXT("bin") / MetalFrontendBinary;

	bool bUseLocalMetalToolchain = FPaths::FileExists(MetalFrontendBinaryCommand[AppleSDKMac]) && FPaths::FileExists(MetalFrontendBinaryCommand[AppleSDKMobile]);
	if (!bUseLocalMetalToolchain)
	{
#if CHECK_METAL_COMPILER_TOOLCHAIN_SETUP
		UE_LOG(LogMetalCompilerSetup, Display, TEXT("Searching for Metal toolchain, but it doesn't appear to be installed."));
		UE_LOG(LogMetalCompilerSetup, Display, TEXT("Searched for %s and %s"), *MetalFrontendBinaryCommand[AppleSDKMac], *MetalFrontendBinaryCommand[AppleSDKMobile]);
#endif
		return EMetalToolchainStatus::ToolchainNotFound;
	}

	MetalArBinaryCommand[AppleSDKMac] = ToolchainBase / TEXT("macos") / TEXT("bin") / MetalArBinary;
	MetalArBinaryCommand[AppleSDKMobile] = ToolchainBase / TEXT("ios") / TEXT("bin") / MetalArBinary;

	MetalLibBinaryCommand[AppleSDKMac] = ToolchainBase / TEXT("macos") / TEXT("bin") / MetalLibraryBinary;
	MetalLibBinaryCommand[AppleSDKMobile] = ToolchainBase / TEXT("ios") / TEXT("bin") / MetalLibraryBinary;

	AirPackBinaryCommand[AppleSDKMac] = ToolchainBase / TEXT("macos") / TEXT("bin") / AirPackBinary;
    AirPackBinaryCommand[AppleSDKMobile] = ToolchainBase / TEXT("ios") / TEXT("bin") / AirPackBinary;
    
	if (!FPaths::FileExists(MetalArBinaryCommand[AppleSDKMac]) ||
		!FPaths::FileExists(MetalArBinaryCommand[AppleSDKMobile]) ||
		!FPaths::FileExists(MetalLibBinaryCommand[AppleSDKMac]) ||
		!FPaths::FileExists(MetalLibBinaryCommand[AppleSDKMobile]) ||
        !FPaths::FileExists(AirPackBinaryCommand[AppleSDKMac]) ||
        !FPaths::FileExists(AirPackBinaryCommand[AppleSDKMobile]))
	{
#if CHECK_METAL_COMPILER_TOOLCHAIN_SETUP
		UE_LOG(LogMetalCompilerSetup, Warning, TEXT("Missing toolchain binaries."))
#endif
		return EMetalToolchainStatus::ToolchainNotFound;
	}

	this->bToolchainBinariesPresent = true;
	

	return EMetalToolchainStatus::Success;
}
#endif

bool FMetalCompilerToolchain::ExecMetalFrontend(EAppleSDKType SDK, const TCHAR* Parameters, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr) const
{
	check(this->bToolchainBinariesPresent);
#if PLATFORM_MAC
	if (this->MetalFrontendBinaryCommand[SDK].IsEmpty())
	{
		FString BuiltParams = FString::Printf(TEXT("--sdk %s %s %s"), *SDKToString(SDK), *this->MetalFrontendBinary, Parameters);
		return ExecGenericCommand(*XcrunPath, *BuiltParams, OutReturnCode, OutStdOut, OutStdErr);
	}
	else
#endif
	return ExecGenericCommand(*this->MetalFrontendBinaryCommand[SDK], Parameters, OutReturnCode, OutStdOut, OutStdErr);
}

bool FMetalCompilerToolchain::ExecMetalLib(EAppleSDKType SDK, const TCHAR* Parameters, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr) const
{
	check(this->bToolchainBinariesPresent);
#if PLATFORM_MAC
	if (this->MetalLibBinaryCommand[SDK].IsEmpty())
	{
		FString BuiltParams = FString::Printf(TEXT("--sdk %s %s %s"), *SDKToString(SDK), *this->MetalLibraryBinary, Parameters);
		return ExecGenericCommand(*XcrunPath, *BuiltParams, OutReturnCode, OutStdOut, OutStdErr);
	}
	else
#endif
	return ExecGenericCommand(*this->MetalLibBinaryCommand[SDK], Parameters, OutReturnCode, OutStdOut, OutStdErr);
}

bool FMetalCompilerToolchain::ExecAirPack(EAppleSDKType SDK, const TCHAR* Parameters, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr) const
{
    check(this->bToolchainBinariesPresent);
#if PLATFORM_MAC
    if (this->AirPackBinaryCommand[SDK].IsEmpty())
    {
        FString BuiltParams = FString::Printf(TEXT("--sdk %s %s %s"), *SDKToString(SDK), *this->AirPackBinary, Parameters);
        return ExecGenericCommand(*XcrunPath, *BuiltParams, OutReturnCode, OutStdOut, OutStdErr);
    }
    else
#endif
    return ExecGenericCommand(*this->AirPackBinaryCommand[SDK], Parameters, OutReturnCode, OutStdOut, OutStdErr);
}

bool FMetalCompilerToolchain::ExecMetalAr(EAppleSDKType SDK, const TCHAR* ScriptFile, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr) const
{
	check(this->bToolchainBinariesPresent);
	// WARNING: This phase may be run in parallel so we must not collide our scripts

	// metal-ar is really llvm-ar, which acts like the standard ar. Since we usually end up with a ton of objects we are archiving we'd like to script it
	// Unfortunately ar reads its script from stdin (when the -M arg is present) instead of being provided a file
	// So on windows we'll spawn cmd.exe and pipe the script file into metal-ar.exe -M
#if PLATFORM_MAC
	FString Command;
	if (this->MetalArBinaryCommand[SDK].IsEmpty())
	{
		Command = FString::Printf(TEXT("-c \"%s -sdk %s '%s' -M < '%s'\""), *XcrunPath, *SDKToString(SDK), *this->MetalArBinary, ScriptFile);
	}
	else
	{
		Command = FString::Printf(TEXT("-c \"'%s' -M < '%s'\""), *this->MetalArBinaryCommand[SDK], ScriptFile);
	}
	bool bSuccess = ExecGenericCommand(TEXT("/bin/sh"), *Command, OutReturnCode, OutStdOut, OutStdErr);
#else
	FString Command = FString::Printf(TEXT("/C type \"%s\" | \"%s\" -M"), ScriptFile, *this->MetalArBinaryCommand[SDK]);
	bool bSuccess = ExecGenericCommand(TEXT("cmd.exe"), *Command, OutReturnCode, OutStdOut, OutStdErr);
#endif
	if (!bSuccess)
	{
		UE_LOG(LogMetalShaderCompiler, Error, TEXT("Error creating .metalar. %s."), **OutStdOut);
		UE_LOG(LogMetalShaderCompiler, Error, TEXT("Error creating .metalar. %s."), **OutStdErr);
	}
	return bSuccess;
}

bool FMetalCompilerToolchain::ExecGenericCommand(const TCHAR* Command, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr) const
{
#if PLATFORM_WINDOWS
	{
		// Why do we have our own implementation here? Because metal.exe wants to create a console window. 
		// So if we don't specify the options to CreateProc we end up with tons and tons of windows appearing and disappearing during a cook.
		void* OutputReadPipe = nullptr;
		void* OutputWritePipe = nullptr;
		FPlatformProcess::CreatePipe(OutputReadPipe, OutputWritePipe);
		FProcHandle Proc = FPlatformProcess::CreateProc(Command, Params, false, true, true, nullptr, -1, nullptr, OutputWritePipe, nullptr);

		if (!Proc.IsValid())
		{
			FPlatformProcess::ClosePipe(OutputReadPipe, OutputWritePipe);
			return false;
		}

		int32 RC;
		FPlatformProcess::WaitForProc(Proc);
		FPlatformProcess::GetProcReturnCode(Proc, &RC);
		if (OutStdOut)
		{
			*OutStdOut = FPlatformProcess::ReadPipe(OutputReadPipe);
		}
		FPlatformProcess::ClosePipe(OutputReadPipe, OutputWritePipe);
		FPlatformProcess::CloseProc(Proc);
		if (OutReturnCode)
		{
			*OutReturnCode = RC;
		}

		return RC == 0;
	}
#else
	// Otherwise use the API
	return FPlatformProcess::ExecProcess(Command, Params, OutReturnCode, OutStdOut, OutStdErr);
#endif
}

bool FMetalCompilerToolchain::CompileMetalShader(FMetalShaderBytecodeJob& Job, FMetalShaderBytecode& Output) const
{
	// The local files 
	const FString& LocalInputMetalFilePath = Job.InputFile;
	const FString& LocalOutputMetalAIRFilePath = Job.OutputObjectFile;
	const FString& LocalOutputMetalLibFilePath = Job.OutputFile;

	EAppleSDKType SDK = FMetalCompilerToolchain::MetalFormatToSDK(Job.ShaderFormat);
	
	// .metal -> .air
	FString IncludeArgs = Job.IncludeDir.Len() ? FString::Printf(TEXT("-I %s"), *Job.IncludeDir) : TEXT("");
	{
		// Invoke the metal frontend.
		FString MetalParams = FString::Printf(TEXT("%s %s %s %s -Wno-null-character -fbracket-depth=1024 %s %s %s %s %s -o %s"), *Job.MinOSVersion, *Job.PreserveInvariance, *Job.DebugInfo, *Job.MathMode, TEXT("-c"), *Job.Standard, *Job.Defines, *IncludeArgs, *LocalInputMetalFilePath, *LocalOutputMetalAIRFilePath);
		bool bSuccess = this->ExecMetalFrontend(SDK, *MetalParams, &Job.ReturnCode, &Job.Results, &Job.Errors);

		if (!bSuccess || (Job.ReturnCode != 0))
		{
			Job.Message = FString::Printf(TEXT("Failed to compile %s to bytecode %s, code: %d, output: %s %s"), *LocalInputMetalFilePath, *LocalOutputMetalAIRFilePath, Job.ReturnCode, *Job.Results, *Job.Errors);
			return false;
		}
	}

	{
		// If we have succeeded, now we can create a metallib out of the AIR.
		// TODO do we want to do this in every case? Should be able to skip if we are using Shader Libraries at the high level

		FString MetalLibParams = FString::Printf(TEXT("-o %s %s"), *LocalOutputMetalLibFilePath, *LocalOutputMetalAIRFilePath);
		bool bSuccess = this->ExecMetalLib(SDK, *MetalLibParams, &Job.ReturnCode, &Job.Results, &Job.Errors);
		if (!bSuccess || (Job.ReturnCode != 0))
		{
			Job.Message = FString::Printf(TEXT("Failed to package %s into %s, code: %d, output: %s %s"), *LocalOutputMetalAIRFilePath, *LocalOutputMetalLibFilePath, Job.ReturnCode, *Job.Results, *Job.Errors);
			return false;
		}
	}

	// At this point we have an .air file and a .metallib file
	if (Job.bRetainObjectFile)
	{
		// Retain the .air. This usually means we are using shared native libraries.
		bool bSuccess = FFileHelper::LoadFileToArray(Output.ObjectFile, *LocalOutputMetalAIRFilePath);
		if (!bSuccess)
		{
			Job.Message = FString::Printf(TEXT("Failed to store AIR %s"), *LocalOutputMetalAIRFilePath);
			return false;
		}
	}

	{
		// Retain the .metallib
		Output.NativePath = LocalInputMetalFilePath;
		bool bSuccess = FFileHelper::LoadFileToArray(Output.OutputFile, *LocalOutputMetalLibFilePath);

		if (!bSuccess)
		{
			Job.Message = FString::Printf(TEXT("Failed to store metallib %s"), *LocalOutputMetalAIRFilePath);
			return false;
		}
	}

	return true;
}
