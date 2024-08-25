// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCommon.h"

#include "UnsyncCompression.h"
#include "UnsyncCore.h"
#include "UnsyncFile.h"
#include "UnsyncHashTable.h"
#include "UnsyncProgress.h"
#include "UnsyncProxy.h"
#include "UnsyncScan.h"
#include "UnsyncScavenger.h"
#include "UnsyncSerialization.h"
#include "UnsyncThread.h"
#include "UnsyncUtil.h"
#include "UnsyncTarget.h"

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

UNSYNC_THIRD_PARTY_INCLUDES_START
#include <blake3.h>
#include <md5-sse2.h>
UNSYNC_THIRD_PARTY_INCLUDES_END

#define UNSYNC_VERSION_STR "1.0.64"

namespace unsync {

bool GDryRun = false;

inline uint32
ComputeMinVariableBlockSize(uint32 BlockSize)
{
	return std::max(BlockSize, 4096u) / 2;	// changing this invalidates cached blocks
}

inline uint32
ComputeMaxVariableBlockSize(uint32 BlockSize)
{
	return std::max(BlockSize, 4096u) * 4;	// changing this invalidates cached blocks
}

static uint64
BlockingReadLarge(FIOReader& Reader, uint64 Offset, uint64 Size, uint8* OutputBuffer, uint64 OutputBufferSize)
{
	const uint64 BytesPerRead = 2_MB;
	const uint64 ReadEnd	  = std::min(Offset + Size, Reader.GetSize());
	const uint64 ClampedSize  = ReadEnd - Offset;

	std::atomic<uint64> TotalReadSize = 0;

	if (ClampedSize == 0)
	{
		return TotalReadSize;
	}

	FTaskGroup CopyTasks;
	FSemaphore IoSemaphore(MAX_ACTIVE_READERS);

	uint64 NumReads = DivUp(ClampedSize, BytesPerRead);
	for (uint64 ReadIndex = 0; ReadIndex < NumReads; ++ReadIndex)
	{
		const uint64 ThisBatchSize	= CalcChunkSize(ReadIndex, BytesPerRead, ClampedSize);
		const uint64 OutputOffset	= BytesPerRead * ReadIndex;
		const uint64 ThisReadOffset = Offset + OutputOffset;

		IoSemaphore.Acquire();

		auto ReadCallback = [OutputBuffer, OutputBufferSize, &TotalReadSize, &CopyTasks, &IoSemaphore](FIOBuffer CmdBuffer,
																									   uint64	 CmdSourceOffset,
																									   uint64	 CmdReadSize,
																									   uint64	 OutputOffset) {
			UNSYNC_ASSERT(OutputOffset + CmdReadSize <= OutputBufferSize);

			CopyTasks.run(
				[OutputBuffer, OutputOffset, CmdReadSize, CmdBuffer = MakeShared(std::move(CmdBuffer)), &TotalReadSize, &IoSemaphore]() {
					memcpy(OutputBuffer + OutputOffset, CmdBuffer->GetData(), CmdReadSize);
					TotalReadSize += CmdReadSize;
					IoSemaphore.Release();
				});
		};

		Reader.ReadAsync(ThisReadOffset, ThisBatchSize, OutputOffset, ReadCallback);
	}

	Reader.FlushAll();
	CopyTasks.wait();

	return TotalReadSize;
}

FBlock128
ToBlock128(const FGenericBlock& GenericBlock)
{
	FBlock128 Result;
	Result.HashStrong = GenericBlock.HashStrong.ToHash128();
	Result.HashWeak	  = GenericBlock.HashWeak;
	Result.Offset	  = GenericBlock.Offset;
	Result.Size		  = GenericBlock.Size;
	return Result;
}

std::vector<FBlock128>
ToBlock128(FGenericBlockArray& GenericBlocks)
{
	std::vector<FBlock128> Result;
	Result.reserve(GenericBlocks.size());

	for (const FGenericBlock& It : GenericBlocks)
	{
		Result.push_back(ToBlock128(It));
	}

	return Result;
}

template<typename WeakHasher>
FComputeBlocksResult
ComputeBlocksVariableT(FIOReader& Reader, const FComputeBlocksParams& Params)
{
	const uint32 BlockSize = Params.BlockSize;

	const uint64 InputSize = Reader.GetSize();

	const uint32 MinimumBlockSize = ComputeMinVariableBlockSize(BlockSize);
	const uint32 MaximumBlockSize = ComputeMaxVariableBlockSize(BlockSize);

	const uint64 BytesPerTask = std::min<uint64>(256_MB, std::max<uint64>(InputSize, 1ull));  // TODO: handle task boundary overlap
	const uint64 NumTasks	  = DivUp(InputSize, BytesPerTask);

	const uint64 TargetMacroBlockSize	 = Params.bNeedMacroBlocks ? Params.MacroBlockTargetSize : 0;
	const uint64 MinimumMacroBlockSize	 = std::max<uint64>(MinimumBlockSize, TargetMacroBlockSize / 8);
	const uint64 MaximumMacroBlockSize	 = Params.bNeedMacroBlocks ? Params.MacroBlockMaxSize : 0;
	const uint32 BlocksPerMacroBlock	 = CheckedNarrow(DivUp(TargetMacroBlockSize - MinimumMacroBlockSize, BlockSize));
	const uint32 MacroBlockHashThreshold = BlocksPerMacroBlock ? (0xFFFFFFFF / BlocksPerMacroBlock) : 0;

	struct FTask
	{
		uint64			   Offset = 0;
		FGenericBlockArray Blocks;
		FGenericBlockArray MacroBlocks;
	};

	std::vector<FTask> Tasks;
	Tasks.resize(NumTasks);

	FSemaphore IoSemaphore(MAX_ACTIVE_READERS);

	FTaskGroup TaskGroup;

	FBufferPool BufferPool(BytesPerTask);

	for (uint64 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
	{
		IoSemaphore.Acquire();

		const uint64 ThisTaskOffset = BytesPerTask * TaskIndex;
		const uint64 ThisTaskSize	= CalcChunkSize(TaskIndex, BytesPerTask, InputSize);

		Tasks[TaskIndex].Offset = ThisTaskOffset;

		FBuffer* ScanTaskBuffer = BufferPool.Acquire();
		UNSYNC_ASSERT(ScanTaskBuffer->Size() >= ThisTaskSize);

		uint64 ReadBytesForTask = BlockingReadLarge(Reader, ThisTaskOffset, ThisTaskSize, ScanTaskBuffer->Data(), ThisTaskSize);

		if (ReadBytesForTask != ThisTaskSize)
		{
			UNSYNC_FATAL(L"Expected to read %lld bytes from input, but %lld was actually read.", ThisTaskSize, ReadBytesForTask);
		}

		auto ScanTask = [&Tasks,
						 &IoSemaphore,
						 &BufferPool,
						 &Params,
						 MinimumBlockSize,
						 MaximumBlockSize,
						 ScanTaskBuffer,
						 TaskIndex,
						 ThisTaskSize,
						 TargetMacroBlockSize,
						 MinimumMacroBlockSize,
						 MaximumMacroBlockSize,
						 MacroBlockHashThreshold]() {
			FTask& Task = Tasks[TaskIndex];

			const uint8* DataBegin	  = ScanTaskBuffer->Data();
			const uint8* DataEnd	  = DataBegin + ThisTaskSize;
			const uint8* LastBlockEnd = DataBegin;

			FGenericBlock CurrentMacroBlock;
			CurrentMacroBlock.HashStrong.Type = EHashType::Blake3_160;
			CurrentMacroBlock.Offset		  = Task.Offset;

			blake3_hasher MacroBlockHasher;
			blake3_hasher_init(&MacroBlockHasher);

			auto ScanFn = [MacroBlockHashThreshold,
						   MaximumMacroBlockSize,
						   MaximumBlockSize,
						   MinimumMacroBlockSize,
						   DataEnd,
						   &LastBlockEnd,
						   &Task,
						   DataBegin,
						   &Params,
						   TargetMacroBlockSize,
						   &MacroBlockHasher,
						   &CurrentMacroBlock](const uint8* WindowBegin, const uint8* WindowEnd, uint32 WindowHash)
							  UNSYNC_ATTRIB_FORCEINLINE {
								  // WARNING: Changing this invalidates some of the previously cached blocks.
								  // TODO: compute based on target average block size
								  const uint32 ChunkWindowHashThreshold = 0x20000;

								  const bool   bLastBlock	 = WindowEnd == DataEnd;
								  const uint64 ThisBlockSize = uint64(WindowEnd - LastBlockEnd);

								  if (ThisBlockSize >= MaximumBlockSize || WindowHash < ChunkWindowHashThreshold || bLastBlock)
								  {
									  FGenericBlock Block;
									  Block.Offset	   = Task.Offset + uint64(LastBlockEnd - DataBegin);
									  Block.Size	   = CheckedNarrow(ThisBlockSize);
									  Block.HashWeak   = WindowHash;
									  Block.HashStrong = ComputeHash(LastBlockEnd, ThisBlockSize, Params.Algorithm.StrongHashAlgorithmId);

									  if (TargetMacroBlockSize)
									  {
										  blake3_hasher_update(&MacroBlockHasher, LastBlockEnd, ThisBlockSize);
										  CurrentMacroBlock.Size += Block.Size;

										  uint32 HashStrong32 = 0;
										  memcpy(&HashStrong32, Block.HashStrong.Data, 4);

										  if ((CurrentMacroBlock.Size >= MinimumMacroBlockSize && HashStrong32 < MacroBlockHashThreshold) ||
											  (CurrentMacroBlock.Size + Block.Size > MaximumMacroBlockSize) || bLastBlock)
										  {
											  // Commit the macro block
											  blake3_hasher_finalize(&MacroBlockHasher,
																	 CurrentMacroBlock.HashStrong.Data,
																	 sizeof(CurrentMacroBlock.HashStrong.Data));
											  Task.MacroBlocks.push_back(CurrentMacroBlock);

											  if (Params.OnMacroBlockGenerated)
											  {
												  FBufferView BlockView;
												  BlockView.Data = (LastBlockEnd + Block.Size) - CurrentMacroBlock.Size;
												  BlockView.Size = CurrentMacroBlock.Size;
												  Params.OnMacroBlockGenerated(CurrentMacroBlock, BlockView);
											  }

											  // Reset macro block state
											  blake3_hasher_init(&MacroBlockHasher);
											  CurrentMacroBlock.Offset += CurrentMacroBlock.Size;
											  CurrentMacroBlock.Size = 0;
										  }
									  }

									  if (Params.OnBlockGenerated)
									  {
										  FBufferView BlockView;
										  BlockView.Data = LastBlockEnd;
										  BlockView.Size = Block.Size;
										  Params.OnBlockGenerated(Block, BlockView);
									  }

									  if (!Task.Blocks.empty())
									  {
										  UNSYNC_ASSERT(Task.Blocks.back().Offset + Task.Blocks.back().Size == Block.Offset);
									  }

									  Task.Blocks.push_back(Block);

									  LastBlockEnd = WindowEnd;

									  return true;
								  }
								  else
								  {
									  return false;
								  }
							  };

			HashScan<WeakHasher>(DataBegin, ThisTaskSize, MinimumBlockSize, ScanFn);

			BufferPool.Release(ScanTaskBuffer);
			IoSemaphore.Release();
		};

		TaskGroup.run(ScanTask);
	}

	TaskGroup.wait();

	// Merge blocks for all the tasks

	FComputeBlocksResult Result;

	for (uint64 I = 0; I < NumTasks; ++I)
	{
		const FTask& Task = Tasks[I];
		for (uint64 J = 0; J < Task.Blocks.size(); ++J)
		{
			Result.Blocks.push_back(Task.Blocks[J]);
		}
	}

	if (Params.bNeedMacroBlocks)
	{
		for (uint64 I = 0; I < NumTasks; ++I)
		{
			const FTask& Task = Tasks[I];
			for (uint64 J = 0; J < Task.MacroBlocks.size(); ++J)
			{
				Result.MacroBlocks.push_back(Task.MacroBlocks[J]);
			}
		}
	}

	uint64 UniqueBlockTotalSize = 0;
	uint64 UniqueBlockMinSize	= ~0ull;
	uint64 UniqueBlockMaxSize	= 0ull;

	uint64 NumTinyBlocks   = 0;
	uint64 NumSmallBlocks  = 0;
	uint64 NumMediumBlocks = 0;
	uint64 NumLargeBlocks  = 0;

	uint64 NumTotalBlocks = 0;

	THashSet<FGenericHash> UniqueBlockSet;
	FGenericBlockArray	   UniqueBlocks;
	for (const FGenericBlock& It : Result.Blocks)
	{
		auto InsertResult = UniqueBlockSet.insert(It.HashStrong);
		if (InsertResult.second)
		{
			if (It.Offset + It.Size < InputSize || Result.Blocks.size() == 1)
			{
				UniqueBlockMinSize = std::min<uint64>(UniqueBlockMinSize, It.Size);
			}

			UniqueBlockMaxSize = std::max<uint64>(UniqueBlockMaxSize, It.Size);
			UniqueBlockTotalSize += It.Size;
			UniqueBlocks.push_back(It);
		}

		if (It.Size < MaximumBlockSize / 8)
		{
			NumTinyBlocks++;
		}
		else if (It.Size <= MaximumBlockSize / 4)
		{
			NumSmallBlocks++;
		}
		else if (It.Size <= MaximumBlockSize / 2)
		{
			NumMediumBlocks++;
		}
		else
		{
			NumLargeBlocks++;
		}

		++NumTotalBlocks;
	}

	double AverageBlockSize = InputSize ? double(UniqueBlockTotalSize / UniqueBlocks.size()) : 0;

	UNSYNC_VERBOSE2(
		L"Blocks (tiny/small/medium/large): %llu / %llu / %llu / %llu, average unique size: %llu bytes, unique count: %llu, total count: "
		L"%llu",
		NumTinyBlocks,
		NumSmallBlocks,
		NumMediumBlocks,
		NumLargeBlocks,
		(uint64)AverageBlockSize,
		(uint64)UniqueBlocks.size(),
		NumTotalBlocks);

	UNSYNC_ASSERT(NumTotalBlocks == Result.Blocks.size());

	return Result;
}

FComputeBlocksResult
ComputeBlocksVariable(FIOReader& Reader, const FComputeBlocksParams& Params)
{
	switch (Params.Algorithm.WeakHashAlgorithmId)
	{
		case EWeakHashAlgorithmID::Naive:
			return ComputeBlocksVariableT<FRollingChecksum>(Reader, Params);
		case EWeakHashAlgorithmID::BuzHash:
			return ComputeBlocksVariableT<FBuzHash>(Reader, Params);
		default:
			UNSYNC_FATAL(L"Unsupported weak hash algorithm mode");
			return {};
	}
}

FGenericBlockArray
ComputeBlocks(FIOReader& Reader, uint32 BlockSize, FAlgorithmOptions Algorithm)
{
	FComputeBlocksParams Params;
	Params.Algorithm			= Algorithm;
	Params.BlockSize			= BlockSize;
	FComputeBlocksResult Result = ComputeBlocks(Reader, Params);
	return std::move(Result.Blocks);
}

FGenericBlockArray
ComputeBlocks(const uint8* Data, uint64 Size, uint32 BlockSize, FAlgorithmOptions Algorithm)
{
	FComputeBlocksParams Params;
	Params.Algorithm			= Algorithm;
	Params.BlockSize			= BlockSize;
	FComputeBlocksResult Result = ComputeBlocks(Data, Size, Params);
	return std::move(Result.Blocks);
}

FGenericBlockArray
ComputeBlocksVariable(FIOReader& Reader, uint32 BlockSize, EWeakHashAlgorithmID WeakHasher, EStrongHashAlgorithmID StrongHasher)
{
	FComputeBlocksParams Params;
	Params.Algorithm.WeakHashAlgorithmId   = WeakHasher;
	Params.Algorithm.StrongHashAlgorithmId = StrongHasher;
	Params.BlockSize					   = BlockSize;
	FComputeBlocksResult Result			   = ComputeBlocks(Reader, Params);
	return std::move(Result.Blocks);
}

template<typename WeakHasher>
FComputeBlocksResult
ComputeBlocksFixedT(FIOReader& Reader, const FComputeBlocksParams& Params)
{
	UNSYNC_LOG_INDENT;

	auto TimeBegin = TimePointNow();

	const uint32 BlockSize = Params.BlockSize;
	const uint64 NumBlocks = DivUp(Reader.GetSize(), BlockSize);

	FGenericBlockArray Blocks(NumBlocks);
	for (uint64 I = 0; I < NumBlocks; ++I)
	{
		uint64 ChunkSize = CalcChunkSize(I, BlockSize, Reader.GetSize());
		Blocks[I].Offset = I * BlockSize;
		Blocks[I].Size	 = CheckedNarrow(ChunkSize);
	}

	uint64 ReadSize = std::max<uint64>(BlockSize, 8_MB);
	if (Params.bNeedMacroBlocks)
	{
		UNSYNC_FATAL(L"Macro block generation is not implemented for fixed block mode");
		ReadSize = std::max<uint64>(ReadSize, Params.MacroBlockTargetSize);
	}
	UNSYNC_ASSERT(ReadSize % BlockSize == 0);

	const uint64		NumReads		   = DivUp(Reader.GetSize(), ReadSize);
	std::atomic<uint64> NumReadsCompleted  = {};
	std::atomic<uint64> NumBlocksCompleted = {};

	{
		FSemaphore IoSemaphore(MAX_ACTIVE_READERS);
		FTaskGroup TaskGroup;

		for (uint64 I = 0; I < NumReads; ++I)
		{
			uint64 ThisReadSize = CalcChunkSize(I, ReadSize, Reader.GetSize());
			uint64 Offset		= I * ReadSize;

			IoSemaphore.Acquire();

			auto ReadCallback = [&NumReadsCompleted, &TaskGroup, &NumBlocksCompleted, &Blocks, &IoSemaphore, &Params, BlockSize](
									FIOBuffer CmdBuffer,
									uint64	  CmdOffset,
									uint64	  CmdReadSize,
									uint64	  CmdUserData) {
				UNSYNC_ASSERT(CmdReadSize);

				TaskGroup.run([&NumReadsCompleted,
							   &NumBlocksCompleted,
							   &Blocks,
							   &IoSemaphore,
							   &Params,
							   BlockSize,
							   CmdBuffer  = MakeShared(std::move(CmdBuffer)),
							   BufferSize = CmdReadSize,
							   Offset	  = CmdOffset]() {
					UNSYNC_ASSERT(CmdBuffer->GetSize() == BufferSize);

					uint8* Buffer = CmdBuffer->GetData();

					UNSYNC_ASSERT(Offset % BlockSize == 0);
					UNSYNC_ASSERT(BufferSize);
					UNSYNC_ASSERT(Buffer);

					uint64 FirstBlock	  = Offset / BlockSize;
					uint64 NumLocalBlocks = DivUp(BufferSize, BlockSize);
					for (uint64 I = 0; I < NumLocalBlocks; ++I)
					{
						FGenericBlock& Block = Blocks[FirstBlock + I];

						UNSYNC_ASSERT(Block.HashWeak == 0);
						UNSYNC_ASSERT(Block.HashStrong == FGenericHash{});

						Block.HashStrong = ComputeHash(Buffer + I * BlockSize, Block.Size, Params.Algorithm.StrongHashAlgorithmId);

						WeakHasher HashWeak;
						HashWeak.Update(Buffer + I * BlockSize, Block.Size);
						Block.HashWeak = HashWeak.Get();

						++NumBlocksCompleted;
					}

					++NumReadsCompleted;

					IoSemaphore.Release();
				});
			};

			Reader.ReadAsync(Offset, ThisReadSize, 0, ReadCallback);
		}
		Reader.FlushAll();
		TaskGroup.wait();
	}

	UNSYNC_ASSERT(NumReadsCompleted == NumReads);
	UNSYNC_ASSERT(NumBlocksCompleted == NumBlocks);

	md5_context Hasher;
	md5_init(&Hasher);
	for (uint64 I = 0; I < NumBlocks; ++I)
	{
		if (Blocks[I].HashStrong == FGenericHash{})
		{
			UNSYNC_ERROR(L"Found invalid hash in block %llu", I);
		}
		UNSYNC_ASSERT(Blocks[I].HashStrong != FGenericHash{});
		md5_update(&Hasher, Blocks[I].HashStrong.Data, sizeof(Blocks[I].HashStrong));
	}
	uint8 Hash[16] = {};
	md5_finish(&Hasher, Hash);
	std::string HashStr = BytesToHexString(Hash, sizeof(Hash));
	UNSYNC_VERBOSE2(L"Hash: %hs", HashStr.c_str());

	uint64 TocSize = sizeof(FBlock128) * NumBlocks;
	UNSYNC_VERBOSE2(L"Manifest size: %lld bytes (%.2f MB), blocks: %d", (long long)TocSize, SizeMb(TocSize), uint32(NumBlocks));

	double Duration = DurationSec(TimeBegin, TimePointNow());
	UNSYNC_VERBOSE2(L"Done in %.3f sec (%.3f MB / sec)", Duration, SizeMb((double(Reader.GetSize()) / Duration)));

	THashSet<uint32> UniqueHashes;
	for (const auto& It : Blocks)
	{
		UniqueHashes.insert(It.HashWeak);
	}

	FComputeBlocksResult Result;

	std::swap(Result.Blocks, Blocks);

	return Result;
}

FComputeBlocksResult
ComputeBlocksFixed(FIOReader& Reader, const FComputeBlocksParams& Params)
{
	switch (Params.Algorithm.WeakHashAlgorithmId)
	{
		case EWeakHashAlgorithmID::Naive:
			return ComputeBlocksFixedT<FRollingChecksum>(Reader, Params);
		case EWeakHashAlgorithmID::BuzHash:
			return ComputeBlocksFixedT<FBuzHash>(Reader, Params);
		default:
			UNSYNC_FATAL(L"Unsupported weak hash algorithm mode");
			return {};
	}
}

const std::string&
GetVersionString()
{
	static std::string Result = []() {
		// TODO: generate a version string based on git state
		const char* GitRev	  = "";
		const char* GitBranch = "";
		// const char* GIT_TAG = nullptr;

		static char Str[256];

		if (strlen(GitBranch) && strlen(GitRev))
		{
			snprintf(Str, sizeof(Str), UNSYNC_VERSION_STR " [%s:%s]", GitBranch, GitRev);
		}
		else if (strlen(GitRev))
		{
			snprintf(Str, sizeof(Str), UNSYNC_VERSION_STR " [%s]", GitRev);
		}
		else
		{
			snprintf(Str, sizeof(Str), UNSYNC_VERSION_STR);
		}

		return std::string(Str);
	}();

	return Result;
}

FComputeBlocksResult
ComputeBlocks(FIOReader& Reader, const FComputeBlocksParams& Params)
{
	switch (Params.Algorithm.ChunkingAlgorithmId)
	{
		case EChunkingAlgorithmID::FixedBlocks:
			return ComputeBlocksFixed(Reader, Params);
		case EChunkingAlgorithmID::VariableBlocks:
			return ComputeBlocksVariable(Reader, Params);
		default:
			UNSYNC_FATAL(L"Unsupported chunking mode");
			return {};
	}
}

FComputeBlocksResult
ComputeBlocks(const uint8* Data, uint64 Size, const FComputeBlocksParams& Params)
{
	FMemReader DataReader(Data, Size);
	return ComputeBlocks(DataReader, Params);
}

template<typename BlockType>
bool
ValidateBlockListT(const std::vector<BlockType>& Blocks)
{
	uint64 CurrentOffset = 0;
	for (const BlockType& Block : Blocks)
	{
		if (CurrentOffset != Block.Offset)
		{
			UNSYNC_ERROR(L"Found block at unexpected offset. Blocks are expected to be ordered by offset and contiguous.");
			return false;
		}

		CurrentOffset += Block.Size;
	}

	return true;
}

FNeedList
DiffBlocksVariable(FIOReader&				 BaseDataReader,
				   uint32					 BlockSize,
				   EWeakHashAlgorithmID		 WeakHasher,
				   EStrongHashAlgorithmID	 StrongHasher,
				   const FGenericBlockArray& SourceBlocks)
{
	FGenericBlockArray BaseBlocks = ComputeBlocksVariable(BaseDataReader, BlockSize, WeakHasher, StrongHasher);
	if (!ValidateBlockListT(BaseBlocks))
	{
		UNSYNC_FATAL(L"Base block list validation failed");
	}

	return DiffManifestBlocks(SourceBlocks, BaseBlocks);
}

inline FHash128
ToHash128(const FHash128& X)
{
	return X;
}

inline FHash128
ToHash128(const FGenericHash& X)
{
	return X.ToHash128();
}

template<typename BlockType>
FNeedList
DiffManifestBlocksT(const std::vector<BlockType>& SourceBlocks, const std::vector<BlockType>& BaseBlocks)
{
	FNeedList NeedList;

	struct BlockIndexAndCount
	{
		uint64 Index = 0;
		uint64 Count = 0;
	};

	THashMap<typename BlockType::StrongHashType, BlockIndexAndCount> BaseBlockMap;
	THashMap<uint64, uint64>										 BaseBlockByOffset;

	for (uint64 I = 0; I < BaseBlocks.size(); ++I)
	{
		const BlockType& Block			= BaseBlocks[I];
		BaseBlockByOffset[Block.Offset] = I;

		auto Existing = BaseBlockMap.find(Block.HashStrong);
		if (Existing == BaseBlockMap.end())
		{
			BlockIndexAndCount Item;
			Item.Index = I;
			Item.Count = 1;
			BaseBlockMap.insert(std::make_pair(Block.HashStrong, Item));
		}
		else
		{
			Existing->second.Count += 1;
		}
	}

	for (uint64 I = 0; I < SourceBlocks.size(); ++I)
	{
		const BlockType& SourceBlock = SourceBlocks[I];

		auto BaseBlockIt = BaseBlockMap.find(SourceBlock.HashStrong);
		if (BaseBlockIt == BaseBlockMap.end())
		{
			FNeedBlock NeedBlock;
			NeedBlock.Hash		   = SourceBlock.HashStrong;
			NeedBlock.Size		   = SourceBlock.Size;
			NeedBlock.SourceOffset = SourceBlock.Offset;
			NeedBlock.TargetOffset = SourceBlock.Offset;
			NeedList.Source.push_back(NeedBlock);
		}
		else
		{
			BlockIndexAndCount IndexAndCount = BaseBlockIt->second;
			const BlockType&   BaseBlock	 = BaseBlocks[IndexAndCount.Index];

			UNSYNC_ASSERT(BaseBlock.Size == SourceBlock.Size);

			FNeedBlock NeedBlock;
			NeedBlock.Hash		   = BaseBlock.HashStrong;
			NeedBlock.Size		   = BaseBlock.Size;
			NeedBlock.SourceOffset = BaseBlock.Offset;
			NeedBlock.TargetOffset = SourceBlock.Offset;

			const FNeedBlock* LastBaseNeedBlock = NeedList.Base.empty() ? nullptr : &(NeedList.Base.back());

			// Try to preserve contiguous base data reads
			if (LastBaseNeedBlock)
			{
				uint64 LastBlockEnd			  = LastBaseNeedBlock->SourceOffset + LastBaseNeedBlock->Size;
				auto   ConsecutiveBaseBlockIt = BaseBlockByOffset.find(LastBlockEnd);
				if (ConsecutiveBaseBlockIt != BaseBlockByOffset.end())
				{
					const BlockType& ConsecutiveBaseBlock = BaseBlocks[ConsecutiveBaseBlockIt->second];
					if (ConsecutiveBaseBlock.HashStrong == NeedBlock.Hash)
					{
						UNSYNC_ASSERT(NeedBlock.Size == ConsecutiveBaseBlock.Size);
						NeedBlock.SourceOffset = ConsecutiveBaseBlock.Offset;
					}
				}
			}

			NeedList.Base.push_back(NeedBlock);
		}

		NeedList.Sequence.push_back(ToHash128(SourceBlock.HashStrong));	 // #wip-widehash
	}

	return NeedList;
}

FNeedList
DiffManifestBlocks(const FGenericBlockArray& SourceBlocks, const FGenericBlockArray& BaseBlocks)
{
	return DiffManifestBlocksT(SourceBlocks, BaseBlocks);
}

template<typename WeakHasher>
FNeedList
DiffBlocksParallelT(FIOReader&				  BaseDataReader,
					uint32					  BlockSize,
					EStrongHashAlgorithmID	  StrongHasher,
					const FGenericBlockArray& SourceBlocks,
					uint64					  BytesPerTask)
{
	auto TimeBegin = TimePointNow();

	const uint64 BaseDataSize = BaseDataReader.GetSize();

	THashSet<uint32, FIdentityHash32>							  SourceWeakHashSet;
	THashSet<FGenericBlock, FBlockStrongHash, FBlockStrongHashEq> SourceStrongHashSet;

	for (uint32 I = 0; I < uint32(SourceBlocks.size()); ++I)
	{
		SourceWeakHashSet.insert(SourceBlocks[I].HashWeak);
		SourceStrongHashSet.insert(SourceBlocks[I]);
	}

	FNeedList NeedList;

	struct FTask
	{
		uint64														  Offset = 0;
		uint64														  Size	 = 0;
		std::vector<FHash128>										  Sequence;
		THashSet<FGenericBlock, FBlockStrongHash, FBlockStrongHashEq> BaseStrongHashSet;
	};

	BytesPerTask = std::max<uint64>(BlockSize, BytesPerTask);

	std::vector<FTask> Tasks;
	const uint64	   NumTasks = DivUp(BaseDataSize, BytesPerTask);
	Tasks.resize(NumTasks);

	FSemaphore IoSemaphore(MAX_ACTIVE_READERS);
	FTaskGroup TaskGroup;

	for (uint64 I = 0; I < NumTasks; ++I)
	{
		FTask& Task		 = Tasks[I];
		uint64 TaskBegin = I * BytesPerTask;
		uint64 TaskEnd	 = std::min(TaskBegin + BytesPerTask, BaseDataSize);

		Task.Offset = TaskBegin;
		Task.Size	= TaskEnd - TaskBegin;

		IoSemaphore.Acquire();

		auto ReadCallback =
			[&SourceStrongHashSet, &SourceWeakHashSet, &Tasks, &TaskGroup, &IoSemaphore, BaseDataSize, StrongHasher, BlockSize](
				FIOBuffer CmdBuffer,
				uint64	  CmdOffset,
				uint64	  CmdReadSize,
				uint64	  CmdUserData) {
				TaskGroup.run([&SourceStrongHashSet,
							   &SourceWeakHashSet,
							   &Tasks,
							   &IoSemaphore,
							   CmdBuffer = std::make_shared<FIOBuffer>(std::move(CmdBuffer)),
							   CmdReadSize,
							   CmdUserData,
							   BaseDataSize,
							   StrongHasher,
							   BlockSize]() {
					UNSYNC_ASSERT(CmdBuffer->GetSize() == CmdReadSize);

					uint8* TaskBuffer = CmdBuffer->GetData();
					uint64 TaskIndex  = CmdUserData;
					FTask& Task		  = Tasks[TaskIndex];

					UNSYNC_ASSERT(Task.Size == CmdReadSize);

					const uint8* TaskEnd = TaskBuffer + Task.Size;

					const uint32							  MaxWeakHashFalsePositives = 8;
					THashMap<uint32, uint32, FIdentityHash32> WeakHashFalsePositives;
					THashSet<uint32, FIdentityHash32>		  WeakHashBanList;

					auto ScanFn = [&SourceWeakHashSet,
								   &WeakHashBanList,
								   BlockSize,
								   &Task,
								   TaskBuffer,
								   StrongHasher,
								   &SourceStrongHashSet,
								   &WeakHashFalsePositives,
								   TaskEnd,
								   BaseDataSize](const uint8* WindowBegin, const uint8* WindowEnd, uint32 WindowHash) {
						uint64 ThisBlockSize = WindowEnd - WindowBegin;

						if (SourceWeakHashSet.find(WindowHash) != SourceWeakHashSet.end() &&
							WeakHashBanList.find(WindowHash) == WeakHashBanList.end())
						{
							UNSYNC_ASSERT(ThisBlockSize <= BlockSize);

							FGenericBlock BaseBlock;
							BaseBlock.Offset	 = Task.Offset + (WindowBegin - TaskBuffer);
							BaseBlock.Size		 = uint32(ThisBlockSize);
							BaseBlock.HashWeak	 = WindowHash;
							BaseBlock.HashStrong = ComputeHash(WindowBegin, ThisBlockSize, StrongHasher);

							auto SourceBlockIt = SourceStrongHashSet.find(BaseBlock);
							if (SourceBlockIt != SourceStrongHashSet.end())
							{
								const FGenericBlock& SourceBlock = *SourceBlockIt;

								Task.BaseStrongHashSet.insert(BaseBlock);
								Task.Sequence.push_back(SourceBlock.HashStrong.ToHash128());  // #wip-widehash

								return true;
							}

							uint32 FalsePositives = WeakHashFalsePositives[WindowHash]++;
							if (FalsePositives >= MaxWeakHashFalsePositives)
							{
								WeakHashBanList.insert(WindowHash);
							}
						}

						return WindowEnd == TaskEnd && (Task.Offset + Task.Size) != BaseDataSize;
					};

					HashScan<WeakHasher>(TaskBuffer, Task.Size, BlockSize, ScanFn);

					IoSemaphore.Release();
				});
			};

		BaseDataReader.ReadAsync(Task.Offset, Task.Size, I, ReadCallback);
	}

	BaseDataReader.FlushAll();
	TaskGroup.wait();

	THashSet<FGenericBlock, FBlockStrongHash, FBlockStrongHashEq> BaseStrongHashSet;

	for (FTask& Task : Tasks)
	{
		NeedList.Sequence.insert(NeedList.Sequence.end(), Task.Sequence.begin(), Task.Sequence.end());

		for (const FGenericBlock& Block : Task.BaseStrongHashSet)
		{
			BaseStrongHashSet.insert(Block);
		}
	}

	uint64 NeedBaseBytes   = 0;
	uint64 NeedSourceBytes = 0;

	for (const FGenericBlock& SourceBlock : SourceBlocks)
	{
		FNeedBlock NeedBlock;
		NeedBlock.Size		   = SourceBlock.Size;
		NeedBlock.TargetOffset = SourceBlock.Offset;
		NeedBlock.Hash		   = SourceBlock.HashStrong;

		auto BaseBlockIt = BaseStrongHashSet.find(SourceBlock);
		if (BaseBlockIt != BaseStrongHashSet.end())
		{
			NeedBlock.SourceOffset = BaseBlockIt->Offset;
			NeedList.Base.push_back(NeedBlock);
			NeedBaseBytes += BaseBlockIt->Size;
		}
		else
		{
			NeedBlock.SourceOffset = SourceBlock.Offset;
			NeedList.Source.push_back(NeedBlock);
			NeedSourceBytes += SourceBlock.Size;
		}
	}

	double Duration = DurationSec(TimeBegin, TimePointNow());
	UNSYNC_VERBOSE(L"Done in %.3f sec (%.3f MB / sec)", Duration, SizeMb(double(BaseDataSize) / Duration));

	return NeedList;
}

FNeedList
DiffBlocksParallel(FIOReader&				 BaseDataReader,
				   uint32					 BlockSize,
				   EWeakHashAlgorithmID		 WeakHasher,
				   EStrongHashAlgorithmID	 StrongHasher,
				   const FGenericBlockArray& SourceBlocks,
				   uint64					 BytesPerTask)
{
	switch (WeakHasher)
	{
		case EWeakHashAlgorithmID::Naive:
			return DiffBlocksParallelT<FRollingChecksum>(BaseDataReader, BlockSize, StrongHasher, SourceBlocks, BytesPerTask);
		case EWeakHashAlgorithmID::BuzHash:
			return DiffBlocksParallelT<FBuzHash>(BaseDataReader, BlockSize, StrongHasher, SourceBlocks, BytesPerTask);
		default:
			UNSYNC_FATAL(L"Unexpected weak hash algorithm id");
			return {};
	}
}

FNeedList
DiffBlocks(FIOReader&				 BaseDataReader,
		   uint32					 BlockSize,
		   EWeakHashAlgorithmID		 WeakHasher,
		   EStrongHashAlgorithmID	 StrongHasher,
		   const FGenericBlockArray& SourceBlocks)
{
	const uint64 BytesPerTask = 32_MB;	// <-- reasonably OK balance between accuracy and speed
	// const uint64 bytes_per_task = base_data_size; // <-- run single-threaded
	return DiffBlocksParallel(BaseDataReader, BlockSize, WeakHasher, StrongHasher, SourceBlocks, BytesPerTask);
}

FNeedList
DiffBlocks(const uint8*				 BaseData,
		   uint64					 BaseDataSize,
		   uint32					 BlockSize,
		   EWeakHashAlgorithmID		 WeakHasher,
		   EStrongHashAlgorithmID	 StrongHasher,
		   const FGenericBlockArray& SourceBlocks)
{
	FMemReader BaseReader(BaseData, BaseDataSize);
	return DiffBlocks(BaseReader, BlockSize, WeakHasher, StrongHasher, SourceBlocks);
}

FNeedList
DiffBlocksParallel(const uint8*				 BaseData,
				   uint64					 BaseDataSize,
				   uint32					 BlockSize,
				   EWeakHashAlgorithmID		 WeakHasher,
				   EStrongHashAlgorithmID	 StrongHasher,
				   const FGenericBlockArray& SourceBlocks,
				   uint64					 BytesPerTask)
{
	FMemReader BaseReader(BaseData, BaseDataSize);
	return DiffBlocksParallel(BaseReader, BlockSize, WeakHasher, StrongHasher, SourceBlocks, BytesPerTask);
}

std::vector<FCopyCommand>
OptimizeNeedList(const std::vector<FNeedBlock>& Input, uint64 MaxMergedBlockSize)
{
	std::vector<FCopyCommand> Result;
	Result.reserve(Input.size());
	for (const FNeedBlock& Block : Input)
	{
		FCopyCommand Cmd;
		Cmd.SourceOffset = Block.SourceOffset;
		Cmd.TargetOffset = Block.TargetOffset;
		Cmd.Size		 = Block.Size;
		Result.push_back(Cmd);
	}

	std::sort(Result.begin(), Result.end(), FCopyCommand::FCompareBySourceOffset());

	for (uint64 I = 1; I < Result.size(); ++I)
	{
		FCopyCommand& PrevBlock = Result[I - 1];
		FCopyCommand& ThisBlock = Result[I];
		if (PrevBlock.SourceOffset + PrevBlock.Size == ThisBlock.SourceOffset &&
			PrevBlock.TargetOffset + PrevBlock.Size == ThisBlock.TargetOffset && PrevBlock.Size + ThisBlock.Size <= MaxMergedBlockSize)
		{
			ThisBlock.SourceOffset = PrevBlock.SourceOffset;
			ThisBlock.TargetOffset = PrevBlock.TargetOffset;
			ThisBlock.Size += PrevBlock.Size;
			UNSYNC_ASSERT(ThisBlock.Size <= MaxMergedBlockSize);
			PrevBlock.Size = 0;
		}
	}

	for (uint64 I = 0; I < Result.size(); ++I)
	{
		UNSYNC_ASSERT(Result[I].Size <= MaxMergedBlockSize);
	}

	auto It = std::remove_if(Result.begin(), Result.end(), [](const FCopyCommand& Block) { return Block.Size == 0; });

	Result.erase(It, Result.end());

	return Result;
}

FReadSchedule
BuildReadSchedule(const std::vector<FNeedBlock>& Blocks)
{
	FReadSchedule Result;
	Result.Blocks = OptimizeNeedList(Blocks);
	std::sort(Result.Blocks.begin(), Result.Blocks.end(), [](const FCopyCommand& A, const FCopyCommand& B) {
		if (A.Size == B.Size)
		{
			return A.SourceOffset < B.SourceOffset;
		}
		else
		{
			return A.Size < B.Size;
		}
	});

	for (uint64 I = 0; I < Result.Blocks.size(); ++I)
	{
		Result.Requests.push_back(I);
	}

	return Result;
}

bool
IsSynchronized(const FNeedList& NeedList, const FGenericBlockArray& SourceBlocks)
{
	if (NeedList.Source.size() != 0)
	{
		return false;
	}

	if (NeedList.Base.size() != SourceBlocks.size())
	{
		return false;
	}

	if (NeedList.Sequence.size() != SourceBlocks.size())
	{
		return false;
	}

	for (uint64 I = 0; I < SourceBlocks.size(); ++I)
	{
		if (NeedList.Sequence[I] != SourceBlocks[I].HashStrong.ToHash128())	 // #wip-widehash
		{
			return false;
		}
	}

	return true;
}

bool
ValidateTarget(FIOReader& Reader, const FNeedList& NeedList, EStrongHashAlgorithmID StrongHasher)
{
	FGenericBlockArray ValidationBlocks;
	for (const FNeedBlock& It : NeedList.Source)
	{
		FGenericBlock Block;
		Block.Size		 = CheckedNarrow(It.Size);
		Block.Offset	 = It.TargetOffset;
		Block.HashStrong = It.Hash;
		ValidationBlocks.push_back(Block);
	}
	for (const FNeedBlock& It : NeedList.Base)
	{
		FGenericBlock Block;
		Block.Size		 = CheckedNarrow(It.Size);
		Block.Offset	 = It.TargetOffset;
		Block.HashStrong = It.Hash;
		ValidationBlocks.push_back(Block);
	}

	std::sort(ValidationBlocks.begin(), ValidationBlocks.end(), [](const FGenericBlock& A, const FGenericBlock& B) {
		return A.Offset < B.Offset;
	});

	FSemaphore IoSemaphore(MAX_ACTIVE_READERS);

	const uint64		TotalStreamBytes = Reader.GetSize();
	std::atomic<uint64> NumInvalidBlocks = {};
	FTaskGroup			TaskGroup;

	FLogProgressScope ValidationProgressLogger(TotalStreamBytes, ELogProgressUnits::MB);

	// Inherit verbosity and indentation from parent theread
	// TODO: make a helper that sets verbosity and indentation automatically
	const bool	 bLogVerbose = GLogVerbose;
	const uint32 LogIndent	 = GLogIndent;

	uint64 MaxBatchSizeBytes = 8_MB;

	uint64 BatchBegin	  = 0;
	uint64 BatchSizeBytes = 0;

	for (uint64 BlockIndex = 0; BlockIndex < ValidationBlocks.size(); ++BlockIndex)
	{
		const FGenericBlock& CurrBlock = ValidationBlocks[BlockIndex];

		if (BlockIndex > 0)
		{
			const FGenericBlock& PrevBlock = ValidationBlocks[BlockIndex - 1];
			if (PrevBlock.Offset + PrevBlock.Size != CurrBlock.Offset)
			{
				UNSYNC_ERROR(L"Found block at unexpected offset");
				return false;
			}
		}

		BatchSizeBytes += CurrBlock.Size;

		if (BlockIndex + 1 < ValidationBlocks.size() && BatchSizeBytes + ValidationBlocks[BlockIndex + 1].Size < MaxBatchSizeBytes)
		{
			continue;
		}

		UNSYNC_ASSERT(BatchSizeBytes <= MaxBatchSizeBytes || BatchBegin == BlockIndex);

		IoSemaphore.Acquire();

		const uint64 ReadOffset = ValidationBlocks[BatchBegin].Offset;
		UNSYNC_ASSERT(BlockIndex + 1 == ValidationBlocks.size() ||
					  (ReadOffset + BatchSizeBytes) == ValidationBlocks[BlockIndex + 1].Offset);

		auto ReadCallback = [StrongHasher,
							 bLogVerbose,
							 LogIndent,
							 BatchBegin,
							 BatchEnd = BlockIndex + 1,
							 BatchSizeBytes,
							 &NumInvalidBlocks,
							 &TaskGroup,
							 &IoSemaphore,
							 &ValidationProgressLogger,
							 &ValidationBlocks](FIOBuffer CmdBuffer, uint64 CmdSourceOffset, uint64 CmdReadSize, uint64 CmdUserData) {
			if (CmdReadSize != BatchSizeBytes)
			{
				UNSYNC_ERROR(L"Expected to read %lld bytes, but read %lld", BatchSizeBytes, CmdReadSize);
				NumInvalidBlocks++;
				return;
			}

			TaskGroup.run([CmdBuffer = std::make_shared<FIOBuffer>(std::move(CmdBuffer)),
						   BatchBegin,
						   BatchEnd,
						   StrongHasher,
						   bLogVerbose,
						   LogIndent,
						   &NumInvalidBlocks,
						   &IoSemaphore,
						   &ValidationProgressLogger,
						   &ValidationBlocks]() {
				FLogIndentScope	   IndentScope(LogIndent, true);
				FLogVerbosityScope VerbosityScope(bLogVerbose);

				const uint64 FirstBlockOffset = ValidationBlocks[BatchBegin].Offset;
				for (uint64 I = BatchBegin; I < BatchEnd; ++I)
				{
					const FGenericBlock& Block			   = ValidationBlocks[I];
					const uint64		 BlockBufferOffset = Block.Offset - FirstBlockOffset;
					FGenericHash		 Hash = ComputeHash(CmdBuffer->GetData() + BlockBufferOffset, Block.Size, StrongHasher);
					if (Hash != Block.HashStrong)
					{
						UNSYNC_ERROR(L"Found block hash mismatch at offset %llu", llu(BlockBufferOffset));
						NumInvalidBlocks++;
						return;
					}

					ValidationProgressLogger.Add(Block.Size);
				}

				IoSemaphore.Release();
			});
		};

		Reader.ReadAsync(ReadOffset, BatchSizeBytes, 0, ReadCallback);

		if (NumInvalidBlocks)
		{
			break;
		}

		BatchSizeBytes = 0;
		BatchBegin	   = BlockIndex + 1;
	}

	Reader.FlushAll();
	TaskGroup.wait();

	ValidationProgressLogger.Complete();

	return NumInvalidBlocks == 0;
}

FFileSyncResult
SyncFile(const FNeedList&		   NeedList,
		 const FPath&			   SourceFilePath,
		 const FGenericBlockArray& SourceBlocks,
		 FIOReader&				   BaseDataReader,
		 const FPath&			   TargetFilePath,
		 const FSyncFileOptions&   Options)
{
	UNSYNC_LOG_INDENT;

	FFileSyncResult Result;

	uint64 NeedFromSource = ComputeSize(NeedList.Source);
	uint64 NeedFromBase	  = ComputeSize(NeedList.Base);
	UNSYNC_VERBOSE(L"Need from source %.2f MB, from base: %.2f MB", SizeMb(NeedFromSource), SizeMb(NeedFromBase));

	const FFileAttributes TargetFileAttributes = GetFileAttrib(TargetFilePath);

	if (!TargetFileAttributes.bValid && NeedList.Sequence.empty())
	{
		UNSYNC_VERBOSE(L"Creating empty file '%ls'", TargetFilePath.wstring().c_str());

		if (GDryRun)
		{
			Result.Status = EFileSyncStatus::Ok;
		}
		else
		{
			FPath TargetFileParent = TargetFilePath.parent_path();
			if (!PathExists(TargetFileParent))
			{
				CreateDirectories(TargetFileParent);
			}

			auto TargetFile = FNativeFile(TargetFilePath, EFileMode::CreateWriteOnly, 0);
			if (TargetFile.IsValid())
			{
				Result.Status = EFileSyncStatus::Ok;
			}
			else
			{
				Result.Status		   = EFileSyncStatus::ErrorTargetFileCreate;
				Result.SystemErrorCode = std::error_code(TargetFile.GetError(), std::system_category());
			}
		}
	}
	else if (!IsSynchronized(NeedList, SourceBlocks))
	{
		LogStatus(TargetFilePath.wstring().c_str(), L"Initializing");

		FPath TempTargetFilePath = TargetFilePath;
		TempTargetFilePath.replace_extension(TargetFilePath.extension().wstring() + L".tmp");
		const FNeedListSize TargetFileSizeInfo = ComputeNeedListSize(NeedList);

		FBuffer							 TargetFileBuffer;
		std::unique_ptr<FIOReaderWriter> TargetFile;
		if (GDryRun)
		{
			if (Options.bValidateTargetFiles)
			{
				TargetFileBuffer.Resize(TargetFileSizeInfo.TotalBytes);
				TargetFile = std::make_unique<FMemReaderWriter>(TargetFileBuffer.Data(), TargetFileBuffer.Size());
			}
			else
			{
				TargetFile = std::make_unique<FNullReaderWriter>(TargetFileSizeInfo.TotalBytes);
			}
		}
		else
		{
			FPath TargetFileParent = TempTargetFilePath.parent_path();
			if (!PathExists(TargetFileParent))
			{
				CreateDirectories(TargetFileParent);
			}

			TargetFile = std::make_unique<FNativeFile>(TempTargetFilePath, EFileMode::CreateWriteOnly, TargetFileSizeInfo.TotalBytes);
			if (TargetFile->GetError() != 0)
			{
				UNSYNC_FATAL(L"Failed to create output file '%ls'. %hs",
							 TempTargetFilePath.wstring().c_str(),
							 FormatSystemErrorMessage(TargetFile->GetError()).c_str());
			}
		}

		LogStatus(TargetFilePath.wstring().c_str(), L"Patching");

		FDeferredOpenReader SourceFile([SourceFilePath, TargetFilePath] {
			UNSYNC_VERBOSE(L"Opening source file '%ls'", SourceFilePath.wstring().c_str());
			LogStatus(TargetFilePath.wstring().c_str(), L"Opening source file");
			return std::unique_ptr<FNativeFile>(new FNativeFile(SourceFilePath, EFileMode::ReadOnlyUnbuffered));
		});

		FBuildTargetParams BuildParams;
		BuildParams.StrongHasher	 = Options.Algorithm.StrongHashAlgorithmId;
		BuildParams.ProxyPool		 = Options.ProxyPool;
		BuildParams.BlockCache		 = Options.BlockCache;
		BuildParams.ScavengeDatabase = Options.ScavengeDatabase;

		FBuildTargetResult BuildResult = BuildTarget(*TargetFile, SourceFile, BaseDataReader, NeedList, BuildParams);

		Result.SourceBytes = BuildResult.SourceBytes;
		Result.BaseBytes   = BuildResult.BaseBytes;

		if (!BuildResult.bSuccess)
		{
			Result.Status = EFileSyncStatus::ErrorBuildTargetFailed;
			return Result;
		}

		if (Options.bValidateTargetFiles)
		{
			LogStatus(TargetFilePath.wstring().c_str(), L"Verifying");
			UNSYNC_VERBOSE(L"Verifying patched file '%ls'", TargetFilePath.wstring().c_str());
			UNSYNC_LOG_INDENT;

			if (!GDryRun)
			{
				// Reopen the file in unuffered read mode for optimal reading performance
				TargetFile = nullptr;
				TargetFile = std::make_unique<FNativeFile>(TempTargetFilePath, EFileMode::ReadOnlyUnbuffered);
			}

			if (!ValidateTarget(*TargetFile, NeedList, Options.Algorithm.StrongHashAlgorithmId))
			{
				Result.Status = EFileSyncStatus::ErrorValidation;
				return Result;
			}
		}

		if (GDryRun)
		{
			Result.Status = EFileSyncStatus::Ok;
		}
		else
		{
			LogStatus(TargetFilePath.wstring().c_str(), L"Finalizing");
			UNSYNC_VERBOSE(L"Finalizing target file '%ls'", TargetFilePath.wstring().c_str());
			BaseDataReader.Close();
			if (TargetFile)
			{
				TargetFile->Close();
			}

			if (GetFileAttrib(TargetFilePath).bReadOnly)
			{
				UNSYNC_VERBOSE(L"Clearing read-only flag from target file '%ls'", TargetFilePath.wstring().c_str());
				bool bClearReadOnlyOk = SetFileReadOnly(TargetFilePath, false);
				if (!bClearReadOnlyOk)
				{
					UNSYNC_ERROR(L"Failed to clear read-only flag from '%ls'", TargetFilePath.wstring().c_str());
				}
			}

			std::error_code ErrorCode = {};
			FileRename(TempTargetFilePath, TargetFilePath, ErrorCode);

			if (ErrorCode.value() == 0)
			{
				Result.Status = EFileSyncStatus::Ok;
			}
			else
			{
				Result.Status		   = EFileSyncStatus::ErrorFinalRename;
				Result.SystemErrorCode = ErrorCode;
			}
		}

		const uint64 ExpectedSourceBytes = ComputeSize(NeedList.Source);
		const uint64 ExpectedBaseBytes	 = ComputeSize(NeedList.Base);

		const uint64 ActualProcessedBytes	= BuildResult.SourceBytes + BuildResult.BaseBytes;
		const uint64 ExpectedProcessedBytes = ExpectedSourceBytes + ExpectedBaseBytes;

		if (ActualProcessedBytes != ExpectedProcessedBytes)
		{
			Result.Status = EFileSyncStatus::ErrorValidation;
			UNSYNC_ERROR(L"Failed to patch file '%ls'. Expected to write %llu bytes, but actually wrote %llu bytes.",
						 TargetFilePath.wstring().c_str(),
						 llu(ExpectedProcessedBytes),
						 llu(ActualProcessedBytes));
		}
	}
	else
	{
		UNSYNC_VERBOSE(L"Target file '%ls' already synchronized", TargetFilePath.wstring().c_str());
		Result.Status	 = EFileSyncStatus::Ok;
		Result.BaseBytes = NeedFromBase;
	}

	return Result;
}

FFileSyncResult
SyncFile(const FPath&			   SourceFilePath,
		 const FGenericBlockArray& SourceBlocks,
		 FIOReader&				   BaseDataReader,
		 const FPath&			   TargetFilePath,
		 const FSyncFileOptions&   Options)
{
	UNSYNC_LOG_INDENT;
	UNSYNC_VERBOSE(L"Computing difference for target '%ls' (base size: %.2f MB)",
				   TargetFilePath.wstring().c_str(),
				   SizeMb(BaseDataReader.GetSize()));
	FNeedList NeedList = DiffBlocks(BaseDataReader,
									Options.BlockSize,
									Options.Algorithm.WeakHashAlgorithmId,
									Options.Algorithm.StrongHashAlgorithmId,
									SourceBlocks);
	return SyncFile(NeedList, SourceFilePath, SourceBlocks, BaseDataReader, TargetFilePath, Options);
}

FFileSyncResult
SyncFile(const FPath& SourceFilePath, const FPath& BaseFilePath, const FPath& TargetFilePath, const FSyncFileOptions& InOptions)
{
	UNSYNC_LOG_INDENT;

	FSyncFileOptions Options = InOptions;  // This may be modified by LoadBlocks()

	FFileSyncResult Result;

	FNativeFile BaseFile(BaseFilePath, EFileMode::ReadOnlyUnbuffered);
	if (!BaseFile.IsValid())
	{
		BaseFile.Close();

		UNSYNC_VERBOSE(L"Full copy required for '%ls' (base does not exist)", BaseFilePath.wstring().c_str());
		std::error_code ErrorCode;
		bool			bCopyOk = FileCopy(SourceFilePath, TargetFilePath, ErrorCode);
		if (bCopyOk)
		{
			Result.Status = EFileSyncStatus::Ok;
		}
		else
		{
			Result.Status		   = EFileSyncStatus::ErrorFullCopy;
			Result.SystemErrorCode = ErrorCode;
		}
		Result.SourceBytes = GetFileAttrib(SourceFilePath).Size;
		return Result;
	}

	FGenericBlockArray SourceBlocks;
	FPath			   BlockFilename = BaseFilePath.wstring() + std::wstring(L".unsync");

	UNSYNC_VERBOSE(L"Loading block manifest from '%ls'", BlockFilename.wstring().c_str());
	if (LoadBlocks(SourceBlocks, Options.BlockSize, BlockFilename.c_str()))
	{
		UNSYNC_VERBOSE(L"Loaded blocks: %d", uint32(SourceBlocks.size()));
	}
	else
	{
		UNSYNC_VERBOSE(L"Full copy required (manifest file does not exist or is invalid)");

		std::error_code ErrorCode;
		bool			bCopyOk = FileCopy(SourceFilePath, TargetFilePath, ErrorCode);
		if (bCopyOk)
		{
			Result.Status = EFileSyncStatus::Ok;
		}
		else
		{
			Result.Status		   = EFileSyncStatus::ErrorFullCopy;
			Result.SystemErrorCode = ErrorCode;
		}

		Result.SourceBytes = GetFileAttrib(SourceFilePath).Size;

		return Result;
	}

	return SyncFile(SourceFilePath, SourceBlocks, BaseFile, TargetFilePath, Options);
}

std::error_code
CopyFileIfNewer(const FPath& Source, const FPath& Target)
{
	FFileAttributes SourceAttr = GetFileAttrib(Source);
	FFileAttributes TargetAttr = GetFileAttrib(Target);
	std::error_code Ec;
	if (SourceAttr.Size != TargetAttr.Size || SourceAttr.Mtime != TargetAttr.Mtime)
	{
		FileCopyOverwrite(Source, Target, Ec);
	}
	return Ec;
}

static bool
IsNonCaseSensitiveFileSystem(const FPath& ExistingPath)
{
	UNSYNC_ASSERTF(PathExists(ExistingPath), L"IsCaseSensitiveFileSystem must be called with a path that exists on disk");

	// Assume file system is case-sensitive if all-upper and all-lower versions of the path exist and resolve to the same FS entry.
	// This is not 100% robust due to symlinks, but is good enough for most practical purposes.

	FPath PathUpper = StringToUpper(ExistingPath.wstring());
	FPath PathLower = StringToLower(ExistingPath.wstring());

	if (PathExists(PathUpper) && PathExists(PathLower))
	{
		return std::filesystem::equivalent(ExistingPath, PathUpper) && std::filesystem::equivalent(PathLower, PathUpper);
	}
	else
	{
		return false;
	}
}

static bool
IsCaseSensitiveFileSystem(const FPath& ExistingPath)
{
	return !IsNonCaseSensitiveFileSystem(ExistingPath);
}

struct FPendingFileRename
{
	std::wstring Old;
	std::wstring New;
};

// Updates the target directory manifest filename case to be consistent with reference.
// Internally we always perform case-sensitive path comparisons, however on non-case-sensitive filesystems some local files may be renamed
// to a mismatching case. We can update the locally-generated manifest to take the case from the reference manifest for equivalent paths.
// Returns a list of files that should be renamed on disk.
static std::vector<FPendingFileRename>
FixManifestFileNameCases(FDirectoryManifest& TargetDirectoryManifest, const FDirectoryManifest& ReferenceManifest)
{
	// Build a lookup table of lowercase -> original file names and detect potential case conflicts (which will explode on Windows and Mac)

	std::unordered_map<std::wstring, std::wstring> ReferenceFileNamesLowerCase;
	bool										   bFoundCaseConflicts = false;
	for (auto& ReferenceManifestEntry : ReferenceManifest.Files)
	{
		std::wstring FileNameLowerCase = StringToLower(ReferenceManifestEntry.first);
		auto		 InsertResult =
			ReferenceFileNamesLowerCase.insert(std::pair<std::wstring, std::wstring>(FileNameLowerCase, ReferenceManifestEntry.first));

		if (!InsertResult.second)
		{
			UNSYNC_WARNING(L"Found file name case conflict: '%ls'", ReferenceManifestEntry.first.c_str());
			bFoundCaseConflicts = true;
		}
	}

	if (bFoundCaseConflicts)
	{
		UNSYNC_WARNING(L"File name case conflicts will result in issues on case-insensitive systems, such as Windows and macOS.");
	}

	// Find inconsistently-cased files and add them to a list to be fixed up

	std::vector<FPendingFileRename> FixupEntries;

	for (auto& TargetManifestEntry : TargetDirectoryManifest.Files)
	{
		const std::wstring& TargetFileName = TargetManifestEntry.first;
		if (ReferenceManifest.Files.find(TargetFileName) == ReferenceManifest.Files.end())
		{
			std::wstring TargetFileNameLowerCase = StringToLower(TargetFileName);
			auto		 ReferenceIt			 = ReferenceFileNamesLowerCase.find(TargetFileNameLowerCase);
			if (ReferenceIt != ReferenceFileNamesLowerCase.end())
			{
				FixupEntries.push_back({TargetFileName, ReferenceIt->second});
			}
		}
	}

	// Re-add file manifests under the correct names

	for (const FPendingFileRename& Entry : FixupEntries)
	{
		auto It = TargetDirectoryManifest.Files.find(Entry.Old);
		UNSYNC_ASSERT(It != TargetDirectoryManifest.Files.end());

		FFileManifest Manifest;
		std::swap(It->second, Manifest);
		TargetDirectoryManifest.Files.erase(Entry.Old);
		TargetDirectoryManifest.Files.insert(std::pair(Entry.New, std::move(Manifest)));
	}

	return FixupEntries;
}

// Takes a list of file names that require case fixup and performs the necessary renaming.
// Handles renaming of intermediate directories as well as the leaf files.
// Quite wasteful in terms of mallocs, but doesn't matter since we're about to touch the file system anyway.
static bool
FixFileNameCases(const FPath& RootPath, const std::vector<FPendingFileRename>& PendingRenames)
{
	std::vector<FPendingFileRename>		   UniqueRenames;
	std::unordered_set<FPath::string_type> UniqueRenamesSet;

	// Build a rename schedule, with only unique entries (taking subdirectories into account)

	for (const FPendingFileRename& Entry : PendingRenames)
	{
		UNSYNC_ASSERTF(StringToLower(Entry.Old) == StringToLower(Entry.New),
					   L"FixFileNameCases expects inputs that are different only by case. Old: '%ls', New: '%ls'",
					   Entry.Old.c_str(),
					   Entry.New.c_str());

		FPath OldPath = Entry.Old;
		FPath NewPath = Entry.New;

		auto ItOld = OldPath.begin();
		auto ItNew = NewPath.begin();

		FPath OldPathPart;
		FPath NewPathPart;

		while (ItOld != OldPath.end())
		{
			OldPathPart /= *ItOld;
			NewPathPart /= *ItNew;

			if (*ItOld != *ItNew)
			{
				auto InsertResult = UniqueRenamesSet.insert(OldPathPart.native());
				if (InsertResult.second)
				{
					UniqueRenames.push_back({OldPathPart.wstring(), NewPathPart.wstring()});
				}
			}

			++ItOld;
			++ItNew;
		}
	}

	std::sort(UniqueRenames.begin(), UniqueRenames.end(), [](const FPendingFileRename& A, const FPendingFileRename& B) {
		return A.Old < B.Old;
	});

	// Perform actual renaming

	for (const FPendingFileRename& Entry : UniqueRenames)
	{
		FPath OldPath = RootPath / Entry.Old;
		FPath NewPath = RootPath / Entry.New;

		std::error_code ErrorCode;

		if (GDryRun)
		{
			UNSYNC_VERBOSE(L"Renaming '%ls' -> '%ls' (skipped due to dry run mode)", Entry.Old.c_str(), Entry.New.c_str());
		}
		else
		{
			UNSYNC_VERBOSE(L"Renaming '%ls' -> '%ls'", Entry.Old.c_str(), Entry.New.c_str());
			FileRename(OldPath, NewPath, ErrorCode);
		}

		if (ErrorCode)
		{
			UNSYNC_VERBOSE(L"Failed to rename file. System error code %d: %hs", ErrorCode.value(), ErrorCode.message().c_str());
			return false;
		}
	}

	return true;
}

static bool
MergeManifests(FDirectoryManifest& Existing, const FDirectoryManifest& Other, bool bCaseSensitive)
{
	if (!Existing.IsValid())
	{
		Existing = Other;
		return true;
	}

	if (!AlgorithmOptionsCompatible(Existing.Algorithm, Other.Algorithm))
	{
		UNSYNC_ERROR("Trying to merge incompatible manifests (diff algorithm options do not match)");
		return false;
	}

	if (bCaseSensitive)
	{
		// Trivial case: just replace existing entries
		for (const auto& OtherFile : Other.Files)
		{
			Existing.Files[OtherFile.first] = OtherFile.second;
		}
	}
	else
	{
		// Lookup table of lowercase -> original file name used to replace conflicting entries on non-case-sensitive filesystems
		// TODO: Could potentially add case-sensitive/insensitive entry lookup helper functions to FDirectoryManifest itself in the future
		std::unordered_map<std::wstring, std::wstring> ExistingFileNamesLowerCase;

		for (auto& ExistingEntry : Existing.Files)
		{
			std::wstring FileNameLowerCase = StringToLower(ExistingEntry.first);
			ExistingFileNamesLowerCase.insert(std::pair<std::wstring, std::wstring>(FileNameLowerCase, ExistingEntry.first));
		}

		for (const auto& OtherFile : Other.Files)
		{
			std::wstring OtherNameLowerCase = StringToLower(OtherFile.first);
			auto		 LowerCaseEntry		= ExistingFileNamesLowerCase.find(OtherNameLowerCase);
			if (LowerCaseEntry != ExistingFileNamesLowerCase.end())
			{
				// Remove file with conflicting case and add entry from the other manifest instead
				const std::wstring& ExistingNameOriginalCase = LowerCaseEntry->second;
				Existing.Files.erase(ExistingNameOriginalCase);

				// Update the lookup table entry to refer to the name we're about to insert
				ExistingFileNamesLowerCase[LowerCaseEntry->first] = OtherFile.first;
			}

			Existing.Files[OtherFile.first] = OtherFile.second;
		}
	}

	return true;
}

// Delete files from target directory that are not in the source directory manifest
static void
DeleteUnnecessaryFiles(const FPath&				 TargetDirectory,
					   const FDirectoryManifest& TargetDirectoryManifest,
					   const FDirectoryManifest& ReferenceManifest,
					   const FSyncFilter*		 SyncFilter)
{
	auto ShouldCleanup = [SyncFilter](const FPath& Filename) -> bool {
		if (SyncFilter)
		{
			return SyncFilter->ShouldCleanup(Filename);
		}
		else
		{
			return true;
		}
	};

	for (const auto& TargetManifestEntry : TargetDirectoryManifest.Files)
	{
		const std::wstring& TargetFileName = TargetManifestEntry.first;

		auto Cleanup = [&ShouldCleanup, &TargetDirectory](const std::wstring& TargetFileName, const wchar_t* Reason) {
			FPath FilePath = TargetDirectory / TargetFileName;

			if (!ShouldCleanup(TargetFileName))
			{
				UNSYNC_VERBOSE2(L"Skipped deleting '%ls' (excluded by cleanup filter)", FilePath.wstring().c_str());
				return;
			}

			if (GDryRun)
			{
				UNSYNC_VERBOSE(L"Deleting '%ls' (%ls, skipped due to dry run mode)", FilePath.wstring().c_str(), Reason);
			}
			else
			{
				UNSYNC_VERBOSE(L"Deleting '%ls' (%ls)", FilePath.wstring().c_str(), Reason);
				std::error_code ErrorCode = {};
				FileRemove(FilePath, ErrorCode);
				if (ErrorCode)
				{
					UNSYNC_VERBOSE(L"System error code %d: %hs", ErrorCode.value(), ErrorCode.message().c_str());
				}
			}
		};

		if (ReferenceManifest.Files.find(TargetFileName) == ReferenceManifest.Files.end())
		{
			Cleanup(TargetFileName, L"not in manifest");
		}
		else if (!SyncFilter->ShouldSync(TargetFileName))
		{
			Cleanup(TargetFileName, L"excluded from sync");
		}
	}
}

FPath
ToPath(const std::wstring_view& Str)
{
#if UNSYNC_PLATFORM_UNIX
	// TODO: ensure that all serialized path separators are unix style ('/')
	std::wstring Temp = std::wstring(Str);
	std::replace(Temp.begin(), Temp.end(), L'\\', L'/');
	return FPath(Temp);
#else	// UNSYNC_PLATFORM_UNIX
	return FPath(Str);
#endif	// UNSYNC_PLATFORM_UNIX
}

struct FPooledProxy
{
	FPooledProxy(FProxyPool& InProxyPool) : ProxyPool(InProxyPool) { Proxy = ProxyPool.Alloc(); }
	~FPooledProxy() { ProxyPool.Dealloc(std::move(Proxy)); }
	const FProxy&			operator->() const { return *Proxy; }
	FProxyPool&				ProxyPool;
	std::unique_ptr<FProxy> Proxy;
};

static bool
DownloadFileIfNewer(const FRemoteDesc& RemoteDesc, const FAuthDesc* AuthDesc, const FPath& Source, const FPath& Target, EFileMode TargetFileMode)
{
	using FDirectoryListing		 = ProxyQuery::FDirectoryListing;
	using FDirectoryListingEntry = ProxyQuery::FDirectoryListingEntry;

	FPath SourceParent = Source.parent_path();
	FPath SourceFileName = Source.filename().string();

	std::string SourceUtf8		   = ConvertWideToUtf8(Source.wstring());
	std::string SourceParentUtf8   = ConvertWideToUtf8(SourceParent.wstring());
	std::string SourceFileNameUtf8 = ConvertWideToUtf8(SourceFileName.wstring());

	// TODO: could have a dedicated single file stat query
	TResult<FDirectoryListing> DirectoryListingResult = ProxyQuery::ListDirectory(RemoteDesc, AuthDesc, SourceParentUtf8);
	if (DirectoryListingResult.IsError())
	{
		LogError(DirectoryListingResult.GetError());
		return false;
	}

	const FDirectoryListing&	  DirectoryListing = DirectoryListingResult.GetData();
	const FDirectoryListingEntry* SourceEntry	   = nullptr;
	for (const FDirectoryListingEntry& Entry : DirectoryListing.Entries)
	{
		if (Entry.Name == SourceFileNameUtf8 && !Entry.bDirectory)
		{
			SourceEntry = &Entry;
			break;
		}
	}

	if (!SourceEntry)
	{
		UNSYNC_ERROR(L"Remote file '%ls' does not exist", Source.wstring().c_str());
		return false;
	}

	FFileAttributes TargetAttr = GetFileAttrib(Target);
	if (SourceEntry->Size != TargetAttr.Size || SourceEntry->Mtime != TargetAttr.Mtime)
	{
		UNSYNC_VERBOSE(L"Downloading '%ls'", Source.wstring().c_str());
		TResult<FBuffer> DownloadResult = ProxyQuery::DownloadFile(RemoteDesc, AuthDesc, SourceUtf8);
		if (DownloadResult.IsError())
		{
			LogError(DownloadResult.GetError());
			return false;
		}

		const FBuffer& FileBuffer = DownloadResult.GetData();
		if (FileBuffer.Size() != SourceEntry->Size)
		{
			UNSYNC_ERROR(L"Downloaded file size mismatch. Expected %llu, actual %llu.",
						 llu(SourceEntry->Size),
						 llu(FileBuffer.Size()));
			return false;
		}

		bool bFileWritten = WriteBufferToFile(Target, FileBuffer, TargetFileMode);
		if (!bFileWritten)
		{
			UNSYNC_ERROR(L"Failed to write downloaded file '%ls'", Target.wstring().c_str());
			return false;
		}

		SetFileMtime(Target, SourceEntry->Mtime, /*allow in dry run*/ true);
	}

	return true;
}

static bool
LoadAndMergeSourceManifest(FDirectoryManifest& Output,
						   FProxyPool&		   ProxyPool,
						   const FPath&		   SourcePath,
						   const FPath&		   TempPath,
						   FSyncFilter*		   SyncFilter,
						   const FPath&		   SourceManifestOverride,
						   bool				   bCaseSensitiveTargetFileSystem)
{
	const FRemoteProtocolFeatures& ProxyFeatures = ProxyPool.GetFeatures();

	const bool bDownloadManifestFromProxy =
		ProxyPool.RemoteDesc.Protocol == EProtocolFlavor::Unsync && ProxyFeatures.bDirectoryListing && ProxyFeatures.bFileDownload;

	UNSYNC_VERBOSE2(L"LoadAndMergeSourceManifest: '%ls' (%hs)",
					SourcePath.wstring().c_str(),
					bDownloadManifestFromProxy ? "download" : "filesystem");

	auto ResolvePath = [SyncFilter, bDownloadManifestFromProxy](const FPath& Filename) -> FPath
	{ return (SyncFilter && !bDownloadManifestFromProxy) ? SyncFilter->Resolve(Filename) : Filename; };

	FDirectoryManifest LoadedManifest;

	FPath SourceManifestRoot = SourcePath / ".unsync";
	FPath SourceManifestPath = SourceManifestRoot / "manifest.bin";

	SourceManifestPath = ResolvePath(SourceManifestPath);

	if (!SourceManifestOverride.empty())
	{
		SourceManifestPath = SourceManifestOverride;
	}

	FHash128 SourcePathHash =
		HashBlake3Bytes<FHash128>((const uint8*)SourcePath.native().c_str(), SourcePath.native().length() * sizeof(SourcePath.native()[0]));

	std::string SourcePathHashStr	   = BytesToHexString(SourcePathHash.Data, sizeof(SourcePathHash.Data));
	FPath		SourceManifestTempPath = TempPath / SourcePathHashStr;

	LogGlobalStatus(L"Caching source manifest");
	UNSYNC_VERBOSE(L"Caching source manifest");

	UNSYNC_VERBOSE(L" Source '%ls'", SourceManifestPath.wstring().c_str());
	UNSYNC_VERBOSE(L" Target '%ls'", SourceManifestTempPath.wstring().c_str());

	if (bDownloadManifestFromProxy)
	{
		UNSYNC_LOG_INDENT;
		bool bDownloadedOk = DownloadFileIfNewer(ProxyPool.RemoteDesc,
												 ProxyPool.AuthDesc,
												 SourceManifestPath,
												 SourceManifestTempPath,
												 EFileMode::CreateWriteOnly | EFileMode::IgnoreDryRun);

		if (!bDownloadedOk)
		{
			UNSYNC_ERROR(L"Failed to download manifest file '%ls'", SourceManifestPath.wstring().c_str());
			return false;
		}
	}
	else
	{
		UNSYNC_LOG_INDENT;
		if (!PathExists(SourceManifestPath))
		{
			UNSYNC_ERROR(L"Source manifest '%ls' does not exist", SourceManifestPath.wstring().c_str());
			return false;
		}

		std::error_code CopyErrorCode = CopyFileIfNewer(SourceManifestPath, SourceManifestTempPath);
		if (CopyErrorCode)
		{
			UNSYNC_LOG(L"Failed to copy manifest '%ls' to '%ls'",
					   SourceManifestPath.wstring().c_str(),
					   SourceManifestTempPath.wstring().c_str());
			UNSYNC_ERROR(L"%hs (%d)", CopyErrorCode.message().c_str(), CopyErrorCode.value());
			return false;
		}
	}

	if (!LoadDirectoryManifest(LoadedManifest, SourcePath, SourceManifestTempPath))
	{
		UNSYNC_ERROR(L"Failed to load source directory manifest '%ls'", SourceManifestPath.wstring().c_str());

		return false;
	}

	if (Output.IsValid() && !AlgorithmOptionsCompatible(Output.Algorithm, LoadedManifest.Algorithm))
	{
		UNSYNC_ERROR(L"Can't merge manifest '%ls' as it uses different algorithm options", SourcePath.wstring().c_str());
		return false;
	}

	return MergeManifests(Output, LoadedManifest, bCaseSensitiveTargetFileSystem);
}

struct FFileSyncTaskBatch
{
	std::vector<const FFileSyncTask*> FileTasks;

	uint64 TotalSizeBytes	   = 0;
	uint64 NeedBytesFromSource = 0;

	std::unique_ptr<FBlockCache> CreateBlockCache(FProxyPool& ProxyPool, EStrongHashAlgorithmID StrongHasher) const
	{
		std::unique_ptr<FBlockCache> Result = std::make_unique<FBlockCache>();

		Result->BlockData.Resize(NeedBytesFromSource);

		uint64 OutputCursor = 0;

		THashSet<FHash128>		UniqueBlockSet;
		std::vector<FNeedBlock> UniqueNeedBlocks;
		for (const FFileSyncTask* Task : FileTasks)
		{
			for (const FNeedBlock& Block : Task->NeedList.Source)
			{
				if (UniqueBlockSet.insert(Block.Hash.ToHash128()).second)
				{
					UniqueNeedBlocks.push_back(Block);
				}
			}
		}

		Result->BlockMap.reserve(UniqueNeedBlocks.size());

		ProxyPool.ParallelDownloadSemaphore.Acquire();
		std::unique_ptr<FProxy> Proxy = ProxyPool.Alloc();

		if (Proxy)
		{
			auto DownloadCallback = [&OutputCursor, &Result, &UniqueBlockSet, StrongHasher](const FDownloadedBlock& Block,
																							FHash128				BlockHash) {
				if (OutputCursor + Block.DecompressedSize <= Result->BlockData.Size())
				{
					if (UniqueBlockSet.find(BlockHash) != UniqueBlockSet.end())
					{
						FMutBufferView OutputView = Result->BlockData.MutView(OutputCursor, Block.DecompressedSize);

						bool bOk = true;

						if (Block.IsCompressed())
						{
							bOk = Decompress(Block.Data, Block.CompressedSize, OutputView.Data, OutputView.Size);
						}
						else
						{
							memcpy(OutputView.Data, Block.Data, OutputView.Size);
						}

						if (bOk)
						{
							const FHash128 ActualBlockHash = ComputeHash(OutputView.Data, OutputView.Size, StrongHasher).ToHash128();

							bOk = BlockHash == ActualBlockHash;
						}

						if (bOk)
						{
							Result->BlockMap[BlockHash] = FBufferView{OutputView.Data, OutputView.Size};
							OutputCursor += Block.DecompressedSize;
						}
					}
				}
			};

			FDownloadResult DownloadResult =
				Proxy->Download(MakeView<FNeedBlock>(UniqueNeedBlocks.data(), UniqueNeedBlocks.size()), DownloadCallback);

			UNSYNC_UNUSED(DownloadResult);
		}

		ProxyPool.ParallelDownloadSemaphore.Release();

		return Result;
	}
};

static void
DeleteOldFilesInDirectory(FPath& Path, uint32 MaxFilesToKeep)
{
	struct FEntry
	{
		FPath  Path;
		uint64 Mtime;
	};

	std::vector<FEntry> Entries;

	FPath ExtendedPath = MakeExtendedAbsolutePath(Path);
	for (const std::filesystem::directory_entry& It : std::filesystem::directory_iterator(ExtendedPath))
	{
		if (It.is_regular_file())
		{
			FEntry Entry;
			Entry.Mtime = ToWindowsFileTime(It.last_write_time());
			Entry.Path	= It.path();
			Entries.push_back(Entry);
		}
	}

	// reverse sort
	std::sort(Entries.begin(), Entries.end(), [](const FEntry& A, const FEntry& B) { return A.Mtime > B.Mtime; });

	while (Entries.size() > MaxFilesToKeep)
	{
		const FEntry& Oldest = Entries.back();

		std::wstring PathStr = FPath(RemoveExtendedPathPrefix(Oldest.Path)).wstring();

		if (GDryRun)
		{
			UNSYNC_VERBOSE(L"Deleting '%ls'(skipped due to dry run mode)", PathStr.c_str());
		}
		else
		{
			UNSYNC_VERBOSE(L"Deleting '%ls'", PathStr.c_str());
			std::error_code ErrorCode = {};
			FileRemove(Oldest.Path, ErrorCode);
		}

		Entries.pop_back();
	}
}

bool  // TODO: return a TResult
SyncDirectory(const FSyncDirectoryOptions& SyncOptions)
{
	FProxyPool	DummyProxyPool;
	FProxyPool& ProxyPool = SyncOptions.ProxyPool ? *SyncOptions.ProxyPool : DummyProxyPool;

	FTimePoint TimeBegin = TimePointNow();

	const bool bFileSystemSource = SyncOptions.SourceType == ESyncSourceType::FileSystem;
	const bool bServerSource =
		SyncOptions.SourceType == ESyncSourceType::Server || SyncOptions.SourceType == ESyncSourceType::ServerWithManifestHash;

	UNSYNC_ASSERT(bFileSystemSource || bServerSource);

	const FPath	 SourcePath				= bFileSystemSource ? std::filesystem::absolute(SyncOptions.Source) : SyncOptions.Source;
	const FPath	 BasePath				= std::filesystem::absolute(SyncOptions.Base);
	const FPath	 TargetPath				= std::filesystem::absolute(SyncOptions.Target);
	const FPath& SourceManifestOverride = SyncOptions.SourceManifestOverride;

	FSyncFilter* SyncFilter = SyncOptions.SyncFilter;

	bool bSourceManifestOk = true;

	UNSYNC_LOG_INDENT;

	if (SyncOptions.bCleanup)
	{
		UNSYNC_LOG(L"Unnecessary files will be deleted after sync (cleanup mode)");
	}

	FPath BaseManifestRoot = BasePath / ".unsync";
	FPath BaseManifestPath = BaseManifestRoot / "manifest.bin";

	FPath TargetManifestRoot = TargetPath / ".unsync";
	FPath TargetManifestPath = TargetManifestRoot / "manifest.bin";
	FPath TargetTempPath	 = TargetManifestRoot / "temp";

	bool bTempDirectoryExists = (PathExists(TargetTempPath) && IsDirectory(TargetTempPath)) || CreateDirectories(TargetTempPath);

	if (!bTempDirectoryExists)
	{
		UNSYNC_ERROR(L"Failed to create temporary working directory");
		return false;
	}

	// Delete oldest cached manifest files if there are more than N
	{
		UNSYNC_VERBOSE(L"Cleaning temporary directory");
		UNSYNC_LOG_INDENT;
		const uint32 MaxFilesToKeep = uint32(5 + SyncOptions.Overlays.size());
		DeleteOldFilesInDirectory(TargetTempPath, MaxFilesToKeep);
	}

	FPath		  LogFilePath = TargetManifestRoot / L"unsync.log";
	FLogFileScope LogFileScope(LogFilePath.wstring().c_str());
	SetCrashDumpPath(TargetManifestRoot);

	auto ShouldSync = [SyncFilter](const FPath& Filename) -> bool {
		if (SyncFilter)
		{
			return SyncFilter->ShouldSync(Filename);
		}
		else
		{
			return true;
		}
	};

	auto ResolvePath = [SyncFilter](const FPath& Filename) -> FPath { return SyncFilter ? SyncFilter->Resolve(Filename) : Filename; };

	FDirectoryManifest SourceDirectoryManifest;
	FPath			   SourceManifestTempPath;

	const bool bCaseSensitiveTargetFileSystem = IsCaseSensitiveFileSystem(TargetTempPath);

	FTimingLogger ManifestLoadTimingLogger("Manifest load time", ELogLevel::Info);

	if (SyncOptions.SourceType == ESyncSourceType::ServerWithManifestHash)
	{
		if (!ProxyPool.IsValid())
		{
			UNSYNC_ERROR(L"Remote server connection is required when syncing by manifest hash");
			return false;
		}

		UNSYNC_LOG(L"Downloading manifest ...");

		std::unique_ptr<FProxy> Proxy = ProxyPool.Alloc();

		std::string		 SourceManifestName = ConvertWideToUtf8(SyncOptions.Source.wstring());
		TResult<FBuffer> DownloadResult		= Proxy->DownloadManifest(SourceManifestName);
		if (FBuffer* ManifestBuffer = DownloadResult.TryData())
		{
			FMemReader		Reader(*ManifestBuffer);
			FIOReaderStream Stream(Reader);
			FPath			EmptyRoot;	// Don't have a sensible path when not using file system as source
			bSourceManifestOk = LoadDirectoryManifest(SourceDirectoryManifest, EmptyRoot, Stream);
		}
		else
		{
			LogError(DownloadResult.GetError());
			UNSYNC_BREAK_ON_ERROR;
			return false;
		}
	}
	else
	{
		std::vector<FPath> AllSources;
		AllSources.push_back(SourcePath);
		for (const FPath& OverlayPath : SyncOptions.Overlays)
		{
			AllSources.push_back(OverlayPath);
		}

		for (const FPath& ThisSourcePath : AllSources)
		{
			if (!LoadAndMergeSourceManifest(SourceDirectoryManifest,
											ProxyPool,
											ThisSourcePath,
											TargetTempPath,
											SyncFilter,
											SourceManifestOverride,
											bCaseSensitiveTargetFileSystem))
			{
				return false;
			}
		}
	}

	{
		UNSYNC_VERBOSE(L"Loaded manifest properties:");
		UNSYNC_LOG_INDENT;
		FDirectoryManifestInfo ManifestInfo = GetManifestInfo(SourceDirectoryManifest, false /*bGenerateSignature*/);
		LogManifestInfo(ELogLevel::Debug, ManifestInfo);
		if (ProxyPool.RemoteDesc.Protocol == EProtocolFlavor::Jupiter && ManifestInfo.NumMacroBlocks == 0)
		{
			UNSYNC_ERROR(L"Manifest must contain macro blocks when using Jupiter");
			return false;
		}
	}

	ManifestLoadTimingLogger.Finish();

	FTimingLogger TargetManifestTimingLogger("Target directory manifest generation time", ELogLevel::Info);
	UNSYNC_LOG(L"Creating manifest for directory '%ls'", TargetPath.wstring().c_str());


	// Propagate algorithm selection from source
	const FAlgorithmOptions Algorithm = SourceDirectoryManifest.Algorithm;

	FComputeBlocksParams LightweightManifestParams;
	LightweightManifestParams.Algorithm	  = Algorithm;
	LightweightManifestParams.bNeedBlocks = false;
	LightweightManifestParams.BlockSize	  = 0;

	FDirectoryManifest TargetDirectoryManifest = CreateDirectoryManifest(TargetPath, LightweightManifestParams);

	TargetManifestTimingLogger.Finish();

	if (!bCaseSensitiveTargetFileSystem)
	{
		std::vector<FPendingFileRename> PendingRenames;
		PendingRenames = FixManifestFileNameCases(TargetDirectoryManifest, SourceDirectoryManifest);
		if (!PendingRenames.empty())
		{
			UNSYNC_VERBOSE(L"Fixing inconsistent case of target files");
			UNSYNC_LOG_INDENT;
			if (!FixFileNameCases(TargetPath, PendingRenames))
			{
				return false;
			}
		}
	}

	uint32 StatSkipped	   = 0;
	uint32 StatFullCopy	   = 0;
	uint32 StatPartialCopy = 0;

	std::atomic<uint64> NumFailedTasks = {};

	std::atomic<uint64> StatSourceBytes = {};
	std::atomic<uint64> StatBaseBytes	= {};

	std::vector<FFileSyncTask> AllFileTasks;

	LogGlobalStatus(L"Scanning base directory");
	UNSYNC_LOG(L"Scanning base directory");
	FFileAttributeCache BaseAttribCache = CreateFileAttributeCache(BasePath, SyncFilter);
	UNSYNC_LOG(L"Base files: %d", (uint32)BaseAttribCache.Map.size());

	FFileAttributeCache SourceAttribCache;
	if (bFileSystemSource && SyncOptions.bValidateSourceFiles)
	{
		LogGlobalStatus(L"Scanning source directory");
		UNSYNC_LOG(L"Scanning source directory");
		SourceAttribCache = CreateFileAttributeCache(SourcePath, SyncFilter);
	}

	// If variable blocks are used and we already have a manifest file from previous sync,
	// then we can compute difference quickly based only on file timestamps and previously computed chunks.

	FDirectoryManifest BaseDirectoryManifest;
	bool			   bBaseDirectoryManifestValid = false;
	bool			   bQuickDifferencePossible	   = false;

	if (!SyncOptions.bFullDifference && SourceDirectoryManifest.Algorithm.ChunkingAlgorithmId == EChunkingAlgorithmID::VariableBlocks &&
		PathExists(BaseManifestPath))
	{
		bBaseDirectoryManifestValid = LoadDirectoryManifest(BaseDirectoryManifest, BasePath, BaseManifestPath);
		if (bBaseDirectoryManifestValid && AlgorithmOptionsCompatible(SourceDirectoryManifest.Algorithm, TargetDirectoryManifest.Algorithm))
		{
			bQuickDifferencePossible = true;
		}
	}

	if (bQuickDifferencePossible)
	{
		UNSYNC_LOG(L"Quick file difference is allowed (use --full-diff option to override)");
	}

	uint64 TotalSourceSize = 0;

	for (const auto& SourceManifestIt : SourceDirectoryManifest.Files)
	{
		const std::wstring& SourceFilename = SourceManifestIt.first;

		if (!ShouldSync(SourceFilename))
		{
			StatSkipped++;
			UNSYNC_VERBOSE2(L"Skipped '%ls' (excluded by sync filter)", SourceManifestIt.first.c_str());
			continue;
		}

		const FFileManifest& SourceFileManifest = SourceManifestIt.second;

		TotalSourceSize += SourceFileManifest.Size;

		bool bTargetFileAttributesMatch = false;
		auto TargetManifestIt			= TargetDirectoryManifest.Files.find(SourceFilename);
		if (TargetManifestIt != TargetDirectoryManifest.Files.end())
		{
			const FFileManifest& TargetFileManifest = TargetManifestIt->second;

			if (SourceFileManifest.Size == TargetFileManifest.Size && SourceFileManifest.Mtime == TargetFileManifest.Mtime)
			{
				bTargetFileAttributesMatch = true;
			}
		}

		if (bTargetFileAttributesMatch && !SyncOptions.bFullDifference)
		{
			UNSYNC_VERBOSE2(L"Skipped '%ls' (up to date)", SourceManifestIt.first.c_str());
			StatSkipped++;
			continue;
		}

		FPath SourceFilePath = SourceManifestIt.second.CurrentPath;
		FPath BaseFilePath	 = BasePath / ToPath(SourceManifestIt.first);
		FPath TargetFilePath = TargetPath / ToPath(SourceManifestIt.first);

		FPath ResolvedSourceFilePath = ResolvePath(SourceFilePath);

		if (bFileSystemSource && SyncOptions.bValidateSourceFiles)
		{
			FFileAttributes SourceFileAttrib = GetFileAttrib(ResolvedSourceFilePath, &SourceAttribCache);

			if (!SourceFileAttrib.bValid)
			{
				UNSYNC_ERROR(L"Source file '%ls' is declared in manifest but does not exist. Manifest may be wrong or out of date.",
							 SourceFilePath.wstring().c_str());
				bSourceManifestOk = false;
			}

			if (bSourceManifestOk && SourceFileAttrib.Size != SourceFileManifest.Size)
			{
				UNSYNC_ERROR(
					L"Source file '%ls' size (%lld bytes) does not match the manifest (%lld bytes). Manifest may be wrong or out of date.",
					SourceFilePath.wstring().c_str(),
					SourceFileAttrib.Size,
					SourceFileManifest.Size);
				bSourceManifestOk = false;
			}

			if (bSourceManifestOk && SourceFileAttrib.Mtime != SourceFileManifest.Mtime)
			{
				UNSYNC_ERROR(
					L"Source file '%ls' modification time (%lld) does not match the manifest (%lld). Manifest may be wrong or out of date.",
					SourceFilePath.wstring().c_str(),
					SourceFileAttrib.Mtime,
					SourceFileManifest.Mtime);
				bSourceManifestOk = false;
			}
		}

		if (bSourceManifestOk)
		{
			FFileAttributes BaseFileAttrib = GetCachedFileAttrib(BaseFilePath, BaseAttribCache);

			if (!BaseFileAttrib.bValid)
			{
				UNSYNC_VERBOSE2(L"Dirty file: '%ls' (no base data)", SourceFilename.c_str());
				StatFullCopy++;
			}
			else
			{
				if (bTargetFileAttributesMatch && SyncOptions.bFullDifference)
				{
					UNSYNC_VERBOSE2(L"Dirty file: '%ls' (forced by --full-diff)", SourceManifestIt.first.c_str());
				}
				else
				{
					UNSYNC_VERBOSE2(L"Dirty file: '%ls'", SourceManifestIt.first.c_str());
				}

				StatPartialCopy++;

				if (bFileSystemSource && SyncOptions.bValidateSourceFiles && !SourceAttribCache.Exists(ResolvedSourceFilePath) &&
					!PathExists(ResolvedSourceFilePath))
				{
					UNSYNC_VERBOSE(L"Source file '%ls' does not exist", SourceFilePath.wstring().c_str());
					continue;
				}
			}

			FFileSyncTask Task;
			Task.OriginalSourceFilePath = std::move(SourceFilePath);
			Task.ResolvedSourceFilePath = std::move(ResolvedSourceFilePath);
			Task.BaseFilePath			= std::move(BaseFilePath);
			Task.TargetFilePath			= std::move(TargetFilePath);
			Task.SourceManifest			= &SourceFileManifest;

			if (bQuickDifferencePossible)
			{
				UNSYNC_ASSERT(bBaseDirectoryManifestValid);
				auto BaseManifestIt = BaseDirectoryManifest.Files.find(SourceFilename);
				if (BaseManifestIt != BaseDirectoryManifest.Files.end())
				{
					const FFileManifest& BaseFileManifest = BaseManifestIt->second;
					if (BaseFileManifest.Mtime == BaseFileAttrib.Mtime && BaseFileManifest.Size == BaseFileAttrib.Size)
					{
						if (ValidateBlockListT(BaseFileManifest.Blocks))
						{
							Task.BaseManifest = &BaseFileManifest;
						}
					}
				}
			}

			AllFileTasks.push_back(Task);
		}
	}

	if (SourceDirectoryManifest.Files.empty())
	{
		UNSYNC_ERROR(L"Source directory manifest is empty");
		bSourceManifestOk = false;
	}

	if (!bSourceManifestOk)
	{
		return false;
	}

	LogGlobalStatus(L"Computing difference");
	UNSYNC_LOG(L"Computing difference ...");

	uint64 EstimatedNeedBytesFromSource = 0;
	uint64 EstimatedNeedBytesFromBase	= 0;
	uint64 TotalSyncSizeBytes			= 0;

	{
		UNSYNC_LOG_INDENT;
		auto TimeDiffBegin = TimePointNow();

		auto DiffTask = [Algorithm, bQuickDifferencePossible](FFileSyncTask& Item) {
			FLogVerbosityScope VerbosityScope(false);  // turn off logging from threads

			const FGenericBlockArray& SourceBlocks = Item.SourceManifest->Blocks;

			if (Item.IsBaseValid() && PathExists(Item.BaseFilePath))
			{
				FNativeFile BaseFile(Item.BaseFilePath, EFileMode::ReadOnlyUnbuffered);
				uint32	   SourceBlockSize = Item.SourceManifest->BlockSize;
				UNSYNC_VERBOSE(L"Computing difference for target '%ls' (base size: %.2f MB)",
							   Item.BaseFilePath.wstring().c_str(),
							   SizeMb(BaseFile.GetSize()));

				if (bQuickDifferencePossible && Item.BaseManifest)
				{
					Item.NeedList = DiffManifestBlocks(Item.SourceManifest->Blocks, Item.BaseManifest->Blocks);
				}
				else if (Algorithm.ChunkingAlgorithmId == EChunkingAlgorithmID::FixedBlocks)
				{
					Item.NeedList =
						DiffBlocks(BaseFile, SourceBlockSize, Algorithm.WeakHashAlgorithmId, Algorithm.StrongHashAlgorithmId, SourceBlocks);
				}
				else if (Algorithm.ChunkingAlgorithmId == EChunkingAlgorithmID::VariableBlocks)
				{
					Item.NeedList = DiffBlocksVariable(BaseFile,
													   SourceBlockSize,
													   Algorithm.WeakHashAlgorithmId,
													   Algorithm.StrongHashAlgorithmId,
													   SourceBlocks);
				}
				else
				{
					UNSYNC_FATAL(L"Unexpected file difference calculation mode");
				}
			}
			else
			{
				Item.NeedList.Sequence.reserve(SourceBlocks.size());
				Item.NeedList.Source.reserve(SourceBlocks.size());
				for (const FGenericBlock& Block : SourceBlocks)
				{
					FNeedBlock NeedBlock;
					NeedBlock.Size		   = Block.Size;
					NeedBlock.SourceOffset = Block.Offset;
					NeedBlock.TargetOffset = Block.Offset;
					NeedBlock.Hash		   = Block.HashStrong;
					Item.NeedList.Source.push_back(NeedBlock);
					Item.NeedList.Sequence.push_back(NeedBlock.Hash.ToHash128());  // #wip-widehash
				}
			}

			Item.NeedBytesFromSource = ComputeSize(Item.NeedList.Source);
			Item.NeedBytesFromBase	 = ComputeSize(Item.NeedList.Base);
			Item.TotalSizeBytes		 = Item.SourceManifest->Size;

			UNSYNC_ASSERT(Item.NeedBytesFromSource + Item.NeedBytesFromBase == Item.TotalSizeBytes);
		};

		ParallelForEach(AllFileTasks, DiffTask);

		auto TimeDiffEnd = TimePointNow();

		double Duration = DurationSec(TimeDiffBegin, TimeDiffEnd);
		UNSYNC_LOG(L"Difference complete in %.3f sec", Duration);

		for (FFileSyncTask& Item : AllFileTasks)
		{
			EstimatedNeedBytesFromSource += Item.NeedBytesFromSource;
			EstimatedNeedBytesFromBase += Item.NeedBytesFromBase;
			TotalSyncSizeBytes += Item.TotalSizeBytes;
		}

		UNSYNC_LOG(L"Total need from source: %.2f MB", SizeMb(EstimatedNeedBytesFromSource));
		UNSYNC_LOG(L"Total need from base: %.2f MB", SizeMb(EstimatedNeedBytesFromBase));

		uint64 AvailableDiskBytes = SyncOptions.bCheckAvailableSpace ? GetAvailableDiskSpace(TargetPath) : ~0ull;
		if (TotalSyncSizeBytes > AvailableDiskBytes)
		{
			UNSYNC_ERROR(
				L"Sync requires %.0f MB (%llu bytes) of disk space, but only %.0f MB (%llu bytes) is available. "
				L"Use --no-space-validation flag to suppress this check.",
				SizeMb(TotalSyncSizeBytes),
				TotalSyncSizeBytes,
				SizeMb(AvailableDiskBytes),
				AvailableDiskBytes);
			return false;
		}
	}

	GGlobalProgressCurrent = 0;
	GGlobalProgressTotal =
		EstimatedNeedBytesFromSource * GLOBAL_PROGRESS_SOURCE_SCALE + EstimatedNeedBytesFromBase * GLOBAL_PROGRESS_BASE_SCALE;

	std::unique_ptr<FScavengeDatabase> ScavengeDatabase;
	if (!SyncOptions.ScavengeRoot.empty())
	{
		UNSYNC_LOG(L"Scavenging blocks from existing data sets");
		UNSYNC_LOG_INDENT;

		FTimePoint ScavengeDbTimeBegin = TimePointNow();

		TArrayView<FFileSyncTask> AllFileTasksView = MakeView(AllFileTasks.data(), AllFileTasks.size());
		ScavengeDatabase = std::unique_ptr<FScavengeDatabase>(FScavengeDatabase::BuildFromFileSyncTasks(SyncOptions, AllFileTasksView));

		double Duration = DurationSec(ScavengeDbTimeBegin, TimePointNow());

		UNSYNC_LOG(L"Done in %.3f sec", Duration);
	}

	LogGlobalProgress();

	if (ProxyPool.IsValid())
	{
		LogGlobalStatus(L"Connecting to server");
		UNSYNC_LOG(L"Connecting to %hs server '%hs:%d' ...",
					   ToString(ProxyPool.RemoteDesc.Protocol),
					   ProxyPool.RemoteDesc.Host.Address.c_str(),
					   ProxyPool.RemoteDesc.Host.Port);
		UNSYNC_LOG_INDENT;

		std::unique_ptr<FProxy> Proxy = ProxyPool.Alloc();

		if (Proxy.get() && Proxy->IsValid())
		{
			// TODO: report TLS status
			// ESocketSecurity security = proxy->get_socket_security();
			// UNSYNC_LOG(L"Connection established (security: %hs)", ToString(security));

			UNSYNC_LOG(L"Connection established");
			UNSYNC_LOG(L"Building block request map");

			const bool bProxyHasData = Proxy->Contains(SourceDirectoryManifest);

			ProxyPool.Dealloc(std::move(Proxy));

			if (bProxyHasData)
			{
				ProxyPool.InitRequestMap(SourceDirectoryManifest.Algorithm.StrongHashAlgorithmId);

				for (const FFileSyncTask& Task : AllFileTasks)
				{
					ProxyPool.BuildFileBlockRequests(Task.OriginalSourceFilePath, Task.ResolvedSourceFilePath, *Task.SourceManifest);
				}
			}
			else
			{
				UNSYNC_WARNING(L"Remote server does not have the data referenced by manifest");
				ProxyPool.Invalidate();
			}
		}
		else
		{
			Proxy = nullptr;
			ProxyPool.Invalidate();
		}
	}
	else
	{
		// TODO: bail out if remote connection is required for the download,
		// such as when downloading data purely from Jupiter.
		UNSYNC_VERBOSE(L"Attempting to sync without remote server connection");
	}

	LogGlobalStatus(L"Copying files");
	UNSYNC_LOG(L"Copying files ...");

	{
		// Throttle background tasks by trying to keep them to some sensible memory budget. Best effort only, not a hard limit.
		const uint64 BackgroundTaskMemoryBudget	 = SyncOptions.BackgroundTaskMemoryBudget;
		const uint64 TargetTotalSizePerTaskBatch = BackgroundTaskMemoryBudget;
		const uint64 MaxFilesPerTaskBatch		 = 1000;

		UNSYNC_VERBOSE2(L"Background task memory budget: %llu GB", BackgroundTaskMemoryBudget >> 30);

		struct FBackgroundTaskResult
		{
			FPath			TargetFilePath;
			FFileSyncResult SyncResult;
			bool			bIsPartialCopy = false;
		};

		std::deque<FFileSyncTaskBatch> SyncTaskList;

		std::mutex						   BackgroundTaskStatMutex;
		std::vector<FBackgroundTaskResult> BackgroundTaskResults;

		// Tasks are sorted by download size and processed by multiple threads.
		// Large downloads are processed on the foreground thread and small ones on the background.
		std::sort(AllFileTasks.begin(), AllFileTasks.end(), [](const FFileSyncTask& A, const FFileSyncTask& B) {
			return A.NeedBytesFromSource < B.NeedBytesFromSource;
		});

		// Blocks for multiple files can be downloaded in one request.
		// Group small file tasks into batches to reduce the number of individual download requests.
		{
			const uint64 MaxBatchDownloadSize = 4_MB;

			FFileSyncTaskBatch CurrentBatch;
			for (const FFileSyncTask& FileTask : AllFileTasks)
			{
				bool bShouldBreakBatch = false;

				if (!CurrentBatch.FileTasks.empty())
				{
					if (CurrentBatch.NeedBytesFromSource + FileTask.NeedBytesFromSource > MaxBatchDownloadSize)
					{
						bShouldBreakBatch = true;
					}
					else if (CurrentBatch.FileTasks.size() >= MaxFilesPerTaskBatch)
					{
						bShouldBreakBatch = true;
					}
					else if (CurrentBatch.TotalSizeBytes >= TargetTotalSizePerTaskBatch)
					{
						bShouldBreakBatch = true;
					}
				}

				if (bShouldBreakBatch)
				{
					SyncTaskList.push_back(std::move(CurrentBatch));
					CurrentBatch = {};
				}

				CurrentBatch.FileTasks.push_back(&FileTask);
				CurrentBatch.NeedBytesFromSource += FileTask.NeedBytesFromSource;
				CurrentBatch.TotalSizeBytes += FileTask.TotalSizeBytes;
			}

			if (CurrentBatch.FileTasks.size())
			{
				SyncTaskList.push_back(std::move(CurrentBatch));
			}
		}

		// Validate batching
		{
			uint64 TotalSyncSizeBatched = 0;
			uint64 TotalFilesBatched	= 0;
			for (const FFileSyncTaskBatch& Batch : SyncTaskList)
			{
				TotalSyncSizeBatched += Batch.TotalSizeBytes;
				TotalFilesBatched += Batch.FileTasks.size();
			}
			UNSYNC_ASSERT(TotalFilesBatched == AllFileTasks.size());
			UNSYNC_ASSERT(TotalSyncSizeBatched == TotalSyncSizeBytes);
		}

		auto FileSyncTaskBody = [Algorithm,
								 &SyncOptions,
								 &StatSourceBytes,
								 &StatBaseBytes,
								 &BackgroundTaskStatMutex,
								 &BackgroundTaskResults,
								 &NumFailedTasks,
								 &ScavengeDatabase,
								 &ProxyPool](const FFileSyncTask& Item, FBlockCache* BlockCache, bool bBackground) {
			UNSYNC_VERBOSE(L"Copy '%ls' (%ls)", Item.TargetFilePath.wstring().c_str(), (Item.NeedBytesFromBase) ? L"partial" : L"full");

			std::unique_ptr<FNativeFile> BaseFile;
			if (Item.IsBaseValid())
			{
				BaseFile = std::make_unique<FNativeFile>(Item.BaseFilePath, EFileMode::ReadOnlyUnbuffered);
			}

			const FGenericBlockArray& SourceBlocks	  = Item.SourceManifest->Blocks;
			uint32					  SourceBlockSize = Item.SourceManifest->BlockSize;

			FSyncFileOptions SyncFileOptions;
			SyncFileOptions.Algorithm			 = Algorithm;
			SyncFileOptions.BlockSize			 = SourceBlockSize;
			SyncFileOptions.ProxyPool			 = &ProxyPool;
			SyncFileOptions.BlockCache			 = BlockCache;
			SyncFileOptions.ScavengeDatabase	 = ScavengeDatabase.get();
			SyncFileOptions.bValidateTargetFiles = SyncOptions.bValidateTargetFiles;

			FFileSyncResult SyncResult =
				SyncFile(Item.NeedList, Item.ResolvedSourceFilePath, SourceBlocks, *BaseFile.get(), Item.TargetFilePath, SyncFileOptions);

			LogStatus(Item.TargetFilePath.wstring().c_str(), SyncResult.Succeeded() ? L"Succeeded" : L"Failed");

			if (SyncResult.Succeeded())
			{
				StatSourceBytes += SyncResult.SourceBytes;
				StatBaseBytes += SyncResult.BaseBytes;
				UNSYNC_ASSERT(SyncResult.SourceBytes + SyncResult.BaseBytes == Item.TotalSizeBytes);

				if (!GDryRun)
				{
					BaseFile = nullptr;
					SetFileMtime(Item.TargetFilePath, Item.SourceManifest->Mtime);
					if (Item.SourceManifest->bReadOnly)
					{
						SetFileReadOnly(Item.TargetFilePath, true);
					}
				}

				if (bBackground)
				{
					FBackgroundTaskResult Result;
					Result.TargetFilePath = Item.TargetFilePath;
					Result.SyncResult	  = SyncResult;
					Result.bIsPartialCopy = Item.NeedBytesFromBase != 0;

					std::lock_guard<std::mutex> LockGuard(BackgroundTaskStatMutex);
					BackgroundTaskResults.push_back(Result);
				}	
			}
			else
			{
				if (SyncResult.SystemErrorCode.value())
				{
					UNSYNC_ERROR(L"Sync failed from '%ls' to '%ls'. Status: %ls, system error code: %d %hs",
								 Item.ResolvedSourceFilePath.wstring().c_str(),
								 Item.TargetFilePath.wstring().c_str(),
								 ToString(SyncResult.Status),
								 SyncResult.SystemErrorCode.value(),
								 SyncResult.SystemErrorCode.message().c_str());
				}
				else
				{
					UNSYNC_ERROR(L"Sync failed from '%ls' to '%ls'. Status: %ls.",
								 Item.ResolvedSourceFilePath.wstring().c_str(),
								 Item.TargetFilePath.wstring().c_str(),
								 ToString(SyncResult.Status));
				}

				NumFailedTasks++;
			}
		};

		std::atomic<uint64> NumBackgroundTasks = {};
		std::atomic<uint64> NumForegroundTasks = {};

		FTaskGroup BackgroundTaskGroup;
		FTaskGroup ForegroundTaskGroup;

		std::atomic<uint64> BackgroundTaskMemory	   = {};
		std::atomic<uint64> RemainingSourceBytes	   = EstimatedNeedBytesFromSource;

		std::mutex				SchedulerMutex;
		std::condition_variable SchedulerEvent;

		while (!SyncTaskList.empty())
		{
			if (NumForegroundTasks == 0)
			{
				FFileSyncTaskBatch LocalTaskBatch = std::move(SyncTaskList.back());

				SyncTaskList.pop_back();
				++NumForegroundTasks;

				RemainingSourceBytes -= LocalTaskBatch.NeedBytesFromSource;

				ForegroundTaskGroup.run([TaskBatch = std::move(LocalTaskBatch),
										 &SchedulerEvent,
										 &NumForegroundTasks,
										 &FileSyncTaskBody,
										 &ProxyPool,
										 Algorithm,
										 LogVerbose = GLogVerbose]() {
					FLogVerbosityScope VerbosityScope(LogVerbose);

					std::unique_ptr<FBlockCache> BlockCache;
					if (TaskBatch.FileTasks.size() > 1 && ProxyPool.IsValid())
					{
						BlockCache = TaskBatch.CreateBlockCache(ProxyPool, Algorithm.StrongHashAlgorithmId);
					}

					for (const FFileSyncTask* Task : TaskBatch.FileTasks)
					{
						FileSyncTaskBody(*Task, BlockCache.get(), false);
					}

					--NumForegroundTasks;
					SchedulerEvent.notify_one();
				});
				continue;
			}

			const uint32 MaxBackgroundTasks = std::min<uint32>(8, GMaxThreads - 1);

			if (NumBackgroundTasks < MaxBackgroundTasks && (SyncTaskList.front().NeedBytesFromSource < RemainingSourceBytes / 4) &&
				(BackgroundTaskMemory + SyncTaskList.front().TotalSizeBytes < BackgroundTaskMemoryBudget))
			{
				FFileSyncTaskBatch LocalTaskBatch = std::move(SyncTaskList.front());
				SyncTaskList.pop_front();

				BackgroundTaskMemory += LocalTaskBatch.TotalSizeBytes;
				++NumBackgroundTasks;

				RemainingSourceBytes -= LocalTaskBatch.NeedBytesFromSource;

				BackgroundTaskGroup.run([TaskBatch = std::move(LocalTaskBatch),
										 &SchedulerEvent,
										 &NumBackgroundTasks,
										 &FileSyncTaskBody,
										 &BackgroundTaskMemory,
										 &ProxyPool,
										 Algorithm]() {
					FLogVerbosityScope VerbosityScope(false);  // turn off logging from background threads

					std::unique_ptr<FBlockCache> BlockCache;
					if (TaskBatch.FileTasks.size() > 1 && ProxyPool.IsValid())
					{
						BlockCache = TaskBatch.CreateBlockCache(ProxyPool, Algorithm.StrongHashAlgorithmId);
					}

					for (const FFileSyncTask* Task : TaskBatch.FileTasks)
					{
						FileSyncTaskBody(*Task, BlockCache.get(), true);
					}
					BackgroundTaskMemory -= TaskBatch.TotalSizeBytes;
					--NumBackgroundTasks;
					SchedulerEvent.notify_one();
				});

				continue;
			}

			std::unique_lock<std::mutex> SchedulerLock(SchedulerMutex);
			SchedulerEvent.wait(SchedulerLock);
		}

		ForegroundTaskGroup.wait();

		if (NumBackgroundTasks != 0)
		{
			UNSYNC_LOG(L"Waiting for background tasks to complete");
		}
		BackgroundTaskGroup.wait();

		UNSYNC_ASSERT(RemainingSourceBytes == 0);

		bool   bAllBackgroundTasksSucceeded = true;
		uint32 NumBackgroundSyncFiles		= 0;
		uint64 DownloadedBackgroundBytes	= 0;
		for (const FBackgroundTaskResult& Item : BackgroundTaskResults)
		{
			if (Item.SyncResult.Succeeded())
			{
				UNSYNC_VERBOSE2(L"Copied '%ls' (%ls, background)",
								Item.TargetFilePath.wstring().c_str(),
								Item.bIsPartialCopy ? L"partial" : L"full");
				++NumBackgroundSyncFiles;
				DownloadedBackgroundBytes += Item.SyncResult.SourceBytes;
			}
			else
			{
				bAllBackgroundTasksSucceeded = false;
			}
		}

		if (NumBackgroundSyncFiles)
		{
			UNSYNC_VERBOSE(L"Background file copies: %d (%.2f MB)", NumBackgroundSyncFiles, SizeMb(DownloadedBackgroundBytes));
		}

		if (!bAllBackgroundTasksSucceeded)
		{
			for (const FBackgroundTaskResult& Item : BackgroundTaskResults)
			{
				if (!Item.SyncResult.Succeeded())
				{
					UNSYNC_ERROR(L"Failed to copy file '%ls' on background task. Status: %ls, system error code: %d %hs",
								 Item.TargetFilePath.wstring().c_str(),
								 ToString(Item.SyncResult.Status),
								 Item.SyncResult.SystemErrorCode.value(),
								 Item.SyncResult.SystemErrorCode.message().c_str());
				}
			}
			UNSYNC_ERROR(L"Background file copy process failed!");
		}
	}

	const bool bSyncSucceeded = NumFailedTasks.load() == 0;

	if (bSyncSucceeded && SyncOptions.bCleanup)
	{
		UNSYNC_LOG(L"Deleting unnecessary files");
		UNSYNC_LOG_INDENT;
		DeleteUnnecessaryFiles(TargetPath, TargetDirectoryManifest, SourceDirectoryManifest, SyncFilter);
	}

	// Save the source directory manifest on success.
	// It can be used to speed up the diffing process during next sync.
	if (bSyncSucceeded && !GDryRun)
	{
		bool bSaveOk = SaveDirectoryManifest(SourceDirectoryManifest, TargetManifestPath);

		if (!bSaveOk)
		{
			UNSYNC_ERROR(L"Failed to save manifest after sync");
		}
	}

	UNSYNC_LOG(L"Skipped files: %d, full copies: %d, partial copies: %d", StatSkipped, StatFullCopy, StatPartialCopy);
	UNSYNC_LOG(L"Copied from source: %.2f MB, copied from base: %.2f MB", SizeMb(StatSourceBytes), SizeMb(StatBaseBytes));
	UNSYNC_LOG(L"Sync completed %ls", bSyncSucceeded ? L"successfully" : L"with errors (see log for details)");

	double ElapsedSeconds = DurationSec(TimeBegin, TimePointNow());
	UNSYNC_VERBOSE2(L"Sync time: %.2f seconds", ElapsedSeconds);

	if (ProxyPool.IsValid() && ProxyPool.GetFeatures().bTelemetry)
	{
		FTelemetryEventSyncComplete Event;

		Event.ClientVersion		 = GetVersionString();
		Event.Session			 = ProxyPool.GetSessionId();
		Event.Source			 = ConvertWideToUtf8(SourcePath.wstring());
		Event.ClientHostNameHash = GetAnonymizedMachineIdString();
		Event.TotalBytes		 = TotalSourceSize;
		Event.SourceBytes		 = StatSourceBytes;
		Event.BaseBytes			 = StatBaseBytes;
		Event.SkippedFiles		 = StatSkipped;
		Event.FullCopyFiles		 = StatFullCopy;
		Event.PartialCopyFiles	 = StatPartialCopy;
		Event.Elapsed			 = ElapsedSeconds;
		Event.bSuccess			 = bSyncSucceeded;

		ProxyPool.SendTelemetryEvent(Event);
	}

	return bSyncSucceeded;
}

FPath
FSyncFilter::Resolve(const FPath& Filename) const
{
	std::wstring FilenameLower = StringToLower(Filename.wstring());

	FPath Result;
	for (const auto& Alias : DfsAliases)
	{
		// TODO: add a case-insensitive find() helper
		size_t Pos = FilenameLower.find(StringToLower(Alias.Source.wstring()));
		if (Pos == 0)
		{
			auto Tail = (Filename.wstring().substr(Alias.Source.wstring().length() + 1));
			Result	  = Alias.Target / Tail;
			break;
		}
	}

	if (Result.empty())
	{
		Result = Filename;
	}

	return Result;
}

FBuffer
GeneratePatch(const uint8*			 BaseData,
			  uint64				 BaseDataSize,
			  const uint8*			 SourceData,
			  uint64				 SourceDataSize,
			  uint32				 BlockSize,
			  EWeakHashAlgorithmID	 WeakHasher,
			  EStrongHashAlgorithmID StrongHasher,
			  int32					 CompressionLevel)
{
	FBuffer Result;

	FAlgorithmOptions Algorithm;
	Algorithm.ChunkingAlgorithmId	= EChunkingAlgorithmID::FixedBlocks;
	Algorithm.WeakHashAlgorithmId	= WeakHasher;
	Algorithm.StrongHashAlgorithmId = StrongHasher;

	UNSYNC_VERBOSE(L"Computing blocks for source (%.2f MB)", SizeMb(SourceDataSize));
	FGenericBlockArray SourceBlocks = ComputeBlocks(SourceData, SourceDataSize, BlockSize, Algorithm);

	FGenericBlockArray SourceValidation, BaseValidation;

	{
		FLogVerbosityScope VerbosityScope(false);
		SourceValidation = ComputeBlocks(SourceData, SourceDataSize, FPatchHeader::VALIDATION_BLOCK_SIZE, Algorithm);
		BaseValidation	 = ComputeBlocks(BaseData, BaseDataSize, FPatchHeader::VALIDATION_BLOCK_SIZE, Algorithm);
	}

	UNSYNC_VERBOSE(L"Computing difference for base (%.2f MB)", SizeMb(BaseDataSize));
	FNeedList NeedList = DiffBlocks(BaseData, BaseDataSize, BlockSize, WeakHasher, StrongHasher, SourceBlocks);

	if (IsSynchronized(NeedList, SourceBlocks))
	{
		return Result;
	}

	FPatchCommandList PatchCommands;
	PatchCommands.Source = OptimizeNeedList(NeedList.Source);
	PatchCommands.Base	 = OptimizeNeedList(NeedList.Base);

	uint64 NeedFromSource = ComputeSize(NeedList.Source);
	uint64 NeedFromBase	  = ComputeSize(NeedList.Base);
	UNSYNC_VERBOSE(L"Need from source %.2f MB, from base: %.2f MB", SizeMb(NeedFromSource), SizeMb(NeedFromBase));

	FVectorStreamOut Stream(Result);

	FPatchHeader Header;
	Header.SourceSize				 = SourceDataSize;
	Header.BaseSize					 = BaseDataSize;
	Header.NumSourceValidationBlocks = SourceValidation.size();
	Header.NumBaseValidationBlocks	 = BaseValidation.size();
	Header.NumSourceBlocks			 = PatchCommands.Source.size();
	Header.NumBaseBlocks			 = PatchCommands.Base.size();
	Header.BlockSize				 = BlockSize;
	Header.WeakHashAlgorithmId		 = WeakHasher;
	Header.StrongHashAlgorithmId	 = StrongHasher;
	Stream.Write(&Header, sizeof(Header));

	FHash128 HeaderHash = HashBlake3Bytes<FHash128>(Result.Data(), Result.Size());
	Stream.Write(&HeaderHash, sizeof(HeaderHash));

	for (const FGenericBlock& Block : SourceValidation)
	{
		Stream.Write(&Block, sizeof(Block));
	}
	for (const FGenericBlock& Block : BaseValidation)
	{
		Stream.Write(&Block, sizeof(Block));
	}
	for (FCopyCommand& Cmd : PatchCommands.Source)
	{
		Stream.Write(&Cmd, sizeof(Cmd));
	}
	for (FCopyCommand& Cmd : PatchCommands.Base)
	{
		Stream.Write(&Cmd, sizeof(Cmd));
	}

	FHash128 BlockHash = HashBlake3Bytes<FHash128>(Result.Data(), Result.Size());
	Stream.Write(&BlockHash, sizeof(BlockHash));

	for (const FCopyCommand& Cmd : PatchCommands.Source)
	{
		Stream.Write(SourceData + Cmd.SourceOffset, Cmd.Size);
	}

	const uint64 RawPatchSize = Result.Size();
	UNSYNC_VERBOSE(L"Compressing patch (%.2f MB raw)", SizeMb(RawPatchSize));

	Result = Compress(Result.Data(), Result.Size(), CompressionLevel);

	UNSYNC_VERBOSE(L"Compressed patch size: %.2f MB", SizeMb(Result.Size()));

	return Result;
}

FNeedListSize
ComputeNeedListSize(const FNeedList& NeedList)
{
	FNeedListSize Result = {};

	for (const FNeedBlock& Block : NeedList.Base)
	{
		Result.TotalBytes += Block.Size;
		Result.BaseBytes += Block.Size;
	}
	for (const FNeedBlock& Block : NeedList.Source)
	{
		Result.TotalBytes += Block.Size;
		Result.SourceBytes += Block.Size;
	}

	return Result;
}

static void
AddCommaSeparatedWordsToList(const std::wstring& CommaSeparatedWords, std::vector<std::wstring>& Output)
{
	size_t Offset = 0;
	size_t Len	  = CommaSeparatedWords.length();
	while (Offset < Len)
	{
		size_t MatchOffset = CommaSeparatedWords.find(L',', Offset);
		if (MatchOffset == std::wstring::npos)
		{
			MatchOffset = Len;
		}

		std::wstring Word = CommaSeparatedWords.substr(Offset, MatchOffset - Offset);
		Output.push_back(Word);

		Offset = MatchOffset + 1;
	}
}

void
FSyncFilter::IncludeInSync(const std::wstring& CommaSeparatedWords)
{
	AddCommaSeparatedWordsToList(CommaSeparatedWords, SyncIncludedWords);
}

void
FSyncFilter::ExcludeFromSync(const std::wstring& CommaSeparatedWords)
{
	AddCommaSeparatedWordsToList(CommaSeparatedWords, SyncExcludedWords);
}

void
FSyncFilter::ExcludeFromCleanup(const std::wstring& CommaSeparatedWords)
{
	AddCommaSeparatedWordsToList(CommaSeparatedWords, CleanupExcludedWords);
}

bool
FSyncFilter::ShouldSync(const FPath& Filename) const
{
#if UNSYNC_PLATFORM_WINDOWS
	return ShouldSync(Filename.native());
#else
	return ShouldSync(Filename.wstring());
#endif
}

bool
FSyncFilter::ShouldSync(const std::wstring& Filename) const
{
	bool bInclude = SyncIncludedWords.empty();	// Include everything if there are no specific inclusions
	for (const std::wstring& Word : SyncIncludedWords)
	{
		if (Filename.find(Word) != std::wstring::npos)
		{
			bInclude = true;
			break;
		}
	}

	if (!bInclude)
	{
		return false;
	}

	for (const std::wstring& Word : SyncExcludedWords)
	{
		if (Filename.find(Word) != std::wstring::npos)
		{
			return false;
		}
	}

	return true;
}

bool
FSyncFilter::ShouldCleanup(const FPath& Filename) const
{
#if UNSYNC_PLATFORM_WINDOWS
	return ShouldCleanup(Filename.native());
#else
	return ShouldCleanup(Filename.wstring());
#endif
}

bool
FSyncFilter::ShouldCleanup(const std::wstring& Filename) const
{
	for (const std::wstring& Word : CleanupExcludedWords)
	{
		if (Filename.find(Word) != std::wstring::npos)
		{
			return false;
		}
	}

	return true;
}

const wchar_t*
ToString(EFileSyncStatus Status)
{
	switch (Status)
	{
		default:
			return L"UNKNOWN";
		case EFileSyncStatus::Ok:
			return L"Ok";
		case EFileSyncStatus::ErrorUnknown:
			return L"Unknown error";
		case EFileSyncStatus::ErrorFullCopy:
			return L"Full file copy failed";
		case EFileSyncStatus::ErrorValidation:
			return L"Patched file validation failed";
		case EFileSyncStatus::ErrorFinalRename:
			return L"Final file rename failed";
		case EFileSyncStatus::ErrorTargetFileCreate:
			return L"Target file creation failed";
		case EFileSyncStatus::ErrorBuildTargetFailed:
			return L"Failed to build target";
	}
}

THashMap<FGenericHash, FGenericBlock>
BuildBlockMap(const FDirectoryManifest& Manifest, bool bNeedMacroBlocks)
{
	THashMap<FGenericHash, FGenericBlock> Result;
	for (const auto& It : Manifest.Files)
	{
		const FFileManifest& File = It.second;
		if (bNeedMacroBlocks)
		{
			for (const FGenericBlock& Block : File.MacroBlocks)
			{
				Result[Block.HashStrong] = Block;
			}
		}
		else
		{
			for (const FGenericBlock& Block : File.Blocks)
			{
				Result[Block.HashStrong] = Block;
			}
		}
	}
	return Result;
}

void
LogManifestDiff(ELogLevel LogLevel, const FDirectoryManifest& ManifestA, const FDirectoryManifest& ManifestB)
{
	THashMap<FGenericHash, FGenericBlock> BlocksA = BuildBlockMap(ManifestA, false);
	THashMap<FGenericHash, FGenericBlock> BlocksB = BuildBlockMap(ManifestB, false);

	THashMap<FGenericHash, FGenericBlock> MacroBlocksA = BuildBlockMap(ManifestA, true);
	THashMap<FGenericHash, FGenericBlock> MacroBlocksB = BuildBlockMap(ManifestB, true);

	uint32 NumCommonBlocks		= 0;
	uint64 TotalCommonBlockSize = 0;
	uint64 TotalSizeA			= 0;
	uint64 TotalSizeB			= 0;

	uint64 PatchSizeFromAtoB = 0;

	for (const auto& ItA : BlocksA)
	{
		TotalSizeA += ItA.second.Size;
		auto ItB = BlocksB.find(ItA.first);
		if (ItB != BlocksB.end())
		{
			NumCommonBlocks++;
			TotalCommonBlockSize += ItA.second.Size;
		}
	}

	for (const auto& ItB : BlocksB)
	{
		TotalSizeB += ItB.second.Size;
		if (BlocksA.find(ItB.first) == BlocksA.end())
		{
			PatchSizeFromAtoB += ItB.second.Size;
		}
	}

	uint32 NumCommonMacroBlocks		 = 0;
	uint64 TotalCommonMacroBlockSize = 0;
	for (const auto& ItA : MacroBlocksA)
	{
		auto ItB = MacroBlocksB.find(ItA.first);
		if (ItB != MacroBlocksB.end())
		{
			NumCommonMacroBlocks++;
			TotalCommonMacroBlockSize += ItA.second.Size;
		}
	}

	LogPrintf(LogLevel, L"Common macro blocks: %d, %.3f MB\n", NumCommonMacroBlocks, SizeMb(TotalCommonMacroBlockSize));

	LogPrintf(LogLevel,
			  L"Common blocks: %d, %.3f MB (%.2f%% of A, %.2f%% of B)\n",
			  NumCommonBlocks,
			  SizeMb(TotalCommonBlockSize),
			  100.0 * double(TotalCommonBlockSize) / double(TotalSizeA),
			  100.0 * double(TotalCommonBlockSize) / double(TotalSizeB));

	LogPrintf(LogLevel, L"Patch size: %.3f MB\n", SizeMb(PatchSizeFromAtoB));
}

void
FilterManifest(const FSyncFilter& SyncFilter, FDirectoryManifest& Manifest)
{
	auto It = Manifest.Files.begin();
	while (It != Manifest.Files.end())
	{
		if (SyncFilter.ShouldSync(It->first))
		{
			++It;
		}
		else
		{
			It = Manifest.Files.erase(It);
		}
	}
}

int32
CmdInfo(const FCmdInfoOptions& Options)
{
	FPath DirectoryManifestPathA = IsDirectory(Options.InputA) ? (Options.InputA / ".unsync" / "manifest.bin") : Options.InputA;
	FPath DirectoryManifestPathB = IsDirectory(Options.InputB) ? (Options.InputB / ".unsync" / "manifest.bin") : Options.InputB;

	FDirectoryManifest ManifestA;

	bool bManifestAValid = LoadDirectoryManifest(ManifestA, Options.InputA, DirectoryManifestPathA);

	if (!bManifestAValid)
	{
		return 1;
	}

	LogPrintf(ELogLevel::Info, L"Manifest A: %ls\n", DirectoryManifestPathA.wstring().c_str());

	if (Options.SyncFilter)
	{
		FilterManifest(*Options.SyncFilter, ManifestA);
	}

	{
		UNSYNC_LOG_INDENT;
		LogManifestInfo(ELogLevel::Info, ManifestA);
	}

	if (Options.bListFiles)
	{
		UNSYNC_LOG_INDENT;
		LogManifestFiles(ELogLevel::Info, ManifestA);
	}

	if (Options.InputB.empty())
	{
		return 0;
	}

	LogPrintf(ELogLevel::Info, L"\n");

	FDirectoryManifest ManifestB;

	bool bManifestBValid = LoadDirectoryManifest(ManifestB, Options.InputB, DirectoryManifestPathB);

	if (!bManifestBValid)
	{
		return 1;
	}

	LogPrintf(ELogLevel::Info, L"Manifest B: %ls\n", DirectoryManifestPathB.wstring().c_str());

	if (Options.SyncFilter)
	{
		FilterManifest(*Options.SyncFilter, ManifestB);
	}

	{
		UNSYNC_LOG_INDENT;
		LogManifestInfo(ELogLevel::Info, ManifestB);
	}
	if (Options.bListFiles)
	{
		UNSYNC_LOG_INDENT;
		LogManifestFiles(ELogLevel::Info, ManifestB);
	}

	LogPrintf(ELogLevel::Info, L"\n");
	LogPrintf(ELogLevel::Info, L"Difference:\n");

	{
		UNSYNC_LOG_INDENT;
		LogManifestDiff(ELogLevel::Info, ManifestA, ManifestB);
	}

	return 0;
}

FHash256
ComputeSerializedManifestHash(const FDirectoryManifest& Manifest)
{
	FBuffer			 ManifestBuffer;
	FVectorStreamOut ManifestStream(ManifestBuffer);
	bool			 bSerializedOk = SaveDirectoryManifest(Manifest, ManifestStream);
	UNSYNC_ASSERT(bSerializedOk);
	return HashBlake3Bytes<FHash256>(ManifestBuffer.Data(), ManifestBuffer.Size());
}

FHash160
ComputeSerializedManifestHash160(const FDirectoryManifest& Manifest)
{
	return ToHash160(ComputeSerializedManifestHash(Manifest));
}

}  // namespace unsync
