// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncBuffer.h"
#include "UnsyncCommon.h"
#include "UnsyncHash.h"
#include "UnsyncProtocol.h"
#include "UnsyncSocket.h"
#include "UnsyncUtil.h"

#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace unsync {

extern bool GDryRun;

class FProxy;
class FProxyPool;
struct FRemoteDesc;
struct FIOReader;
struct FIOWriter;
struct FIOReaderWriter;
struct FComputeMacroBlockParams;

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
};

struct FNeedBlock : FCopyCommand
{
	FGenericHash Hash = {};
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

using FGenericBlockArray = std::vector<FGenericBlock>;

struct FComputeMacroBlockParams
{
	// Inputs
	uint64 TargetBlockSize = 3_MB;
	uint64 MaxBlockSize	   = 5_MB;	// Maximum allowed by Jupiter

	// Outputs
	FGenericBlockArray Output;
};

struct FFileManifest
{
	uint64 Mtime	 = 0;
	uint64 Size		 = 0;
	uint32 BlockSize = 0;

	// runtime-only data:
	FGenericBlockArray Blocks;
	FGenericBlockArray MacroBlocks;
	FPath			   CurrentPath;
};

struct FAlgorithmOptionsV5
{
	static constexpr uint64 MAGIC	= 0x96FF3B56283EC7DBull;
	static constexpr uint64 VERSION = 1;

	EChunkingAlgorithmID   ChunkingAlgorithmId	 = EChunkingAlgorithmID::VariableBlocks;
	EWeakHashAlgorithmID   WeakHashAlgorithmId	 = EWeakHashAlgorithmID::BuzHash;
	EStrongHashAlgorithmID StrongHashAlgorithmId = EStrongHashAlgorithmID::Blake3_128;
};

struct FAlgorithmOptions : FAlgorithmOptionsV5
{
};

struct FDirectoryManifest
{
	enum EVersions : uint64
	{
		Invalid = 0,
		V1_Unsupported,
		V2_Unsupported,
		V3_Unsupported,
		V4,					  // unsync v1.0.7
		V5,					  // unsync v1.0.8: added options
		V6_VariableHash,	  // unsync v1.0.32-iohash: added variable size hashes, moved options section before files
		V7_OptionalSections,  // unsync v1.0.32-iohash

		Latest = V7_OptionalSections
	};

	static constexpr uint64 MAGIC	= 0x80F4CC2A414F4E41ull;
	static constexpr uint64 VERSION = EVersions::Latest;

	// wide string at runtime, utf8 serialized
	// TODO: keep paths in canonical form (utf-8, unix-style separators)
	std::unordered_map<std::wstring, FFileManifest> Files;

	// runtime data
	FAlgorithmOptions Options = {};
	uint64			  Version = 0;

	bool IsValid() const
	{
		return Version != EVersions::Invalid;
	}
};

struct FDirectoryManifestInfo
{
	uint64	 TotalSize		= 0;
	uint64	 NumBlocks		= 0;
	uint64	 NumMacroBlocks = 0;
	uint64	 NumFiles		= 0;
	FHash256 SerializedHash = {};
	FHash256 Signature		= {};

	FAlgorithmOptions Algorithm = {};
};

FDirectoryManifestInfo GetManifestInfo(const FDirectoryManifest& Manifest);
void				   LogManifestInfo(ELogLevel LogLevel, const FDirectoryManifestInfo& Info);
void				   LogManifestFiles(ELogLevel LogLevel, const FDirectoryManifestInfo& Info);

const std::string& GetVersionString();

FGenericBlockArray ComputeBlocks(FIOReader&				   Reader,
								 uint32					   BlockSize,
								 FAlgorithmOptions		   Algorithm,
								 FComputeMacroBlockParams* OutMacroBlocks = nullptr);

FGenericBlockArray ComputeBlocks(const uint8*			   Data,
								 uint64					   Size,
								 uint32					   BlockSize,
								 FAlgorithmOptions		   Algorithm,
								 FComputeMacroBlockParams* OutMacroBlocks = nullptr);

FGenericBlockArray ComputeBlocksVariable(FIOReader&				   Reader,
										 uint32					   BlockSize,
										 EWeakHashAlgorithmID	   WeakHasher,
										 EStrongHashAlgorithmID	   StrongHasher,
										 FComputeMacroBlockParams* OutMacroBlocks = nullptr);

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

void BuildTarget(FIOWriter&				Result,
				 FIOReader&				Source,
				 FIOReader&				Base,
				 const FNeedList&		NeedList,
				 EStrongHashAlgorithmID StrongHasher,
				 FProxyPool*			ProxyPool = nullptr);

FBuffer BuildTargetBuffer(FIOReader&			 SourceProvider,
						  FIOReader&			 BaseProvider,
						  const FNeedList&		 NeedList,
						  EStrongHashAlgorithmID StrongHasher,
						  FProxyPool*			 ProxyPool = nullptr);

FBuffer BuildTargetBuffer(const uint8*			 SourceData,
						  uint64				 SourceSize,
						  const uint8*			 BaseData,
						  uint64				 BaseSize,
						  const FNeedList&		 NeedList,
						  EStrongHashAlgorithmID StrongHasher,
						  FProxyPool*			 ProxyPool = nullptr);

FBuffer BuildTargetWithPatch(const uint8* PatchData, uint64 PatchSize, const uint8* BaseData, uint64 BaseSize);

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
};

const wchar_t* ToString(EFileSyncStatus Status);

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
};

struct FSyncDirectoryOptions
{
	ESyncSourceType	   SourceType;
	FPath			   Source;					// remote data location
	FPath			   Target;					// output target location
	FPath			   Base;					// base data location, which typically is the same as sync target
	std::vector<FPath> Overlays;				// extra source directories to overlay over primary (add extra files, replace existing files)
	FPath			   SourceManifestOverride;	// force the manifest to be read from a specified file instead of source directory
	FSyncFilter*	   SyncFilter = nullptr;	// filter callback for partial sync support
	const FRemoteDesc* Remote	  = nullptr;	// unsync proxy server connection settings
	bool			   bCleanup	  = false;	// whether to cleanup any files in the target directory that are not in the source manifest file
	bool			   bValidateSourceFiles = true;	 // whether to check that all source files declared in the manifest are present/valid
	bool			   bValidateTargetFiles = true;	 // WARNING: turning this off is intended only for testing/profiling
	bool			   bFullDifference = true;	// whether to run full file difference algorithm, even when there is an existing manifest
};

bool SyncDirectory(const FSyncDirectoryOptions& SyncOptions);

void UpdateDirectoryManifestBlocks(FDirectoryManifest& Result, const FPath& Root, uint32 BlockSize, FAlgorithmOptions Algorithm);
FDirectoryManifest CreateDirectoryManifest(const FPath& Root, uint32 BlockSize, FAlgorithmOptions Algorithm);
FDirectoryManifest CreateDirectoryManifestIncremental(const FPath& Root, uint32 BlockSize, FAlgorithmOptions Algorithm);
bool LoadOrCreateDirectoryManifest(FDirectoryManifest& Result, const FPath& Root, uint32 BlockSize, FAlgorithmOptions Algorithm);

// Computes a Blake3 hash of the manifest blocks and file metadata, ignoring any other metadata.
// Files are processed in sorted order, with file names treated as utf-8.
// This produces a relatively stable manifest key that does not depend on the serialization differences between versions.
FHash256 ComputeManifestStableSignature(const FDirectoryManifest& Manifest);
FHash160 ComputeManifestStableSignature160(const FDirectoryManifest& Manifest);

// Computes a Blake3 hash of the serialized manifest data
FHash256 ComputeSerializedManifestHash(const FDirectoryManifest& Manifest);
FHash160 ComputeSerializedManifestHash160(const FDirectoryManifest& Manifest);

// #wip-widehash -- temporary helper functions
FBlock128			   ToBlock128(const FGenericBlock& GenericBlock);
std::vector<FBlock128> ToBlock128(FGenericBlockArray& GenericBlocks);

int32 CmdInfo(const FPath& InputA, const FPath& InputB, bool bListFiles);

}  // namespace unsync
