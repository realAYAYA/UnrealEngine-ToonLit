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

static int32 GBinaryCachePeriodicFlushProgramCount = 20;
static FAutoConsoleVariableRef CVarBinaryCachePeriodicFlushProgramCount(
	TEXT("r.OpenGL.BinaryCachePeriodicFlushProgramCount"),
	GBinaryCachePeriodicFlushProgramCount,
	TEXT("When r.PSOPrecaching is active this value\n")
	TEXT("is the number of appended programs to accumulate before the cache is flushed to storage."),
	ECVF_RenderThreadSafe
);

static int32 GBinaryCacheMMapAfterEveryMB = 50;
static FAutoConsoleVariableRef CVarBinaryCacheMMapAfterEveryMB(
	TEXT("r.OpenGL.BinaryCacheMMapAfterEveryMB"),
	GBinaryCacheMMapAfterEveryMB,
	TEXT("When r.PSOPrecaching is active this value\n")
	TEXT("specifies the size program binary cache can grow before it is memory mapped, the mmapped programs replace the allocated programs and potentially frees up memory for unused precached programs."),
	ECVF_RenderThreadSafe
);

static int32 GBinaryCacheMaxPermittedSizeMB = 350;
static FAutoConsoleVariableRef CVarBinaryCacheMaxPermittedSizeMB(
	TEXT("r.OpenGL.BinaryCacheMaxPermittedSize"),
	GBinaryCacheMaxPermittedSizeMB,
	TEXT("When r.PSOPrecaching is active and the binary cache's size is greater\n")
	TEXT("than this value the cache will be deleted at startup. The precaching cache is rebuilt from empty."),
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

		static const uint32 GBinaryProgramFileVersion = 6;
		struct FBinaryCacheFileHeader
		{
			uint32 Version = 0xFFFFFFFF;
			FGuid BinaryCacheGuid;
			bool bCacheUsesCompressedBinaries;
			uint32 ProgramCount = 0;
			uint32 ValidSize = 0;

			static FBinaryCacheFileHeader CreateHeader(const FGuid& BinaryCacheGuidIn, uint32 NumPrograms, uint32 ValidSize)
			{
				FBinaryCacheFileHeader NewHeader;
				NewHeader.Version = GBinaryProgramFileVersion;
				NewHeader.BinaryCacheGuid = BinaryCacheGuidIn;
				NewHeader.bCacheUsesCompressedBinaries = UE::OpenGL::AreBinaryProgramsCompressed();
				NewHeader.ProgramCount = NumPrograms;
				NewHeader.ValidSize = ValidSize;
				return NewHeader;
			}

			friend FArchive& operator<<(FArchive& Ar, FBinaryCacheFileHeader& Header)
			{
				Ar << Header.Version;
				check(Ar.IsLoading() || Header.IsValidVersion()); // This should always be correct when saving.
				if (Header.IsValidVersion())
				{
					Ar << Header.BinaryCacheGuid;
					Ar << Header.bCacheUsesCompressedBinaries;
					Ar << Header.ProgramCount;
					Ar << Header.ValidSize;
				}

				return Ar;
			}

			bool IsValidVersion() const
			{
				return Version == GBinaryProgramFileVersion;
			}

			bool IsValid(const FGuid* OptionalWantedGuid) const
			{
				return IsValidVersion()
					&& (UE::OpenGL::AreBinaryProgramsCompressed() == bCacheUsesCompressedBinaries)
					&& (OptionalWantedGuid == nullptr || (*OptionalWantedGuid == BinaryCacheGuid))
					&& (ProgramCount > 0);
			}

			FString ToString() const
			{
				return FString::Format(TEXT("{0}, {1}, {2}, {3}"), { Version, bCacheUsesCompressedBinaries, *BinaryCacheGuid.ToString(), ProgramCount });
			}
		};
	}
}

// This contains the mapping for a binary program cache file.
// It also contains a list of programs that the cache contains.
class FOpenGLProgramBinaryMapping : public FThreadSafeRefCountedObject
{
public:
	FOpenGLProgramBinaryMapping(TUniquePtr<IMappedFileHandle> MappedCacheFileIn, uint32 ProgramCountIfKnown) : MappedCacheFile(MoveTemp(MappedCacheFileIn)) { Content.Reserve(ProgramCountIfKnown); }

	TArrayView<const uint8> GetView(uint32 FileOffset, uint32 NumBytes) const
	{
		check(FileOffset >= CurrentMappingRegionOffset);
		uint32 OffsetWithinMapping = FileOffset - CurrentMappingRegionOffset;
		check(OffsetWithinMapping + NumBytes <= GetCurrentMappedRegion()->GetMappedSize());
		return TArrayView<const uint8>(GetCurrentMappedRegion()->GetMappedPtr() + OffsetWithinMapping, NumBytes);
	}

	void AddMapping(uint32 MappingRegionOffset, TUniquePtr<IMappedFileRegion> NewMappedRegionIn)
	{
		check(MappingRegionOffset != 0 || MappedRegions.IsEmpty());
		CurrentMappingRegionOffset = MappingRegionOffset;
		MappedRegions.Add(MoveTemp(NewMappedRegionIn));
	};

	void AddProgramKey(const class FOpenGLProgramKey& KeyIn) { check(!Content.Contains(KeyIn)); Content.Add(KeyIn); }
	bool HasValidMapping() const { return MappedCacheFile.IsValid() && !MappedRegions.IsEmpty() && MappedRegions.Last().IsValid(); }
	int32 NumPrograms() const { return Content.Num(); }

	TUniquePtr<IMappedFileHandle>& GetMappedCacheFile() { return MappedCacheFile; }

	TUniquePtr<IMappedFileRegion>& GetCurrentMappedRegion()
	{
		check(!MappedRegions.IsEmpty());
		return MappedRegions.Last();
	};

	const TUniquePtr<IMappedFileRegion>& GetCurrentMappedRegion() const
	{
		check(!MappedRegions.IsEmpty());
		return MappedRegions.Last();
	};

private:
	TUniquePtr<IMappedFileHandle> MappedCacheFile;
	TArray<TUniquePtr<IMappedFileRegion>> MappedRegions;
	uint32 CurrentMappingRegionOffset = 0;
	TSet<FOpenGLProgramKey> Content;
};

static FCriticalSection GProgramBinaryFileCacheCS;

// guards the container that collects scanned programs and send to RHIT
static FCriticalSection GPendingGLProgramCreateRequestsCS;

FOpenGLProgramBinaryCache* FOpenGLProgramBinaryCache::CachePtr = nullptr;

FOpenGLProgramBinaryCache::FOpenGLProgramBinaryCache(const FString& InCachePathRoot)
	: CachePathRoot(InCachePathRoot)
	, BinaryCacheWriteFileHandle(nullptr)
	, CurrentBinaryFileState(EBinaryFileState::Uninitialized)
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
	
	HashString.Append(AndroidEGL::GetInstance()->IsUsingRobustContext() ? TEXT("ROBUST") : TEXT("NRB"));

	// Optional configrule variable for triggering a rebuild of the cache.
	const FString* ConfigRulesGLProgramKey = FAndroidMisc::GetConfigRulesVariable(TEXT("OpenGLProgramCacheKey"));
	if (ConfigRulesGLProgramKey && !ConfigRulesGLProgramKey->IsEmpty())
	{
		HashString.Append(*ConfigRulesGLProgramKey);
	}
#endif

	FSHAHash VersionHash;
	FSHA1::HashBuffer(TCHAR_TO_ANSI(*HashString), HashString.Len(), VersionHash.Hash);

	CacheSubDir = LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform).ToString() + TEXT("_") + VersionHash.ToString();

	// delete anything from the binary program root that does not match the device string.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *(CachePathRoot/TEXT("*")), true, true);
	for(FString& FoundFile : FoundFiles)
	{
		const FString FullPath = (CachePathRoot / FoundFile);
		const bool bIsDir = PlatformFile.DirectoryExists(*FullPath);
		if (FoundFile != CacheSubDir || !bIsDir)
		{
			bool bSuccess;
			if(bIsDir)
			{
				bSuccess = PlatformFile.DeleteDirectoryRecursively(*FullPath);
			}
			else
			{
				bSuccess = PlatformFile.DeleteFile(*FullPath);
			}
			UE_LOG(LogRHI, Verbose, TEXT("FOpenGLProgramBinaryCache Deleting %s %s"), bIsDir ? TEXT("dir") : TEXT("file"), *FullPath );
			UE_CLOG(!bSuccess, LogRHI, Warning, TEXT("FOpenGLProgramBinaryCache Failed to delete %s"), *FullPath);
		}
	}
}

FOpenGLProgramBinaryCache::~FOpenGLProgramBinaryCache()
{
#if PLATFORM_ANDROID
	if (FAndroidOpenGL::AreRemoteCompileServicesActive())
	{
		FAndroidOpenGL::StopRemoteCompileServices();
	}
#endif

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

extern bool IsPrecachingEnabled();

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


	FString CacheFolderPathRoot;
	FString OldCacheFolderPathRoot;
#if PLATFORM_ANDROID && USE_ANDROID_FILE
	// @todo Lumin: Use that GetPathForExternalWrite or something?
	extern FString GExternalFilePath;
	OldCacheFolderPathRoot = GExternalFilePath / TEXT("ProgramBinaryCache");
	CacheFolderPathRoot = GExternalFilePath / TEXT("RHICache") / TEXT("ProgramBinaryCache");
#else
	OldCacheFolderPathRoot = FPaths::ProjectSavedDir() / TEXT("ProgramBinaryCache");
	CacheFolderPathRoot = FPaths::ProjectSavedDir() / TEXT("RHICache") / TEXT("ProgramBinaryCache");
#endif

	// Remove entire ProgramBinaryCache folder if -ClearOpenGLBinaryProgramCache is specified on command line
	if (FParse::Param(FCommandLine::Get(), TEXT("ClearOpenGLBinaryProgramCache")))
	{
		UE_LOG(LogRHI, Log, TEXT("ClearOpenGLBinaryProgramCache specified, deleting binary program cache folder: %s"), *CacheFolderPathRoot);
		FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*CacheFolderPathRoot);
	}

	if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*OldCacheFolderPathRoot))
	{
		UE_LOG(LogRHI, Log, TEXT("Moving program binary cache: %s -> %s"), *OldCacheFolderPathRoot, *CacheFolderPathRoot);
		
		// Note: have to copy and delete as TManagedStoragePlatformFile prevents moving of directories.
		// FPlatformFileManager::Get().GetPlatformFile().MoveFile(*CacheFolderPathRoot, *OldCacheFolderPathRoot);

		FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*CacheFolderPathRoot, *OldCacheFolderPathRoot, false);
		FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*OldCacheFolderPathRoot);
	}

	CachePtr = new FOpenGLProgramBinaryCache(CacheFolderPathRoot);
	UE_LOG(LogRHI, Log, TEXT("Enabling program binary cache dir at %s"), *CachePtr->GetProgramBinaryCacheDir());


	if (IsPrecachingEnabled())
	{
		CachePtr->InitPrecaching();
	}
	else
	{
		// Add delegates for the ShaderPipelineCache precompile.
		UE_LOG(LogRHI, Log, TEXT("FOpenGLProgramBinaryCache will be initialized when ShaderPipelineCache opens its file"));
		CachePtr->OnShaderPipelineCacheOpenedDelegate = FShaderPipelineCache::GetCacheOpenedDelegate().AddRaw(CachePtr, &FOpenGLProgramBinaryCache::OnShaderPipelineCacheOpened);
		CachePtr->OnShaderPipelineCachePrecompilationCompleteDelegate = FShaderPipelineCache::GetPrecompilationCompleteDelegate().AddRaw(CachePtr, &FOpenGLProgramBinaryCache::OnShaderPipelineCachePrecompilationComplete);
	}
}

#if PLATFORM_ANDROID
static int32 GNumRemoteProgramCompileServices = 4;
static FAutoConsoleVariableRef CVarNumRemoteProgramCompileServices(
	TEXT("Android.OpenGL.NumRemoteProgramCompileServices"),
	GNumRemoteProgramCompileServices,
	TEXT("The number of separate processes to make available to compile opengl programs.\n")
	TEXT("0 to disable use of separate processes to precompile Programs\n")
	TEXT("valid range is 1-8 (4 default).")
	,
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);
#endif


namespace UE
{
	namespace OpenGL
	{
		static bool IsBinaryCacheValid(const FString& CachePath, const FGuid* OptionalGuidCheck, bool bLogWhenInvalid, UE::OpenGL::FBinaryCacheFileHeader* HeaderOUT)
		{
			TUniquePtr<FArchive> BinaryProgramReader = nullptr;
			BinaryProgramReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*CachePath));
			if (BinaryProgramReader.IsValid())
			{
				FArchive& Ar = *BinaryProgramReader;
				UE::OpenGL::FBinaryCacheFileHeader BinaryCacheHeader;
				if (Ar.TotalSize() > 0)
				{
					Ar << BinaryCacheHeader;
				}

				BinaryProgramReader->Close();

				if (BinaryCacheHeader.IsValid(OptionalGuidCheck))
				{
					if (HeaderOUT)
					{
						*HeaderOUT = BinaryCacheHeader;
					}
					// we could validate file content.
					return true;
				}
				UE_CLOG(bLogWhenInvalid, LogRHI, Warning, TEXT("FOpenGLProgramBinaryCache: %s is an invalid binary cache (%s)"), *CachePath, *BinaryCacheHeader.ToString());
			}
			else
			{
				UE_CLOG(bLogWhenInvalid, LogRHI, Warning, TEXT("FOpenGLProgramBinaryCache: %s not found."), *CachePath);
			}

			return false;
		}
	}
}

FShaderPipelineCache::FShaderCachePrecompileContext GShaderCachePrecompileContext;

void FOpenGLProgramBinaryCache::InitPrecaching()
{
	UE_LOG(LogRHI, Log, TEXT("FOpenGLProgramBinaryCache: using precache system."));

	FScopeLock Lock(&GProgramBinaryFileCacheCS);

	const FString PreCacheName(TEXT("PrecacheBinaries"));

	const FString RootDir = CachePathRoot / CacheSubDir;

	TArray<FString> CacheFileNames;
	IFileManager::Get().FindFiles(CacheFileNames, *RootDir, TEXT("*"));
	for (FString& Filename : CacheFileNames)
	{
		Filename = RootDir / Filename;
	}
	
	// remove old caches (such as those used by the PSO file cache) or precache caches that are oversized.
	UE::OpenGL::FBinaryCacheFileHeader PrecacheBinaryHeader;

	for (const FString& CacheFileName : CacheFileNames)
	{
		FString CacheName = FPaths::GetBaseFilename(CacheFileName);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FFileStatData CacheStatData = PlatformFile.GetStatData(*CacheFileName);

		bool bDeleteMe;
		if (CacheName.Equals(PreCacheName) && CacheStatData.FileSize <= GBinaryCacheMaxPermittedSizeMB * 1024 * 1024)
		{
			bDeleteMe = !UE::OpenGL::IsBinaryCacheValid(CacheFileName, nullptr, true, &PrecacheBinaryHeader);
		}
		else
		{
			bDeleteMe = true;
		}

		if (bDeleteMe)
		{
			UE_LOG(LogRHI, Log, TEXT("Deleting binary cache file %s (%d bytes)"), *CacheFileName, CacheStatData.FileSize);
			PlatformFile.DeleteFile(*CacheFileName);
		}
	}

	// Use the existing cache's guid to load it.
	FGuid CacheGuid = PrecacheBinaryHeader.IsValid(nullptr) ? PrecacheBinaryHeader.BinaryCacheGuid : FGuid::NewGuid();
	GShaderCachePrecompileContext = FShaderPipelineCache::FShaderCachePrecompileContext(PreCacheName);

	UE_LOG(LogRHI, Log, TEXT("InitPrecaching : Beginning new cache = %s"), *GShaderCachePrecompileContext.GetCacheName());
	OnShaderPipelineCacheOpened(
		PreCacheName,
		GetFeatureLevelShaderPlatform(FOpenGL::GetFeatureLevel()),
		1,	// count must be > 0. 0 sized caches are ignored.
		CacheGuid,
		GShaderCachePrecompileContext);
}

// if the file has sufficiently grown flush the program cache and mmap the extra programs.
void FOpenGLProgramBinaryCache::UpdatePrecacheMapping()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLUpdatePrecacheMapping);
	// TODO: avoid this lock.
	FScopeLock Lock(&GProgramBinaryFileCacheCS);
	FArchive& DestAr = *BinaryCacheWriteFileHandle;
	int64 DestPos = DestAr.Tell();

	if (DestPos >= CurrentShaderPipelineProperties.LastMappedPosition + (GBinaryCacheMMapAfterEveryMB * 1024 * 1024))
	{
		int32 ProgramCount = ProgramsInCurrentCache.Num();
		MarkValidContent(ProgramCount);

		UE_LOG(LogRHI, Log, TEXT("UpdatePrecacheMapping : Mapping program cache from %d to %d "), CurrentShaderPipelineProperties.LastMappedPosition, DestPos);

		TUniquePtr<FArchive> BinaryProgramReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*GetProgramBinaryCacheFilePath()));
 		BinaryProgramReader->Seek(CurrentShaderPipelineProperties.LastMappedPosition);
		int32 ProgramsToRead = ProgramCount - CurrentShaderPipelineProperties.MappedPrograms;
 		bool bSuccess = ReadProgramFile_Internal(ProgramsToRead, DestPos, GetProgramBinaryCacheFilePath(), *BinaryProgramReader);
		CurrentShaderPipelineProperties.MappedPrograms = ProgramCount;
		check(bSuccess);
	}
}

void FOpenGLProgramBinaryCache::OnShaderPipelineCacheOpened(FString const& Name, EShaderPlatform Platform, uint32 Count, const FGuid& VersionGuid, FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext)
{
	FScopeLock Lock(&GProgramBinaryFileCacheCS);

	checkf(CurrentShaderPipelineProperties.CacheVersionGuid == FGuid(), TEXT("OGL: OnShaderPipelineCacheOpened, previous PSO cache %s (%s) has not completed!"), *CurrentShaderPipelineProperties.PipelineCacheName, *CurrentShaderPipelineProperties.CacheVersionGuid.ToString());
	CurrentShaderPipelineProperties.CacheVersionGuid = VersionGuid;
	CurrentShaderPipelineProperties.PipelineCacheName = ShaderCachePrecompileContext.GetCacheName();

	if (Count == 0)
	{		
		check(CurrentBinaryFileState == EBinaryFileState::Uninitialized);
		UE_LOG(LogRHI, Verbose, TEXT("OnShaderPipelineCacheOpened, Ignoring empty PSO cache. %s (%s)"), *CurrentShaderPipelineProperties.PipelineCacheName, *CurrentShaderPipelineProperties.CacheVersionGuid.ToString());
		return;
	}
	 
	UE_LOG(LogRHI, Log, TEXT("Scanning Binary program cache, using Shader Pipeline Cache %s (%s)"), *CurrentShaderPipelineProperties.PipelineCacheName, *CurrentShaderPipelineProperties.CacheVersionGuid.ToString());

	ScanProgramCacheFile();
	if (IsBuildingCache_internal())
	{
#if PLATFORM_ANDROID
		if (GNumRemoteProgramCompileServices)
		{
			check(!FAndroidOpenGL::AreRemoteCompileServicesActive() || IsPrecachingEnabled());
			if(FAndroidOpenGL::AreRemoteCompileServicesActive() == false)
			{
				FAndroidOpenGL::StartAndWaitForRemoteCompileServices(GNumRemoteProgramCompileServices);
			}
		}
#endif
		ShaderCachePrecompileContext.SetPrecompilationIsSlowTask();
	}
}

void FOpenGLProgramBinaryCache::Reset()
{
	check(!BinaryCacheWriteFileHandle);
	CurrentBinaryFileState = EBinaryFileState::Uninitialized;
 	ProgramsInCurrentCache.Empty();
}

void FOpenGLProgramBinaryCache::OnShaderPipelineCachePrecompilationComplete(uint32 Count, double Seconds, const FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLOnShaderPipelineCachePrecompilationComplete);
	FScopeLock Lock(&GProgramBinaryFileCacheCS);

	// We discard the cache if 0 entries were recorded:
	// if 0 programs were cached when Count >0, then we suffer a performance penalty for invoking the services for no reason.
	
	const bool bIsBuildingCache = IsBuildingCache_internal();
	TRefCountPtr<FOpenGLProgramBinaryMapping>*FoundCache = MappedCacheFiles.Find(CurrentShaderPipelineProperties.CacheVersionGuid);
	const int32 ProgramsInLoadedCache = FoundCache ? (*FoundCache)->NumPrograms() : 0;
	const int32 ProgramsCached = bIsBuildingCache ? ProgramsInCurrentCache.Num() : ProgramsInLoadedCache;

	check(!bIsBuildingCache || Count); // we always start cache building if the count>0
	check(bIsBuildingCache || Count == 0 || CurrentBinaryFileState == EBinaryFileState::ValidCacheFile);

	const TCHAR* CacheStatusText = bIsBuildingCache ? 
		(ProgramsCached == 0 ? TEXT("empty cache discarded") : TEXT("cache built"))
		:
		(Count == 0 ? TEXT("ignored empty cache") : TEXT("cache loaded"));

	UE_LOG(LogRHI, Log, TEXT("OnShaderPipelineCachePrecompilationComplete: %s(%s) - %s %d program binaries (%d requested)"), *CurrentShaderPipelineProperties.PipelineCacheName, *CurrentShaderPipelineProperties.CacheVersionGuid.ToString(), CacheStatusText, ProgramsCached, Count );

	if (bIsBuildingCache)
	{
#if PLATFORM_ANDROID
		if ( !IsPrecachingEnabled() && FAndroidOpenGL::AreRemoteCompileServicesActive())
		{
			FAndroidOpenGL::StopRemoteCompileServices();
		}
#endif

		const bool bSuccess = CloseCacheWriteHandle(ProgramsInCurrentCache.Num());

		if(bSuccess && CVarRestartAndroidAfterPrecompile.GetValueOnAnyThread() == 1)
		{
#if PLATFORM_ANDROID
				FAndroidMisc::bNeedsRestartAfterPSOPrecompile = true;
#if USE_ANDROID_JNI
				extern void AndroidThunkCpp_RestartApplication(const FString & IntentString);
				AndroidThunkCpp_RestartApplication(TEXT(""));
#endif
#endif
		}
		
		Reset();
		if(bSuccess)
		{
			ScanProgramCacheFile();
			check(!IsBuildingCache_internal());
			if (IsBuildingCache_internal())
			{
				UE_LOG( LogRHI, Error, TEXT("Failed to load just completed cache! : %s(%s)"), *CurrentShaderPipelineProperties.PipelineCacheName, *CurrentShaderPipelineProperties.CacheVersionGuid.ToString());
				// The cache we've just written is invalid. This is extremely unlikely.
				CloseCacheWriteHandle(ProgramsInCurrentCache.Num());
			}
		}
	}

	// unset the completed cache.
	Reset();

	CurrentShaderPipelineProperties.CacheVersionGuid = FGuid();
	CurrentShaderPipelineProperties.PipelineCacheName.Reset();
	CurrentShaderPipelineProperties.LastMappedPosition = 0;
	CurrentShaderPipelineProperties.MappedPrograms = 0;
	CurrentShaderPipelineProperties.NumProgramsFlushed = 0;
}

TRefCountPtr<FOpenGLProgramBinaryMapping> FOpenGLProgramBinaryCache::GetOrAddFileMapping(const FString& ProgramCacheFilename, int32 ProgramCount, int64 Offset, int64 Size)
{
	TRefCountPtr<FOpenGLProgramBinaryMapping> CurrentMapping;
	if (!UE::OpenGL::CanMemoryMapGLProgramCache())
	{
		return CurrentMapping;
	}

	TRefCountPtr<FOpenGLProgramBinaryMapping>* FoundMapping = MappedCacheFiles.Find(CurrentShaderPipelineProperties.CacheVersionGuid);
	if( FoundMapping == nullptr )
	{
		TUniquePtr<IMappedFileHandle> MappedCacheFile;
		if(UE::OpenGL::CanMemoryMapGLProgramCache())
		{
			MappedCacheFile = TUniquePtr<IMappedFileHandle>(FPlatformFileManager::Get().GetPlatformFile().OpenMapped(*ProgramCacheFilename));
		}
		CurrentMapping = new FOpenGLProgramBinaryMapping(MoveTemp(MappedCacheFile), ProgramCount);
		MappedCacheFiles.Add(CurrentShaderPipelineProperties.CacheVersionGuid, CurrentMapping);		
	}
	else
	{
		CurrentMapping = *FoundMapping;
	}

	if(UE::OpenGL::CanMemoryMapGLProgramCache())
	{
		TUniquePtr<IMappedFileHandle>& MappedCacheFile = CurrentMapping->GetMappedCacheFile();

		// add the new mapping
		if (ensure(MappedCacheFile.IsValid()))
		{
			check(Size);
			TUniquePtr<IMappedFileRegion> MappedRegion = TUniquePtr<IMappedFileRegion>(MappedCacheFile->MapRegion(Offset, Size));
			check(MappedRegion.IsValid());
			CurrentMapping->AddMapping(Offset, MoveTemp(MappedRegion));
		}
	}

	return CurrentMapping;
}

// Scan the binary cache file and build a record of all programs.
void FOpenGLProgramBinaryCache::ScanProgramCacheFile()
{
	//FScopedDurationTimeLogger Timer(TEXT("ScanProgramCacheFile"));

	UE_LOG(LogRHI, Log, TEXT("OnShaderScanProgramCacheFile"));
	FString ProgramCacheFilename = GetProgramBinaryCacheFilePath();
	FString ProgramCacheFilenameTemp = ProgramCacheFilename + TEXT(".scan");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	check(CurrentBinaryFileState == EBinaryFileState::Uninitialized);
	check(ProgramsInCurrentCache.IsEmpty());

	bool bBinaryFileIsValid = false;

	// Try to move the file to a temporary filename before the scan, so we won't try to read it again if it's corrupted
	PlatformFile.DeleteFile(*ProgramCacheFilenameTemp);
	PlatformFile.MoveFile(*ProgramCacheFilenameTemp, *ProgramCacheFilename);

	TUniquePtr<FArchive> BinaryProgramReader = nullptr;
	BinaryProgramReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*ProgramCacheFilenameTemp));

	if (!BinaryProgramReader)
	{
		UE_LOG(LogRHI, Log, TEXT("OnShaderScanProgramCacheFile : %s was not found, recreating."), *ProgramCacheFilename);
	}
	else
	{
		UE::OpenGL::FBinaryCacheFileHeader BinaryCacheHeader;
		FArchive& Ar = *BinaryProgramReader;
		if (Ar.TotalSize() > 0)
		{
			Ar << BinaryCacheHeader;
		}
		bBinaryFileIsValid = BinaryCacheHeader.IsValid(&CurrentShaderPipelineProperties.CacheVersionGuid);
		if (!bBinaryFileIsValid)
		{
			UE_LOG(LogRHI, Warning, TEXT("ScanProgramCacheFile - invalid binary cache file encountered (%s, %d). Rebuilding binary program cache."), *BinaryCacheHeader.ToString(), Ar.TotalSize());
			BinaryProgramReader->Close();
			PlatformFile.DeleteFile(*ProgramCacheFilenameTemp);
		}
		else
		{
			UE_LOG(LogRHI, Warning, TEXT("ScanProgramCacheFile - reading binary cache, Programs %d, ValidSize: %d, TotalSize %d"), BinaryCacheHeader.ProgramCount, BinaryCacheHeader.ValidSize, Ar.TotalSize());
			ProgramsInCurrentCache.Reserve(BinaryCacheHeader.ProgramCount);
			bool bSuccess = ReadProgramFile_Internal(BinaryCacheHeader.ProgramCount, BinaryCacheHeader.ValidSize, ProgramCacheFilenameTemp, Ar);
			check(bSuccess);
		}
	}

	if (!bBinaryFileIsValid)
	{
		if (OpenCacheWriteHandle(GetProgramBinaryCacheFilePath(), false))
		{
			CurrentBinaryFileState = EBinaryFileState::BuildingCacheFile;
			// save header, 0 program count indicates an unfinished file.
			// The header is overwritten at the end of the process.
			UE::OpenGL::FBinaryCacheFileHeader OutHeader = UE::OpenGL::FBinaryCacheFileHeader::CreateHeader(CurrentShaderPipelineProperties.CacheVersionGuid, 0, 0);
			FArchive& Ar = *BinaryCacheWriteFileHandle;
			Ar << OutHeader;
			CurrentShaderPipelineProperties.LastMappedPosition = Ar.Tell();
		}
		else
		{
			// Binary cache file cannot be used, failed to open output file.
			CurrentBinaryFileState = EBinaryFileState::Uninitialized;
			RHIGetPanicDelegate().ExecuteIfBound(FName("FailedBinaryProgramArchiveOpen"));
			UE_LOG(LogRHI, Fatal, TEXT("ScanProgramCacheFile - Failed to open binary cache."));
		}
	}
	else
	{
		uint32 LastReadPos = BinaryProgramReader->Tell();
		BinaryProgramReader->Close();

		// Rename the file back after a successful scan.
		PlatformFile.MoveFile(*ProgramCacheFilename, *ProgramCacheFilenameTemp);
		CurrentBinaryFileState = EBinaryFileState::ValidCacheFile;

		if (IsPrecachingEnabled())
		{
			if (OpenCacheWriteHandle(GetProgramBinaryCacheFilePath(), true))
			{
				check(LastReadPos == CurrentShaderPipelineProperties.LastMappedPosition);
				CurrentBinaryFileState = EBinaryFileState::BuildingCacheFile;
				BinaryCacheWriteFileHandle->Seek(CurrentShaderPipelineProperties.LastMappedPosition);
			}
		}
	}
}

bool FOpenGLProgramBinaryCache::ReadProgramFile_Internal(uint32 ProgramsToRead, int64 EndOffset, const FString& ProgramCacheFilename, FArchive& Ar)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLReadBinaryProgramCache);
	int32 ProgramIndex = 0;
	FScopeLock Lock(&GPendingGLProgramCreateRequestsCS);
	PendingGLContainerPrograms.Reserve(ProgramsToRead);
	int64 MappingSize = EndOffset - CurrentShaderPipelineProperties.LastMappedPosition;
	TRefCountPtr<FOpenGLProgramBinaryMapping> CurrentMapping = GetOrAddFileMapping(ProgramCacheFilename, ProgramsToRead, CurrentShaderPipelineProperties.LastMappedPosition, MappingSize);

	UE_LOG(LogRHI, Log, TEXT("OnShaderScanProgramCacheFile : %s %s"), CurrentMapping->HasValidMapping() ? TEXT("mapped") : TEXT("opened"), *ProgramCacheFilename);

	while (Ar.Tell() < EndOffset)
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

			ProgramsInCurrentCache.Add(ProgramKey);

			if (CurrentMapping->HasValidMapping())
			{
				PendingGLContainerPrograms.Emplace(ProgramKey, TUniqueObj<FOpenGLProgramBinary>(CurrentMapping->GetView(ProgramBinaryOffset, ProgramBinarySize)));
				Ar.Seek(ProgramBinaryOffset + ProgramBinarySize);
			}
			else
			{
				check(!UE::OpenGL::CanMemoryMapGLProgramCache());
				TArray<uint8> ProgramBytes;
				ProgramBytes.SetNumUninitialized(ProgramBinarySize);
				Ar.Serialize(ProgramBytes.GetData(), ProgramBinarySize);
				PendingGLContainerPrograms.Emplace(ProgramKey, TUniqueObj<FOpenGLProgramBinary>(MoveTemp(ProgramBytes)));
			}
		}
		else
		{
			return false;
		}
	}

	CurrentShaderPipelineProperties.LastMappedPosition = Ar.Tell();

	UE_LOG(LogRHI, VeryVerbose, TEXT("Program Binary cache: Found %d cached programs"), ProgramIndex);
	UE_CLOG(ProgramIndex != ProgramsToRead, LogRHI, Error, TEXT("Program Binary cache: Mismatched program count! expected: %d"), ProgramsToRead);
	return true;
}

bool FOpenGLProgramBinaryCache::OpenCacheWriteHandle(const FString& ProgramCacheFilenameToWrite, bool bAppendToExisting)
{
	check(BinaryCacheWriteFileHandle == nullptr);

	BinaryCacheWriteFileHandle = IFileManager::Get().CreateFileWriter(*ProgramCacheFilenameToWrite, EFileWrite::FILEWRITE_AllowRead | (bAppendToExisting ? EFileWrite::FILEWRITE_Append : EFileWrite::FILEWRITE_None));
	UE_CLOG(BinaryCacheWriteFileHandle, LogRHI, Log, TEXT("Opened binary cache for write (%s)"), *ProgramCacheFilenameToWrite);
	UE_CLOG(BinaryCacheWriteFileHandle == nullptr, LogRHI, Warning, TEXT("Failed to open OGL binary cache output file. (%s)"), *ProgramCacheFilenameToWrite);
	UE_CLOG(BinaryCacheWriteFileHandle && (BinaryCacheWriteFileHandle->IsError() || BinaryCacheWriteFileHandle->IsCriticalError()), LogRHI, Error, TEXT("OGL binary cache output archive error (%s, %d,%d)"), *ProgramCacheFilenameToWrite, BinaryCacheWriteFileHandle->IsError(), BinaryCacheWriteFileHandle->IsCriticalError());
	return BinaryCacheWriteFileHandle != nullptr;
}

// Update the file's header to indicate the last successful write location. This allows the app to ignore a truncated file such as after an abnormal exit.
bool FOpenGLProgramBinaryCache::MarkValidContent(int32 NumPrograms)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLMarkValidBinaryProgramCache);
	check(BinaryCacheWriteFileHandle != nullptr);
	const bool bCacheFileIsEmpty = NumPrograms == 0;

	uint32 FailCode = BinaryCacheWriteFileHandle->IsError() || BinaryCacheWriteFileHandle->IsCriticalError() ? 1 : 0;

	// Overwrite the header with the final program count. This indicates a successful write.
	if (FailCode == 0)
	{
		FArchive& Ar = *BinaryCacheWriteFileHandle;
		int64 CurrPos = Ar.Tell();
		Ar.Seek(0);
		UE::OpenGL::FBinaryCacheFileHeader OutHeader = UE::OpenGL::FBinaryCacheFileHeader::CreateHeader(CurrentShaderPipelineProperties.CacheVersionGuid, NumPrograms, (int32)CurrPos);
		UE_LOG(LogRHI, Verbose, TEXT("MarkValidContent, file valid at %d Programs, %d bytes."), NumPrograms, CurrPos);
		Ar << OutHeader;
		Ar.Seek(CurrPos);
		Ar.Flush();

		FailCode = BinaryCacheWriteFileHandle->IsError() || BinaryCacheWriteFileHandle->IsCriticalError() ? 2 : 0;
	}

	if (FailCode != 0)
	{
		RHIGetPanicDelegate().ExecuteIfBound(FName("FailedBinaryProgramArchiveWrite"));
		UE_LOG(LogRHI, Fatal, TEXT("MarkValidContent - FArchive error bit set, failed to write binary cache. %d, %d"), NumPrograms, FailCode);
	}
	CurrentShaderPipelineProperties.NumProgramsFlushed = NumPrograms;

	return !bCacheFileIsEmpty;
}

bool FOpenGLProgramBinaryCache::CloseCacheWriteHandle(int32 NumProgramsAdded)
{
	check(BinaryCacheWriteFileHandle != nullptr);
	const bool bCacheFileIsEmpty = NumProgramsAdded == 0;

	uint32 FailCode = BinaryCacheWriteFileHandle->IsError() || BinaryCacheWriteFileHandle->IsCriticalError() ? 1 : 0;

	// Overwrite the header with the final program count. This indicates a successful write.
	if(FailCode == 0)
	{
		FArchive& Ar = *BinaryCacheWriteFileHandle;
		Ar.Seek(0);
		UE::OpenGL::FBinaryCacheFileHeader OutHeader = UE::OpenGL::FBinaryCacheFileHeader::CreateHeader(CurrentShaderPipelineProperties.CacheVersionGuid, NumProgramsAdded, (int32)Ar.TotalSize());
		Ar << OutHeader;
		FailCode = BinaryCacheWriteFileHandle->IsError() || BinaryCacheWriteFileHandle->IsCriticalError() ? 2 : 0;
	}

	BinaryCacheWriteFileHandle->Close();
	delete BinaryCacheWriteFileHandle;
	BinaryCacheWriteFileHandle = nullptr;

	const FString ProgramCacheFilename = GetProgramBinaryCacheFilePath();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (FailCode != 0)
	{
		RHIGetPanicDelegate().ExecuteIfBound(FName("FailedBinaryProgramArchiveWrite"));
		UE_LOG(LogRHI, Fatal, TEXT("CloseCacheWriteHandle - FArchive error bit set, failed to write binary cache. %d"), FailCode);
	}

	if (bCacheFileIsEmpty)
	{
		// we dont want empty files left on disk.
		PlatformFile.DeleteFile(*ProgramCacheFilename);
	}

	return !bCacheFileIsEmpty;
}

void FOpenGLProgramBinaryCache::CacheProgramBinary(const FOpenGLProgramKey& ProgramKey, TUniqueObj<FOpenGLProgramBinary>&& ProgramBinary)
{
	if (CachePtr)
	{
		FScopeLock Lock(&GProgramBinaryFileCacheCS);

		if (!CachePtr->ProgramsInCurrentCache.Contains(ProgramKey))
		{
			CachePtr->AddProgramBinaryDataToBinaryCache(ProgramKey, ProgramBinary.Get());

			if (IsPrecachingEnabled())
			{
				// If we're precaching then we need to send it to RHIT immediately. without this we'll wait for this portion of the in progress file to be mmapped.
				EnqueueBinaryForGLProgramContainer(ProgramKey, MoveTemp(ProgramBinary));
			}
		}
	}
}

// Serialize out the program binary data and add to runtime structures.
void FOpenGLProgramBinaryCache::AddProgramBinaryDataToBinaryCache(const FOpenGLProgramKey& ProgramKey, const FOpenGLProgramBinary& BinaryProgramData)
{
	check(IsBuildingCache_internal());
	check(BinaryProgramData.IsValid());
	FArchive& Ar = *BinaryCacheWriteFileHandle;
	bool bInitiallyValid = !(Ar.IsError() || Ar.IsCriticalError());
	// Serialize to output file:
	FOpenGLProgramKey SerializedProgramKey = ProgramKey;
	const TArrayView<const uint8> BinaryProgramDataView = BinaryProgramData.GetDataView();
	uint32 ProgramBinarySize = (uint32)BinaryProgramDataView.Num();
	const uint8* ProgramBinaryBytes = BinaryProgramDataView.GetData();
	uint32 Start = Ar.Tell();
	check(SerializedProgramKey != FOpenGLProgramKey());
	Ar << SerializedProgramKey;
	uint32 ProgramBinaryOffset = Ar.Tell();
	check(ProgramBinarySize > 0);
	Ar << ProgramBinarySize;
	Ar.Serialize(const_cast<uint8*>(ProgramBinaryBytes), ProgramBinarySize);
	uint32 End = Ar.Tell();

	UE_CLOG(Ar.IsError() || Ar.IsCriticalError(), LogRHI, Error, TEXT("AddProgramBinaryDataToBinaryCache : archive failed (%d, %d, %d, %d, %d, %d)"), Ar.IsError(), Ar.IsCriticalError(), Start, End, ProgramBinarySize, bInitiallyValid);
	UE_LOG(LogRHI, VeryVerbose, TEXT("AddProgramBinaryDataToBinaryCache : added %d bytes to cache"), End-Start);

	if (UE::OpenGL::AreBinaryProgramsCompressed())
	{
		static uint32 TotalUncompressed = 0;
		static uint32 TotalCompressed = 0;

		const UE::OpenGL::FCompressedProgramBinaryHeader* Header = (UE::OpenGL::FCompressedProgramBinaryHeader*)ProgramBinaryBytes;
		TotalUncompressed += Header->UncompressedSize;
		TotalCompressed += ProgramBinarySize;

		UE_LOG(LogRHI, Verbose, TEXT("AppendProgramBinaryFile: total Uncompressed: %d, total Compressed %d, Total saved so far: %d"), TotalUncompressed, TotalCompressed, TotalUncompressed - TotalCompressed);
	}
	UE_LOG(LogRHI, VeryVerbose, TEXT("AddProgramBinaryDataToBinaryCache: written Program %s to cache (%d bytes)"), *ProgramKey.ToString(), BinaryProgramDataView.Num());

	ProgramsInCurrentCache.Add(ProgramKey);

 	if (IsPrecachingEnabled())
 	{
		int32 NumProgramsStored = ProgramsInCurrentCache.Num();
		if (NumProgramsStored >= CurrentShaderPipelineProperties.NumProgramsFlushed + GBinaryCachePeriodicFlushProgramCount)
		{
			MarkValidContent(NumProgramsStored);
		}
 	}
}

void FOpenGLProgramBinaryCache::EnqueueBinaryForGLProgramContainer(const FOpenGLProgramKey& ProgramKey, TUniqueObj<FOpenGLProgramBinary>&& ProgramBinary)
{
	if (CachePtr)
	{
		FScopeLock Lock(&GPendingGLProgramCreateRequestsCS);
		CachePtr->PendingGLContainerPrograms.Emplace(ProgramKey, MoveTemp(ProgramBinary));
	}
}

void FOpenGLProgramBinaryCache::Shutdown()
{
	if (CachePtr)
	{
		delete CachePtr;
		CachePtr = nullptr;
	}
}

bool FOpenGLProgramBinaryCache::RequiresCaching(const FOpenGLProgramKey& ProgramKey)
{
	if (CachePtr)
	{
		FScopeLock Lock(&GProgramBinaryFileCacheCS);
		return CachePtr->RequiresCaching_Internal(ProgramKey);
	}
	return false;
}

bool FOpenGLProgramBinaryCache::RequiresCaching_Internal(const FOpenGLProgramKey& ProgramKey)
{
	return !ProgramsInCurrentCache.Contains(ProgramKey);
}

FString FOpenGLProgramBinaryCache::GetProgramBinaryCacheFilePath() const
{
	check(CurrentShaderPipelineProperties.CacheVersionGuid != FGuid());
	FString ProgramFilename = CachePathRoot / CacheSubDir / CurrentShaderPipelineProperties.PipelineCacheName;
	return ProgramFilename;
}

void FOpenGLProgramBinaryCache::TickBinaryCache()
{
	if (CachePtr)
	{
		if(IsPrecachingEnabled())
		{
			CachePtr->UpdatePrecacheMapping();
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLCheckPendingGLProgramCreateRequests);
			check(IsInRenderingThread() || IsInRHIThread());
			FScopeLock Lock(&GPendingGLProgramCreateRequestsCS);
			CachePtr->CheckPendingGLProgramCreateRequests_internal();
		}
	}
}

// Move programs encountered during the scan to the GL RHI program container.
// GMaxBinaryProgramLoadTimeMS attempts to reduce hitching, if we're not using the LRU then we still create GL programs and require more time.
void FOpenGLProgramBinaryCache::CheckPendingGLProgramCreateRequests_internal()
{
	if (PendingGLContainerPrograms.Num() > 0)
	{
		//FScopedDurationTimeLogger Timer(TEXT("CheckPendingGLProgramCreateRequests"));
		float TimeRemainingS = (float)GMaxBinaryProgramLoadTimeMS / 1000.0f;
		double StartTime = FPlatformTime::Seconds();
		int32 Count = 0;

		for (auto It = PendingGLContainerPrograms.CreateIterator(); It && TimeRemainingS > 0.0f; ++It)
		{
			UE::OpenGL::OnGLProgramLoadedFromBinaryCache(It->Key, MoveTemp(It->Value));
			TimeRemainingS -= (float)(FPlatformTime::Seconds() - StartTime);
			StartTime = FPlatformTime::Seconds();
			Count++;
			It.RemoveCurrent();
		}
		float TimeTaken = (float)GMaxBinaryProgramLoadTimeMS - (TimeRemainingS * 1000.0f);
		UE_LOG(LogRHI, Verbose, TEXT("CheckPendingGLProgramCreateRequests : iter count = %d, time taken = %f ms (remaining %d)"), Count, TimeTaken, PendingGLContainerPrograms.Num());
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
	if (PendingGLContainerPrograms.RemoveAndCopyValue(ProgramKey, ProgFound))
	{		
		UE::OpenGL::OnGLProgramLoadedFromBinaryCache(ProgramKey, MoveTemp(ProgFound));
		return true;
	}
	return false;
}
