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
class FOpenGLProgramBinary;
class FOpenGLProgramBinaryMapping;
struct FGLProgramBinaryFileCacheEntry;

class FOpenGLProgramBinaryCache
{
public:
	static void Initialize();
	static void Shutdown();

	static bool IsEnabled();

	/** Extract the program binary from GL and store on disk. */
	static FOpenGLProgramBinary CreateAndCacheProgramBinary(const FOpenGLProgramKey& ProgramKey, GLuint Program);

	/** Is the program already in the currently building cache */
	static bool DoesOutputCacheContain(const FOpenGLProgramKey& ProgramKey);

	/** Take an existing program binary and store on disk. Only when ProgramBinaryCache is enabled
	*/
	static void CacheProgramBinary(const FOpenGLProgramKey& ProgramKey, const FOpenGLProgramBinary& CachedProgramBinary);

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

	// Guid of the PSO cache being procesed.
	FGuid CurrentShaderPipelineCacheVersionGuid;

	/**
	* Shaders that were requested for compilation
	* They will be compiled just before linking a program only in case when there is no saved binary program
	*/
	TMap<GLuint, FPendingShaderCode> ShadersPendingCompilation;

	bool AppendProgramBinaryFile(FArchive& Ar, const FOpenGLProgramKey& ProgramKey, GLuint Program, uint32& ProgramBinaryOffsetOUT, uint32& ProgramBinarySizeOUT);
	void AppendProgramBinaryFileEofEntry(FArchive& Ar);

	void Reset();

	void ScanProgramCacheFile();

	void OpenAsyncReadHandle();
	void CloseAsyncReadHandle();

	bool OpenWriteHandle();
	void CloseWriteHandle();

	FOpenGLProgramBinary CreateAndCacheProgramBinary_Internal(const FOpenGLProgramKey& ProgramKey, GLuint Program);
	bool DoesOutputCacheContain_Internal(const FOpenGLProgramKey& ProgramKey) const;

	void AddProgramBinaryDataToBinaryCache(const FOpenGLProgramKey& ProgramKey, const FOpenGLProgramBinary& BinaryProgramData);

	void ReleaseGLProgram_internal(FOpenGLLinkedProgramConfiguration& Config, GLuint Program);

	void CheckPendingGLProgramCreateRequests_internal();
	bool CheckSinglePendingGLProgramCreateRequest_internal(const FOpenGLProgramKey& ProgramKey);

	/** Delegate handlers to track the ShaderPipelineCache precompile. */
	void OnShaderPipelineCacheOpened(FString const& Name, EShaderPlatform Platform, uint32 Count, const FGuid& VersionGuid, FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);
	void OnShaderPipelineCachePrecompilationComplete(uint32 Count, double Seconds, const FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);

private:

	FDelegateHandle OnShaderPipelineCacheOpenedDelegate;
	FDelegateHandle OnShaderPipelineCachePrecompilationCompleteDelegate;

	TSet<FOpenGLProgramKey> ProgramsWrittenToOutputCache; // Only populated when creating the cache. Used to prevents duplicates.

	// All of the mmapped binary programs encountered during the cache open scan.
	// The contents are moved to OpenGL's program cache during RHI's end of frame tick.
	TMap<FOpenGLProgramKey, TUniqueObj<FOpenGLProgramBinary>> ScannedBinaryPrograms;

	FArchive* BinaryCacheWriteFileHandle;
	bool bShownLoadingScreen;

	enum class EBinaryFileState : uint8
	{
		Uninitialized,					// No binary file is yet established and we should not read or write to it.
		BuildingCacheFile,				// We are precompiling shaders from the PSO and storing them in a new binary cache. Do not attempt to read.
		ValidCacheFile,					// We have a valid cache file we can use for reading.
	};

	bool IsBuildingCache_internal() const	{ return BinaryFileState == EBinaryFileState::BuildingCacheFile; }

	EBinaryFileState BinaryFileState;

	// Container of all currently open program cache files.
	TMap<FGuid, TRefCountPtr<FOpenGLProgramBinaryMapping>> MappedCacheFiles;
};
