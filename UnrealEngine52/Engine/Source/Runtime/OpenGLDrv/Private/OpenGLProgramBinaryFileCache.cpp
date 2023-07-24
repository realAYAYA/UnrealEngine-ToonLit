// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLProgramBinaryFileCache.cpp: OpenGL program binary file cache stores/loads a set of binary ogl programs.
=============================================================================*/

#include "OpenGLShaders.h"
#include "OpenGLProgramBinaryFileCache.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "OpenGLDrvPrivate.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneUtils.h"
#include "PsoLruCache.h"
#include "OpenGLBinaryProgramUtils.h"
#include <Serialization/StaticMemoryReader.h>
#include "ProfilingDebugging/ScopedTimers.h"
#include <Async/MappedFileHandle.h>

#if PLATFORM_ANDROID
#include "Android/AndroidPlatformMisc.h"
#endif
 
static bool GMemoryMapGLProgramCache = true;
static FAutoConsoleVariableRef CVarMemoryMapGLProgramCache(
	TEXT("r.OpenGL.MemoryMapGLProgramCache"),
	GMemoryMapGLProgramCache,
	TEXT("If true enabled memory mapping of the GL program binary cache. (default)\n")
	TEXT("If false then upon opening the binary cache all programs are loaded into memory.\n")
	TEXT("When enabled this can reduce RSS pressure when combined with program LRU. (see r.OpenGL.EnableProgramLRUCache).")
	,
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> FOpenGLProgramBinaryCache::CVarPBCEnable(
	TEXT("r.ProgramBinaryCache.Enable"),
#if PLATFORM_ANDROID
	1,	// Enabled by default on Android.
#else
	0,
#endif
	TEXT("If true, enables binary program cache. Enabled by default only on Android"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> FOpenGLProgramBinaryCache::CVarRestartAndroidAfterPrecompile(
	TEXT("r.ProgramBinaryCache.RestartAndroidAfterPrecompile"),
	0,
	TEXT("If true, Android apps will restart after precompiling the binary program cache."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static int32 GMaxBinaryProgramLoadTimeMS = 3;
static FAutoConsoleVariableRef CVarMaxBinaryProgramLoadTime(
	TEXT("r.OpenGL.MaxBinaryProgramLoadTime"),
	GMaxBinaryProgramLoadTimeMS,
	TEXT("The maximum time per frame to transfer programs from the binary program cache to the GL RHI. in milliseconds.\n")
	TEXT("default 3ms. Note: Driver compile time for programs may exceed this limit if you're not using the LRU."),
	ECVF_RenderThreadSafe
);

namespace UE
{
	namespace OpenGL
	{
		bool CanMemoryMapGLProgramCache()
		{
			return FPlatformProperties::SupportsMemoryMappedFiles() && GMemoryMapGLProgramCache;
		}

		extern void OnGLProgramLoadedFromBinaryCache(const FOpenGLProgramKey& ProgramKey, TUniqueObj<FOpenGLProgramBinary>&& ProgramBinaryData);

		bool AreBinaryProgramsCompressed()
		{
			static const auto StoreCompressedBinariesCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.OpenGL.StoreCompressedProgramBinaries"));
			return StoreCompressedBinariesCVar->GetInt() != 0;
		}

		static const uint32 GBinaryProgramFileVersion = 5;
		struct FBinaryCacheFileHeader
		{
			uint32 Version = 0;
			FGuid BinaryCacheGuid;
			bool bCacheUsesCompressedBinaries;
			uint32 ProgramCount;

			static FBinaryCacheFileHeader CreateHeader(const FGuid& BinaryCacheGuidIn, uint32 NumPrograms)
			{
				FBinaryCacheFileHeader NewHeader;
				NewHeader.Version = GBinaryProgramFileVersion;
				NewHeader.BinaryCacheGuid = BinaryCacheGuidIn;
				NewHeader.bCacheUsesCompressedBinaries = UE::OpenGL::AreBinaryProgramsCompressed();
				NewHeader.ProgramCount = NumPrograms;
				return NewHeader;
			}

			friend FArchive& operator<<(FArchive& Ar, FBinaryCacheFileHeader& Header)
			{
				Ar << Header.Version;
				check(Ar.IsLoading() || Header.Version == GBinaryProgramFileVersion); // This should always be correct when saving.
				if (Header.Version == GBinaryProgramFileVersion)
				{
					Ar << Header.BinaryCacheGuid;
					Ar << Header.bCacheUsesCompressedBinaries;
					Ar << Header.ProgramCount;
				}

				return Ar;
			}

			bool IsValid() const
			{
				return Version == GBinaryProgramFileVersion;
			}
		};
	}
}

// This contains the mapping for a binary program cache file.
// It also contains a list of programs that the cache contains.
class FOpenGLProgramBinaryMapping : public FThreadSafeRefCountedObject
{
public:
	FOpenGLProgramBinaryMapping(TUniquePtr<IMappedFileHandle> MappedCacheFileIn, TUniquePtr<IMappedFileRegion> MappedRegionIn, uint32 ProgramCountIfKnown) : MappedCacheFile(MoveTemp(MappedCacheFileIn)), MappedRegion(MoveTemp(MappedRegionIn)) { Content.Reserve(ProgramCountIfKnown); }

	TArrayView<const uint8> GetView(uint32 FileOffset, uint32 NumBytes) const
	{
		check(FileOffset + NumBytes <= MappedRegion->GetMappedSize());
		return TArrayView<const uint8>(MappedRegion->GetMappedPtr() + FileOffset, NumBytes);
	}

	void AddProgramKey(const class FOpenGLProgramKey& KeyIn) { check(!Content.Contains(KeyIn)); Content.Add(KeyIn); }
	bool HasValidMapping() const { return MappedRegion.IsValid() && MappedCacheFile.IsValid(); }
private:
	TUniquePtr<IMappedFileHandle> MappedCacheFile;
	TUniquePtr<IMappedFileRegion> MappedRegion;
	TSet<FOpenGLProgramKey> Content;
};


static FCriticalSection GProgramBinaryFileCacheCS;

// guards the container that collects scanned programs and send to RHIT
static FCriticalSection GPendingGLProgramCreateRequestsCS;

FOpenGLProgramBinaryCache* FOpenGLProgramBinaryCache::CachePtr = nullptr;

FOpenGLProgramBinaryCache::FOpenGLProgramBinaryCache(const FString& InCachePath)
	: CachePath(InCachePath)
	, BinaryCacheWriteFileHandle(nullptr)
	, BinaryFileState(EBinaryFileState::Uninitialized)
{
	ANSICHAR* GLVersion = (ANSICHAR*)glGetString(GL_VERSION);
	ANSICHAR* GLRenderer = (ANSICHAR*)glGetString(GL_RENDERER);
	FString HashString;
	HashString.Append(GLVersion);
	HashString.Append(GLRenderer);

#if PLATFORM_ANDROID
	// FORT-512259:
	// Apparently we can't rely on GL_VERSION alone to assume binary compatibility.
	// Some devices report binary compatibility errors after minor OS updates even though the GL driver version has not changed.
	const FString BuildNumber = FAndroidMisc::GetDeviceBuildNumber();
	HashString.Append(BuildNumber);

	// Optional configrule variable for triggering a rebuild of the cache.
	const FString* ConfigRulesGLProgramKey = FAndroidMisc::GetConfigRulesVariable(TEXT("OpenGLProgramCacheKey"));
	if (ConfigRulesGLProgramKey && !ConfigRulesGLProgramKey->IsEmpty())
	{
		HashString.Append(*ConfigRulesGLProgramKey);
	}
#endif

	FSHAHash VersionHash;
	FSHA1::HashBuffer(TCHAR_TO_ANSI(*HashString), HashString.Len(), VersionHash.Hash);

	CacheFilename = LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform).ToString() + TEXT("_") + VersionHash.ToString();
}

FOpenGLProgramBinaryCache::~FOpenGLProgramBinaryCache()
{
	if (BinaryCacheWriteFileHandle)
	{
		delete BinaryCacheWriteFileHandle;
	}

	if (OnShaderPipelineCacheOpenedDelegate.IsValid())
	{
		FShaderPipelineCache::GetCacheOpenedDelegate().Remove(OnShaderPipelineCacheOpenedDelegate);
	}

	if (OnShaderPipelineCachePrecompilationCompleteDelegate.IsValid())
	{
		FShaderPipelineCache::GetPrecompilationCompleteDelegate().Remove(OnShaderPipelineCachePrecompilationCompleteDelegate);
	}
};

bool FOpenGLProgramBinaryCache::IsEnabled()
{
	return CachePtr != nullptr;
}

bool FOpenGLProgramBinaryCache::IsBuildingCache()
{
	if (CachePtr != nullptr)
	{
		return CachePtr->IsBuildingCache_internal();
	}
	return false;
}

void FOpenGLProgramBinaryCache::Initialize()
{
	check(CachePtr == nullptr);

	if (CVarPBCEnable.GetValueOnAnyThread() == 0)
	{
		UE_LOG(LogRHI, Log, TEXT("FOpenGLProgramBinaryCache disabled by r.ProgramBinaryCache.Enable=0"));
		return;
	}

	if (!FOpenGL::SupportsProgramBinary())
	{
		UE_LOG(LogRHI, Warning, TEXT("FOpenGLProgramBinaryCache disabled as devices does not support program binaries"));
		return;
	}

#if PLATFORM_ANDROID
	if (FOpenGL::HasBinaryProgramRetrievalFailed())
	{
		if (FOpenGL::SupportsProgramBinary())
		{
			UE_LOG(LogRHI, Warning, TEXT("FOpenGLProgramBinaryCache: Device has failed to emit program binary despite SupportsProgramBinary == true. Disabling binary cache."));
			return;
		}
	}
#endif


	FString CacheFolderPath;
#if PLATFORM_ANDROID && USE_ANDROID_FILE
	// @todo Lumin: Use that GetPathForExternalWrite or something?
	extern FString GExternalFilePath;
	CacheFolderPath = GExternalFilePath / TEXT("ProgramBinaryCache");

#else
	CacheFolderPath = FPaths::ProjectSavedDir() / TEXT("ProgramBinaryCache");
#endif

	// Remove entire ProgramBinaryCache folder if -ClearOpenGLBinaryProgramCache is specified on command line
	if (FParse::Param(FCommandLine::Get(), TEXT("ClearOpenGLBinaryProgramCache")))
	{
		UE_LOG(LogRHI, Log, TEXT("ClearOpenGLBinaryProgramCache specified, deleting binary program cache folder: %s"), *CacheFolderPath);
		FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*CacheFolderPath);
	}

	CachePtr = new FOpenGLProgramBinaryCache(CacheFolderPath);
	UE_LOG(LogRHI, Log, TEXT("Enabling program binary cache as %s"), *CachePtr->GetProgramBinaryCacheFilePath());

	// Add delegates for the ShaderPipelineCache precompile.
	UE_LOG(LogRHI, Log, TEXT("FOpenGLProgramBinaryCache will be initialized when ShaderPipelineCache opens its file"));
	CachePtr->OnShaderPipelineCacheOpenedDelegate = FShaderPipelineCache::GetCacheOpenedDelegate().AddRaw(CachePtr, &FOpenGLProgramBinaryCache::OnShaderPipelineCacheOpened);
	CachePtr->OnShaderPipelineCachePrecompilationCompleteDelegate = FShaderPipelineCache::GetPrecompilationCompleteDelegate().AddRaw(CachePtr, &FOpenGLProgramBinaryCache::OnShaderPipelineCachePrecompilationComplete);
}

#if PLATFORM_ANDROID
static int32 GNumRemoteProgramCompileServices = 4;
static FAutoConsoleVariableRef CVarNumRemoteProgramCompileServices(
	TEXT("Android.OpenGL.NumRemoteProgramCompileServices"),
	GNumRemoteProgramCompileServices,
	TEXT("The number of separate processes to make available to compile opengl programs.\n")
	TEXT("0 to disable use of separate processes to precompile PSOs\n")
	TEXT("valid range is 1-8 (4 default).")
	,
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);
#endif

void FOpenGLProgramBinaryCache::OnShaderPipelineCacheOpened(FString const& Name, EShaderPlatform Platform, uint32 Count, const FGuid& VersionGuid, FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext)
{
	UE_LOG(LogRHI, Log, TEXT("Scanning Binary program cache, using Shader Pipeline Cache version %s"), *VersionGuid.ToString());

	FScopeLock Lock(&GProgramBinaryFileCacheCS);
	check(CurrentShaderPipelineCacheVersionGuid == FGuid());
	CurrentShaderPipelineCacheVersionGuid = VersionGuid;
	ScanProgramCacheFile();
	if (IsBuildingCache_internal())
	{
#if PLATFORM_ANDROID
		if (GNumRemoteProgramCompileServices)
		{
			FAndroidOpenGL::StartAndWaitForRemoteCompileServices(GNumRemoteProgramCompileServices);
		}
#endif
		ShaderCachePrecompileContext.SetPrecompilationIsSlowTask();
	}
}

void FOpenGLProgramBinaryCache::Reset()
{
	check(!BinaryCacheWriteFileHandle);
	BinaryFileState = EBinaryFileState::Uninitialized;
 	ProgramsWrittenToOutputCache.Empty();
}

void FOpenGLProgramBinaryCache::OnShaderPipelineCachePrecompilationComplete(uint32 Count, double Seconds, const FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext)
{
	FScopeLock Lock(&GProgramBinaryFileCacheCS);

#if PLATFORM_ANDROID
	FAndroidOpenGL::StopRemoteCompileServices();
#endif

	UE_LOG(LogRHI, Log, TEXT("OnShaderPipelineCachePrecompilationComplete: %d shaders"), Count);

	const bool bFinishedBuildingCache = IsBuildingCache_internal();
	check(bFinishedBuildingCache || BinaryFileState == EBinaryFileState::ValidCacheFile);

	if (bFinishedBuildingCache)
	{
		CloseWriteHandle();

		if(!ProgramsWrittenToOutputCache.IsEmpty())
		{
#if PLATFORM_ANDROID
			if (CVarRestartAndroidAfterPrecompile.GetValueOnAnyThread() == 1)
			{
				FAndroidMisc::bNeedsRestartAfterPSOPrecompile = true;
#if USE_ANDROID_JNI
				extern void AndroidThunkCpp_RestartApplication(const FString & IntentString);
				AndroidThunkCpp_RestartApplication(TEXT(""));
#endif
			}
#endif
		}
		// Reset state and scan the file we just produced.
		Reset();
		ScanProgramCacheFile();

		// unset current cache.
		CurrentShaderPipelineCacheVersionGuid = FGuid();
	}
}

// Scan the binary cache file and build a record of all programs.
void FOpenGLProgramBinaryCache::ScanProgramCacheFile()
{
	//FScopedDurationTimeLogger Timer(TEXT("ScanProgramCacheFile"));

	UE_LOG(LogRHI, Log, TEXT("OnShaderScanProgramCacheFile"));
	FString ProgramCacheFilename = GetProgramBinaryCacheFilePath();
	FString ProgramCacheFilenameTemp = ProgramCacheFilename + TEXT(".scan");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	check(BinaryFileState == EBinaryFileState::Uninitialized);

	bool bBinaryFileIsValid = false;
	bool bBinaryFileIsValidAndGuidMatch = false;

	// Try to move the file to a temporary filename before the scan, so we won't try to read it again if it's corrupted
	PlatformFile.DeleteFile(*ProgramCacheFilenameTemp);
	PlatformFile.MoveFile(*ProgramCacheFilenameTemp, *ProgramCacheFilename);

	TUniquePtr<FArchive> BinaryProgramReader = nullptr;
	BinaryProgramReader = TUniquePtr<FArchive>( IFileManager::Get().CreateFileReader(*ProgramCacheFilenameTemp) );

	if (BinaryProgramReader)
	{
		FArchive& Ar = *BinaryProgramReader;
		UE::OpenGL::FBinaryCacheFileHeader BinaryCacheHeader;
		Ar << BinaryCacheHeader;
		if (BinaryCacheHeader.IsValid())
		{
			const bool bUseCompressedProgramBinaries = UE::OpenGL::AreBinaryProgramsCompressed();
			bBinaryFileIsValid = (bUseCompressedProgramBinaries == BinaryCacheHeader.bCacheUsesCompressedBinaries);
			bBinaryFileIsValidAndGuidMatch = bBinaryFileIsValid && (!CurrentShaderPipelineCacheVersionGuid.IsValid() || CurrentShaderPipelineCacheVersionGuid == BinaryCacheHeader.BinaryCacheGuid);

			if (bBinaryFileIsValidAndGuidMatch)
			{
				int32 ProgramIndex = 0;
				FScopeLock Lock(&GPendingGLProgramCreateRequestsCS);
				ScannedBinaryPrograms.Reserve(BinaryCacheHeader.ProgramCount);
				if (BinaryCacheHeader.ProgramCount > 0)
				{
					TRefCountPtr<FOpenGLProgramBinaryMapping> CurrentMapping;
					// success
					{
						TUniquePtr<IMappedFileHandle> MappedCacheFile;
						TUniquePtr<IMappedFileRegion> MappedRegion;
						if(UE::OpenGL::CanMemoryMapGLProgramCache())
						{
							MappedCacheFile = TUniquePtr<IMappedFileHandle>(FPlatformFileManager::Get().GetPlatformFile().OpenMapped(*ProgramCacheFilenameTemp));
							if (ensure(MappedCacheFile.IsValid()))
							{
								MappedRegion = TUniquePtr<IMappedFileRegion>(MappedCacheFile->MapRegion());
								check(MappedRegion.IsValid());
							}
						}
						CurrentMapping = new FOpenGLProgramBinaryMapping(MoveTemp(MappedCacheFile), MoveTemp(MappedRegion), BinaryCacheHeader.ProgramCount);
					}

					UE_LOG(LogRHI, Log, TEXT("OnShaderScanProgramCacheFile : %s %s"), CurrentMapping->HasValidMapping() ? TEXT("mapped") : TEXT("opened"), *ProgramCacheFilenameTemp);

					MappedCacheFiles.Add(CurrentShaderPipelineCacheVersionGuid, CurrentMapping);
					while (!Ar.AtEnd())
					{
						FOpenGLProgramKey ProgramKey;
						uint32 ProgramBinarySize = 0;
						Ar << ProgramKey;
						Ar << ProgramBinarySize;
						check(ProgramKey != FOpenGLProgramKey());
						if (ensure(ProgramBinarySize > 0))
						{
							ProgramIndex++;
							uint32 ProgramBinaryOffset = Ar.Tell();
							CurrentMapping->AddProgramKey(ProgramKey);

							UE_LOG(LogRHI, VeryVerbose, TEXT(" scan found PSO %s - %d"), *ProgramKey.ToString(), ProgramBinarySize);

							if(CurrentMapping->HasValidMapping())
							{
								ScannedBinaryPrograms.Emplace(ProgramKey, TUniqueObj<FOpenGLProgramBinary>(CurrentMapping->GetView(ProgramBinaryOffset, ProgramBinarySize)));
								Ar.Seek(ProgramBinaryOffset + ProgramBinarySize);
							}
							else
							{
								check(!UE::OpenGL::CanMemoryMapGLProgramCache());
								TArray<uint8> ProgramBytes;
								ProgramBytes.SetNumUninitialized(ProgramBinarySize);
								Ar.Serialize(ProgramBytes.GetData(), ProgramBinarySize);
								ScannedBinaryPrograms.Emplace(ProgramKey, TUniqueObj<FOpenGLProgramBinary>(MoveTemp(ProgramBytes)));
							}
						}
					}

					UE_LOG(LogRHI, VeryVerbose, TEXT("Program Binary cache: Found %d cached programs"), ProgramIndex);
					UE_CLOG(ProgramIndex != BinaryCacheHeader.ProgramCount, LogRHI, Error, TEXT("Program Binary cache: Mismatched program count! expected: %d"), BinaryCacheHeader.ProgramCount);

					BinaryProgramReader->Close();
					// Rename the file back after a successful scan.
					PlatformFile.MoveFile(*ProgramCacheFilename, *ProgramCacheFilenameTemp);
					BinaryFileState = EBinaryFileState::ValidCacheFile;
				}
				else
				{
					// failed to find sentinel record, the file was not finalized.
					UE_LOG(LogRHI, Warning, TEXT("ScanProgramCacheFile - incomplete or empty binary cache file encountered. Rebuilding binary program cache."));
					BinaryProgramReader->Close();
					bBinaryFileIsValid = false;
					bBinaryFileIsValidAndGuidMatch = false;
					PlatformFile.DeleteFile(*ProgramCacheFilenameTemp);
				}
			}
			else
			{
				UE_LOG(LogRHI, Log, TEXT("OnShaderScanProgramCacheFile : binary file found but is invalid (%s, %s)"), *CurrentShaderPipelineCacheVersionGuid.ToString(), *BinaryCacheHeader.BinaryCacheGuid.ToString());
			}
		}
		else
		{
			UE_LOG(LogRHI, Log, TEXT("OnShaderScanProgramCacheFile : binary file version invalid"));
		}
	}
	else
	{
		UE_LOG(LogRHI, Log, TEXT("OnShaderScanProgramCacheFile : Failed to open %s"), *ProgramCacheFilename);
	}

	if (!bBinaryFileIsValidAndGuidMatch)
	{
		if (OpenWriteHandle())
		{
			BinaryFileState = EBinaryFileState::BuildingCacheFile;
			// save header, 0 program count indicates an unfinished file.
			// The header is overwritten at the end of the process.
			UE::OpenGL::FBinaryCacheFileHeader OutHeader = UE::OpenGL::FBinaryCacheFileHeader::CreateHeader(CurrentShaderPipelineCacheVersionGuid, 0);
			FArchive& Ar = *BinaryCacheWriteFileHandle;
			Ar << OutHeader;
		}
		else
		{
			// Binary cache file cannot be used, failed to open output file.
			BinaryFileState = EBinaryFileState::Uninitialized;
			RHIGetPanicDelegate().ExecuteIfBound(FName("FailedBinaryProgramArchiveOpen"));
			UE_LOG(LogRHI, Fatal, TEXT("ScanProgramCacheFile - Failed to open binary cache."));
		}
	}
}

bool FOpenGLProgramBinaryCache::OpenWriteHandle()
{
	check(BinaryCacheWriteFileHandle == nullptr);

	// perform file writing to a file temporary filename so we don't attempt to use the file later if the write session is interrupted
	const FString ProgramCacheFilenameWrite = GetProgramBinaryCacheFilePath() + TEXT(".write");

	BinaryCacheWriteFileHandle = IFileManager::Get().CreateFileWriter(*ProgramCacheFilenameWrite, EFileWrite::FILEWRITE_None);

	UE_CLOG(BinaryCacheWriteFileHandle == nullptr, LogRHI, Warning, TEXT("Failed to open OGL binary cache output file."));

	return BinaryCacheWriteFileHandle != nullptr;
}

void FOpenGLProgramBinaryCache::CloseWriteHandle()
{
	check(BinaryCacheWriteFileHandle != nullptr);

	bool bArchiveFailed = BinaryCacheWriteFileHandle->IsError() || BinaryCacheWriteFileHandle->IsCriticalError();

	// Overwrite the header with the final program count. This indicates a successful write.
	if(!bArchiveFailed)
	{
		FArchive& Ar = *BinaryCacheWriteFileHandle;
		Ar.Seek(0);
		UE::OpenGL::FBinaryCacheFileHeader OutHeader = UE::OpenGL::FBinaryCacheFileHeader::CreateHeader(CurrentShaderPipelineCacheVersionGuid, ProgramsWrittenToOutputCache.Num());
		Ar << OutHeader;
		bArchiveFailed = BinaryCacheWriteFileHandle->IsError() || BinaryCacheWriteFileHandle->IsCriticalError();
	}

	BinaryCacheWriteFileHandle->Close();
	delete BinaryCacheWriteFileHandle;
	BinaryCacheWriteFileHandle = nullptr;

	const FString ProgramCacheFilename = GetProgramBinaryCacheFilePath();
	const FString ProgramCacheFilenameWrite = ProgramCacheFilename + TEXT(".write");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (bArchiveFailed)
	{
		RHIGetPanicDelegate().ExecuteIfBound(FName("FailedBinaryProgramArchiveWrite"));
		UE_LOG(LogRHI, Fatal, TEXT("CloseWriteHandle - FArchive error bit set, failed to write binary cache."));
	}

	// rename the temp filename back to the final filename
	PlatformFile.DeleteFile(*ProgramCacheFilename); // file should never exist, but for safety
	PlatformFile.MoveFile(*ProgramCacheFilename, *ProgramCacheFilenameWrite);
}

// Called when a new program has been created by OGL RHI, creates the binary cache if it's invalid and then appends the new program details to the file and runtime containers.
FOpenGLProgramBinary FOpenGLProgramBinaryCache::CreateAndCacheProgramBinary_Internal(const FOpenGLProgramKey& ProgramKey, GLuint Program)
{
	FOpenGLProgramBinary CachedProgramBinary;
	// Add to runtime and disk.
	if (!ProgramsWrittenToOutputCache.Contains(ProgramKey))
	{
		uint32 ProgramBinaryOffset = 0, ProgramBinarySize = 0;

		FOpenGLProgramKey SerializedProgramKey = ProgramKey;
		CachedProgramBinary = UE::OpenGL::GetProgramBinaryFromGLProgram(Program);
		if (ensure(CachedProgramBinary.IsValid()))
		{
			AddProgramBinaryDataToBinaryCache(ProgramKey, CachedProgramBinary);
		}
		else
		{
			// we've encountered a problem with this program and there's nothing to write.
			// This likely means the device will never be able to use this program.
			// Panic!
			RHIGetPanicDelegate().ExecuteIfBound(FName("FailedBinaryProgramWrite"));
			UE_LOG(LogRHI, Fatal, TEXT("AppendProgramBinaryFile Binary program returned 0 bytes!"));
			// Panic!
		}
	}
	return CachedProgramBinary;
}

void FOpenGLProgramBinaryCache::CacheProgramBinary(const FOpenGLProgramKey& ProgramKey, const FOpenGLProgramBinary& BinaryProgramData)
{
	if (CachePtr)
	{
		FScopeLock Lock(&GProgramBinaryFileCacheCS);

		if (!CachePtr->ProgramsWrittenToOutputCache.Contains(ProgramKey))
		{
			CachePtr->AddProgramBinaryDataToBinaryCache(ProgramKey, BinaryProgramData);
		}
	}
}

// Serialize out the program binary data and add to runtime structures.
void FOpenGLProgramBinaryCache::AddProgramBinaryDataToBinaryCache(const FOpenGLProgramKey& ProgramKey, const FOpenGLProgramBinary& BinaryProgramData)
{
	check(IsBuildingCache_internal());

	FArchive& Ar = *BinaryCacheWriteFileHandle;
	// Serialize to output file:
	FOpenGLProgramKey SerializedProgramKey = ProgramKey;
	const TArrayView<const uint8> BinaryProgramDataView = BinaryProgramData.GetDataView();
	uint32 ProgramBinarySize = (uint32)BinaryProgramDataView.Num();
	const uint8* ProgramBinaryBytes = BinaryProgramDataView.GetData();
	Ar << SerializedProgramKey;
	uint32 ProgramBinaryOffset = Ar.Tell();
	Ar << ProgramBinarySize;
	Ar.Serialize(const_cast<uint8*>(ProgramBinaryBytes), ProgramBinarySize);
	if (UE::OpenGL::AreBinaryProgramsCompressed())
	{
		static uint32 TotalUncompressed = 0;
		static uint32 TotalCompressed = 0;

		const UE::OpenGL::FCompressedProgramBinaryHeader* Header = (UE::OpenGL::FCompressedProgramBinaryHeader*)ProgramBinaryBytes;
		TotalUncompressed += Header->UncompressedSize;
		TotalCompressed += ProgramBinarySize;

		UE_LOG(LogRHI, Verbose, TEXT("AppendProgramBinaryFile: total Uncompressed: %d, total Compressed %d, Total saved so far: %d"), TotalUncompressed, TotalCompressed, TotalUncompressed - TotalCompressed);
	}
	ProgramsWrittenToOutputCache.Add(ProgramKey);
}

void FOpenGLProgramBinaryCache::Shutdown()
{
	if (CachePtr)
	{
		delete CachePtr;
		CachePtr = nullptr;
	}
}

bool FOpenGLProgramBinaryCache::DoesOutputCacheContain(const FOpenGLProgramKey& ProgramKey)
{
	if (CachePtr)
	{
		FScopeLock Lock(&GProgramBinaryFileCacheCS);
		return CachePtr->DoesOutputCacheContain_Internal(ProgramKey);
	}
	return false;
}

bool FOpenGLProgramBinaryCache::DoesOutputCacheContain_Internal(const FOpenGLProgramKey& ProgramKey) const
{
	return ProgramsWrittenToOutputCache.Contains(ProgramKey);
}

FOpenGLProgramBinary FOpenGLProgramBinaryCache::CreateAndCacheProgramBinary(const FOpenGLProgramKey& ProgramKey, GLuint Program )
{
	FOpenGLProgramBinary ProgramBinary;
	if (CachePtr)
	{
		FScopeLock Lock(&GProgramBinaryFileCacheCS);
		ProgramBinary = CachePtr->CreateAndCacheProgramBinary_Internal(ProgramKey, Program);
	}
	return ProgramBinary;
}

FString FOpenGLProgramBinaryCache::GetProgramBinaryCacheFilePath() const
{
	FString ProgramFilename = CachePath + TEXT("/") + CacheFilename + TEXT("_")+CurrentShaderPipelineCacheVersionGuid.ToString();
	return ProgramFilename;
}

void FOpenGLProgramBinaryCache::CheckPendingGLProgramCreateRequests()
{
	if (CachePtr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderCreateShaderLibRequests);
		check(IsInRenderingThread() || IsInRHIThread());
		FScopeLock Lock(&GPendingGLProgramCreateRequestsCS);
		CachePtr->CheckPendingGLProgramCreateRequests_internal();
	}
}

// Move programs encountered during the scan to the GL RHI program container.
// GMaxBinaryProgramLoadTimeMS attempts to reduce hitching, if we're not using the LRU then we still create GL programs and require more time.
void FOpenGLProgramBinaryCache::CheckPendingGLProgramCreateRequests_internal()
{
	if (ScannedBinaryPrograms.Num() > 0)
	{
		//FScopedDurationTimeLogger Timer(TEXT("CheckPendingGLProgramCreateRequests"));
		float TimeRemainingS = (float)GMaxBinaryProgramLoadTimeMS / 1000.0f;
		double StartTime = FPlatformTime::Seconds();
		int32 Count = 0;

		for (auto It = ScannedBinaryPrograms.CreateIterator(); It && TimeRemainingS > 0.0f; ++It)
		{
			UE::OpenGL::OnGLProgramLoadedFromBinaryCache(It->Key, MoveTemp(It->Value));
			TimeRemainingS -= (float)(FPlatformTime::Seconds() - StartTime);
			StartTime = FPlatformTime::Seconds();
			Count++;
			It.RemoveCurrent();
		}
		float TimeTaken = (float)GMaxBinaryProgramLoadTimeMS - (TimeRemainingS * 1000.0f);
		if (TimeRemainingS < 0.0f)
		{
			UE_LOG(LogRHI, Warning, TEXT("CheckPendingGLProgramCreateRequests : iter count = %d, time taken = %f ms (remaining %d)"), Count, TimeTaken, ScannedBinaryPrograms.Num());
		}
		else
		{
			UE_LOG(LogRHI, Verbose, TEXT("CheckPendingGLProgramCreateRequests : iter count = %d, time taken = %f ms (remaining %d)"), Count, TimeTaken, ScannedBinaryPrograms.Num());
		}
	}
}

bool FOpenGLProgramBinaryCache::CheckSinglePendingGLProgramCreateRequest(const FOpenGLProgramKey& ProgramKey)
{
	if (CachePtr)
	{
		check(IsInRenderingThread() || IsInRHIThread());
		FScopeLock Lock(&GPendingGLProgramCreateRequestsCS);
		return CachePtr->CheckSinglePendingGLProgramCreateRequest_internal(ProgramKey);
	}
	return false;
}

// Any pending program must complete in this case.
bool FOpenGLProgramBinaryCache::CheckSinglePendingGLProgramCreateRequest_internal(const FOpenGLProgramKey& ProgramKey)
{
	TUniqueObj<FOpenGLProgramBinary> ProgFound;
	if (ScannedBinaryPrograms.RemoveAndCopyValue(ProgramKey, ProgFound))
	{		
		UE::OpenGL::OnGLProgramLoadedFromBinaryCache(ProgramKey, MoveTemp(ProgFound));
		return true;
	}
	return false;
}
