// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLProgramBinaryFileCache.h: OpenGL program binary file cache stores/loads a set of binary ogl programs.
=============================================================================*/

#pragma once

#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/Array.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Misc/SecureHash.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderCore.h"
#include "CrossCompilerCommon.h"
#include "ShaderCodeLibrary.h"
#include "Async/AsyncFileHandle.h"
#include "ShaderPipelineCache.h"

class FOpenGLLinkedProgram;


struct FGLProgramBinaryFileCacheEntry;
class FOpenGLProgramBinaryCache
{
public:
	static void Initialize();
	static void Shutdown();

	static bool IsEnabled();

	/** Compile required shaders for a program, only in case binary program was not found in the cache   */
// 	static void CompilePendingShaders(const FOpenGLLinkedProgramConfiguration& Config);

	/** Try to find and load program binary from cache */
	static bool UseCachedProgram(GLuint& ProgramOUT, const FOpenGLProgramKey& ProgramKey, TArray<uint8>& CachedProgramBinaryOUT);

	/** Extract the program binary from GL and store on disk. Only when ProgramBinaryCache is enabled */
	static void CacheProgram(GLuint Program, const FOpenGLProgramKey& ProgramKey, TArray<uint8>& CachedProgramBinaryOUT);

	/** Take an existing program binary and store on disk. Only when ProgramBinaryCache is enabled
	* CachedProgramBinary should be const, cant be due to serialize api
	*/
	static void CacheProgramBinary(const FOpenGLProgramKey& ProgramKey, /*const*/ TArray<uint8>& CachedProgramBinary);

	static void OnShaderLibraryRequestShaderCode(const FSHAHash& Hash, FArchive* Ar);

	/** Create any pending GL programs that have come from shader library requests */
	static void CheckPendingGLProgramCreateRequests();

	/** Create any single GL program that have come from shader library requests */
	static bool CheckSinglePendingGLProgramCreateRequest(const FOpenGLProgramKey& ProgramKey);

	/** true if the program binary cache is currently in cache build mode */
	static bool IsBuildingCache();

private:
	FOpenGLProgramBinaryCache(const FString& InCachePath);
	~FOpenGLProgramBinaryCache();

	FString GetProgramBinaryCacheFilePath() const;

	struct FPendingShaderCode
	{
		TArray<ANSICHAR> GlslCode;
		int32 UncompressedSize;
		bool bCompressed;
	};

private:
	static TAutoConsoleVariable<int32> CVarPBCEnable;
	static TAutoConsoleVariable<int32> CVarRestartAndroidAfterPrecompile;
	static FOpenGLProgramBinaryCache* CachePtr;

	/*  Path to directory where binary programs will be stored excluding the cache filename */
	FString CachePath;
	/* Just the cache filename, without the path */
	FString CacheFilename;


	/**
	* Shaders that were requested for compilation
	* They will be compiled just before linking a program only in case when there is no saved binary program
	*/
	TMap<GLuint, FPendingShaderCode> ShadersPendingCompilation;

	bool AppendProgramBinaryFile(FArchive& Ar, const FOpenGLProgramKey& ProgramKey, GLuint Program, uint32& ProgramBinaryOffsetOUT, uint32& ProgramBinarySizeOUT);
	void AppendProgramBinaryFileEofEntry(FArchive& Ar);

	void ScanProgramCacheFile(const FGuid& ShaderPipelineCacheVersionGuid = FGuid());

	/* Add a program */
	void AddProgramFileEntryToMap(FGLProgramBinaryFileCacheEntry* IndexEntry);

	void OpenAsyncReadHandle();
	void CloseAsyncReadHandle();

	bool OpenWriteHandle();
	void CloseWriteHandle();

	void AppendGLProgramToBinaryCache(const FOpenGLProgramKey& ProgramKey, GLuint Program, TArray<uint8>& CachedProgramBinaryOUT);
	void AddUniqueGLProgramToBinaryCache(const FOpenGLProgramKey& ProgramKey, GLuint Program, TArray<uint8>& CachedProgramBinaryOUT);

	void AddProgramBinaryDataToBinaryCache(TArray<uint8>& BinaryProgramData, const FOpenGLProgramKey& ProgramKey);

	void ReleaseGLProgram_internal(FOpenGLLinkedProgramConfiguration& Config, GLuint Program);

	FORCEINLINE_DEBUGGABLE bool ShaderIsLoaded(const FSHAHash& Hash)
	{
		const FGLShaderToPrograms* FoundShaderToBinary = CachePtr->ShaderToProgramsMap.Find(Hash);
		return FoundShaderToBinary && FoundShaderToBinary->bLoaded;
	}

	bool UseCachedProgram_internal(GLuint& ProgramOUT, const FOpenGLProgramKey& ProgramKey, TArray<uint8>& CachedProgramBinaryOUT);

	void OnShaderLibraryRequestShaderCode_internal(const FSHAHash& Hash, FArchive* Ar);

	void BeginProgramReadRequest(FGLProgramBinaryFileCacheEntry* IndexEntry, FArchive* Ar);

	void CheckPendingGLProgramCreateRequests_internal();
	bool CheckSinglePendingGLProgramCreateRequest_internal(const FOpenGLProgramKey& ProgramKey);

	void CompleteLoadedGLProgramRequest_internal(FGLProgramBinaryFileCacheEntry* PendingGLCreate);

	/** Delegate handlers to track the ShaderPipelineCache precompile. */
	void OnShaderPipelineCacheOpened(FString const& Name, EShaderPlatform Platform, uint32 Count, const FGuid& VersionGuid, FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);
	void OnShaderPipelineCachePrecompilationComplete(uint32 Count, double Seconds, const FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);

private:

	FDelegateHandle OnShaderPipelineCacheOpenedDelegate;
	FDelegateHandle OnShaderPipelineCachePrecompilationCompleteDelegate;

	TArray<TUniquePtr<FGLProgramBinaryFileCacheEntry>> ProgramEntryContainer; // this is the owner of all FGLProgramBinaryFileCacheEntry ptrs

	TMap<FOpenGLProgramKey, FGLProgramBinaryFileCacheEntry*> ProgramToBinaryMap; // program key to program entry.

	struct FGLShaderToPrograms
	{
		FGLShaderToPrograms() : bLoaded(false)
		{
		}

		FGLShaderToPrograms(FGLProgramBinaryFileCacheEntry* ProgramEntry) : bLoaded(false)
		{
			AssociatedPrograms.Add(ProgramEntry);
		}

		void Add(FGLProgramBinaryFileCacheEntry* ProgramEntry)
		{
			checkSlow(!AssociatedPrograms.Contains(ProgramEntry));
			AssociatedPrograms.Add(ProgramEntry);
		}

		bool bLoaded;
		TArray<FGLProgramBinaryFileCacheEntry*> AssociatedPrograms;
	};

	// Map of shader hash to a list of programs which reference it.
	TMap<FSHAHash, FGLShaderToPrograms> ShaderToProgramsMap;

	// programs loaded via async and now ready for creation on GL context owning thread.
	TArray<FGLProgramBinaryFileCacheEntry*> PendingGLProgramCreateRequests;

	IAsyncReadFileHandle* BinaryCacheAsyncReadFileHandle;
	FArchive* BinaryCacheWriteFileHandle;
	bool bShownLoadingScreen;

	enum class EBinaryFileState : uint8
	{
		Uninitialized,					// No binary file is yet established and we should not read or write to it.
		BuildingCacheFile,				// We are precompiling shaders from the PSO and storing them in a new binary cache. Do not attempt to read.
		BuildingCacheFileWithMove,		// We are precompiling shaders from the PSO and storing them in a new binary cache, shaders matching from the existing cache are moved to the new file. Do not attempt to read.
		ValidCacheFile,					// We have a valid cache file we can use for reading. Do not attempt to write.
	};

	bool IsBuildingCache_internal() const
	{
		return BinaryFileState == EBinaryFileState::BuildingCacheFile || BinaryFileState == EBinaryFileState::BuildingCacheFileWithMove;
	}

	struct FPreviousGLProgramBinaryCacheInfo
	{
		FPreviousGLProgramBinaryCacheInfo();
		FPreviousGLProgramBinaryCacheInfo(FPreviousGLProgramBinaryCacheInfo&&);
		FPreviousGLProgramBinaryCacheInfo& operator=(FPreviousGLProgramBinaryCacheInfo&&);
		~FPreviousGLProgramBinaryCacheInfo();


		FString OldCacheFilename;
		TUniquePtr<FArchive> OldCacheArchive;
		TMap<FOpenGLProgramKey, TUniquePtr<FGLProgramBinaryFileCacheEntry> > ProgramToOldBinaryCacheMap; // program key to program entry for old cache used when generating new cache.

		// for logging
		uint32 NumberOfOldEntriesReused;
	};
	FPreviousGLProgramBinaryCacheInfo PreviousBinaryCacheInfo;

	EBinaryFileState BinaryFileState;
};
