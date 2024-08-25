// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncBuffer.h"
#include "UnsyncCommon.h"
#include "UnsyncHash.h"
#include "UnsyncProtocol.h"
#include "UnsyncSocket.h"
#include "UnsyncUtil.h"
#include "UnsyncManifest.h"
#include "UnsyncHashTable.h"

#include <stdint.h>
#include <deque>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace unsync {

extern bool GDryRun;

static constexpr uint32 MAX_ACTIVE_READERS = 64;

class FProxy;
class FProxyPool;
class FBlockCache;
class FScavengeDatabase;
struct FRemoteDesc;
struct FIOReader;
struct FIOWriter;
struct FIOReaderWriter;

struct FIdentityHash32
{
	uint32 operator()(uint32 X) const { return X; }
};

struct FBlockStrongHash
{
	uint32 operator()(const FBlock128& Block) const
	{
		FHash128::Hasher Hasher;
		return Hasher(Block.HashStrong);
	}

	uint32 operator()(const FGenericBlock& Block) const
	{
		FGenericHash::Hasher Hasher;
		return Hasher(Block.HashStrong);
	}
};

struct FBlockStrongHashEq
{
	bool operator()(const FBlock128& A, const FBlock128& B) const { return A.HashStrong == B.HashStrong; }

	bool operator()(const FGenericBlock& A, const FGenericBlock& B) const { return A.HashStrong == B.HashStrong; }
};

struct FCopyCommand
{
	uint64 Size			= 0;
	uint64 SourceOffset = 0;
	uint64 TargetOffset = 0;

	struct FCompareBySourceOffset
	{
		bool operator()(const FCopyCommand& A, const FCopyCommand& B) const { return A.SourceOffset < B.SourceOffset; }
	};
};

struct FNeedBlock : FCopyCommand
{
	FGenericHash Hash = {};
};

struct FReadSchedule
{
	std::vector<FCopyCommand> Blocks;
	std::deque<uint64>		  Requests;	 // unique block request indices sorted small to large
};

FReadSchedule BuildReadSchedule(const std::vector<FNeedBlock>& Blocks);

class FBlockCache
{
public:
	FBuffer							BlockData;
	THashMap<FHash128, FBufferView> BlockMap;  // Decompressed block data by hash
};

inline uint64
ComputeSize(const std::vector<FNeedBlock>& NeedBlocks)
{
	uint64 Result = 0;
	for (const FNeedBlock& It : NeedBlocks)
	{
		Result += It.Size;
	}
	return Result;
}

struct FNeedList
{
	std::vector<FNeedBlock> Source;
	std::vector<FNeedBlock> Base;
	std::vector<FHash128>	Sequence;
};

struct FPatchCommandList
{
	std::vector<FCopyCommand> Source;
	std::vector<FCopyCommand> Base;
	std::vector<FHash128>	  Sequence;
};

struct FNeedListSize
{
	uint64 SourceBytes = 0;
	uint64 BaseBytes   = 0;
	uint64 TotalBytes  = 0;
};

FNeedListSize ComputeNeedListSize(const FNeedList& NeedList);

const std::string& GetVersionString();

using FOnBlockGenerated = std::function<void(const FGenericBlock& Block, FBufferView Data)>;

struct FComputeBlocksParams
{
	bool			  bNeedBlocks = true;
	uint32			  BlockSize	  = uint32(64_KB);
	FAlgorithmOptions Algorithm;

	bool   bNeedMacroBlocks		= false;
	uint64 MacroBlockTargetSize = 3_MB;
	uint64 MacroBlockMaxSize	= 5_MB;	 // Maximum allowed by Jupiter

	// Callbacks may be called from worker threads
	FOnBlockGenerated OnBlockGenerated;
	FOnBlockGenerated OnMacroBlockGenerated;

	bool bAllowThreading = true;
};

struct FComputeBlocksResult
{
	FGenericBlockArray Blocks;
	FGenericBlockArray MacroBlocks;
};

FComputeBlocksResult ComputeBlocks(FIOReader& Reader, const FComputeBlocksParams& Params);
FComputeBlocksResult ComputeBlocks(const uint8* Data, uint64 Size, const FComputeBlocksParams& Params);
FComputeBlocksResult ComputeBlocksVariable(FIOReader& Reader, const FComputeBlocksParams& Params);

FGenericBlockArray ComputeBlocks(FIOReader& Reader, uint32 BlockSize, FAlgorithmOptions Algorithm);
FGenericBlockArray ComputeBlocks(const uint8* Data, uint64 Size, uint32 BlockSize, FAlgorithmOptions Algorithm);
FGenericBlockArray ComputeBlocksVariable(FIOReader&				Reader,
										 uint32					BlockSize,
										 EWeakHashAlgorithmID	WeakHasher,
										 EStrongHashAlgorithmID StrongHasher);


FNeedList DiffBlocks(const uint8*			   BaseData,
					 uint64					   BaseDataSize,
					 uint32					   BlockSize,
					 EWeakHashAlgorithmID	   WeakHasher,
					 EStrongHashAlgorithmID	   StrongHasher,
					 const FGenericBlockArray& SourceBlocks);

FNeedList DiffBlocksParallel(const uint8*			   BaseData,
							 uint64					   BaseDataSize,
							 uint32					   BlockSize,
							 EWeakHashAlgorithmID	   WeakHasher,
							 EStrongHashAlgorithmID	   StrongHasher,
							 const FGenericBlockArray& SourceBlocks,
							 uint64					   BytesPerTask);

FNeedList DiffBlocks(FIOReader&				   BaseDataReader,
					 uint32					   BlockSize,
					 EWeakHashAlgorithmID	   WeakHasher,
					 EStrongHashAlgorithmID	   StrongHasher,
					 const FGenericBlockArray& SourceBlocks);

FNeedList DiffBlocksParallel(FIOReader&				   BaseDataReader,
							 uint32					   BlockSize,
							 EWeakHashAlgorithmID	   WeakHasher,
							 EStrongHashAlgorithmID	   StrongHasher,
							 const FGenericBlockArray& SourceBlocks,
							 uint64					   BytesPerTask);

FNeedList DiffBlocksVariable(FIOReader&				   BaseDataReader,
							 uint32					   BlockSize,
							 EWeakHashAlgorithmID	   WeakHasher,
							 EStrongHashAlgorithmID	   StrongHasher,
							 const FGenericBlockArray& SourceBlocks);

FNeedList DiffManifestBlocks(const FGenericBlockArray& SourceBlocks, const FGenericBlockArray& BaseBlocks);

std::vector<FCopyCommand> OptimizeNeedList(const std::vector<FNeedBlock>& Input, uint64 MaxMergedBlockSize = 8_MB);


FBuffer GeneratePatch(const uint8*			 BaseData,
					  uint64				 BaseDataSize,
					  const uint8*			 SourceData,
					  uint64				 SourceDataSize,
					  uint32				 BlockSize,
					  EWeakHashAlgorithmID	 WeakHasher,
					  EStrongHashAlgorithmID StrongHasher,
					  int32					 CompressionLevel = 3);

bool IsSynchronized(const FNeedList& NeedList, const FGenericBlockArray& SourceBlocks);

enum class EFileSyncStatus
{
	Ok,
	ErrorUnknown,
	ErrorFullCopy,
	ErrorValidation,
	ErrorFinalRename,
	ErrorTargetFileCreate,
	ErrorBuildTargetFailed,
};

const wchar_t* ToString(EFileSyncStatus Status);

struct FFileSyncTask
{
	const FFileManifest* SourceManifest = nullptr;
	const FFileManifest* BaseManifest	= nullptr;
	FPath				 OriginalSourceFilePath;
	FPath				 ResolvedSourceFilePath;
	FPath				 BaseFilePath;
	FPath				 TargetFilePath;
	FPath				 RelativeFilePath;
	FNeedList			 NeedList;

	uint64 NeedBytesFromSource = 0;
	uint64 NeedBytesFromBase   = 0;
	uint64 TotalSizeBytes	   = 0;

	bool IsBaseValid() const { return !BaseFilePath.empty(); }
};

struct FFileSyncResult
{
	EFileSyncStatus Status			= EFileSyncStatus::ErrorUnknown;
	std::error_code SystemErrorCode = {};

	bool Succeeded() const { return (uint32)Status < (uint32)EFileSyncStatus::ErrorUnknown; }

	uint64 SourceBytes = 0;
	uint64 BaseBytes   = 0;
};

struct FSyncFileOptions
{
	FAlgorithmOptions Algorithm;
	uint32			  BlockSize = uint32(64_KB);

	FProxyPool* ProxyPool = nullptr;
	FBlockCache* BlockCache = nullptr;
	FScavengeDatabase* ScavengeDatabase = nullptr;

	bool bValidateTargetFiles = true;  // WARNING: turning this off is intended only for testing/profiling
};

FFileSyncResult SyncFile(const FPath&			   SourceFilePath,
						 const FGenericBlockArray& SourceBlocks,
						 FIOReader&				   BaseDataReader,
						 const FPath&			   TargetFilePath,
						 const FSyncFileOptions&   Options);

FFileSyncResult SyncFile(const FPath&			 SourceFilePath,
						 const FPath&			 BaseFilePath,
						 const FPath&			 TargetFilePath,
						 const FSyncFileOptions& Options);

struct FSyncFilter
{
	FSyncFilter() = default;

	// By default all files will be included, calling this will include only files containing these substrings
	void IncludeInSync(const std::wstring& CommaSeparatedWords);
	void ExcludeFromSync(const std::wstring& CommaSeparatedWords);
	void ExcludeFromCleanup(const std::wstring& CommaSeparatedWords);

	bool  ShouldSync(const FPath& Filename) const;
	bool  ShouldSync(const std::wstring& Filename) const;

	bool  ShouldCleanup(const FPath& Filename) const;
	bool  ShouldCleanup(const std::wstring& Filename) const;

	FPath Resolve(const FPath& Filename) const;

	std::vector<std::wstring> SyncIncludedWords;
	std::vector<std::wstring> SyncExcludedWords; // any paths that contain these words will not be synced
	std::vector<std::wstring> CleanupExcludedWords; // any paths that contain these words will not be deleted after sync

	std::vector<FDfsAlias>	  DfsAliases;
};

enum class ESyncSourceType
{
	Unknown,
	FileSystem,
	Server,
	ServerWithManifestHash,
};

struct FSyncDirectoryOptions
{
	ESyncSourceType	   SourceType;
	FPath			   Source;			   // remote data location
	FPath			   Target;			   // output target location
	FPath			   Base;			   // base data location, which typically is the same as sync target
	FPath			   ScavengeRoot;	   // base directory where we may want to find reusable blocks
	uint32			   ScavengeDepth = 5;  // how deep to look for unsync manifests
	std::vector<FPath> Overlays;		   // extra source directories to overlay over primary (add extra files, replace existing files)
	FPath			   SourceManifestOverride;	// force the manifest to be read from a specified file instead of source directory
	FSyncFilter*	   SyncFilter = nullptr;	// filter callback for partial sync support
	FProxyPool*		   ProxyPool  = nullptr;
	bool			   bCleanup	  = false;	// whether to cleanup any files in the target directory that are not in the source manifest file
	bool			   bValidateSourceFiles = true;	 // whether to check that all source files declared in the manifest are present/valid
	bool			   bValidateTargetFiles = true;	 // WARNING: turning this off is intended only for testing/profiling
	bool			   bFullDifference = true;	// whether to run full file difference algorithm, even when there is an existing manifest
	bool			   bCheckAvailableSpace		  = true;  // whether to abort the sync if target path does not have enough available space
	uint64			   BackgroundTaskMemoryBudget = 2_GB;
};

bool SyncDirectory(const FSyncDirectoryOptions& SyncOptions);

// #wip-widehash -- temporary helper functions
FBlock128			   ToBlock128(const FGenericBlock& GenericBlock);
std::vector<FBlock128> ToBlock128(FGenericBlockArray& GenericBlocks);

struct FCmdInfoOptions
{
	FPath InputA;
	FPath InputB;
	bool bListFiles = false;
	const FSyncFilter* SyncFilter = nullptr;
};
int32 CmdInfo(const FCmdInfoOptions& Options);

}  // namespace unsync
