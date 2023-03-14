// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCommon.h"

#include "UnsyncCompression.h"
#include "UnsyncCore.h"
#include "UnsyncFile.h"
#include "UnsyncProxy.h"
#include "UnsyncScan.h"
#include "UnsyncSerialization.h"
#include "UnsyncThread.h"
#include "UnsyncUtil.h"

#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>

UNSYNC_THIRD_PARTY_INCLUDES_START
#include <blake3.h>
#include <md5-sse2.h>
#include <flat_hash_map.hpp>
UNSYNC_THIRD_PARTY_INCLUDES_END

#define UNSYNC_VERSION_STR "1.0.46"

namespace unsync {

bool GDryRun = false;

template<typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
// using HashMap = std::unordered_map<K,V,Hash,Eq>;
using HashMap = ska::flat_hash_map<K, V, Hash, Eq>;

template<typename K, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
// using HashSet = std::unordered_set<K, Hash, Eq>;
using HashSet = ska::flat_hash_set<K, Hash, Eq>;

static const uint32 MAX_ACTIVE_READERS = 64;  // std::max(4, std::thread::hardware_concurrency();

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

// pretend that reading remote files is ~25x slower than local
const uint64 GLOBAL_PROGRESS_SOURCE_SCALE = 25;
const uint64 GLOBAL_PROGRESS_BASE_SCALE	  = 1;

std::atomic<uint64> GGlobalProgressCurrent;
std::atomic<uint64> GGlobalProgressTotal;

inline void
LogGlobalProgress()
{
	LogProgress(nullptr, GGlobalProgressCurrent, GGlobalProgressTotal);
}

enum class ELogProgressUnits
{
	Raw,
	Bytes,
	MB,
	GB,
};

struct FLogProgressScope
{
	UNSYNC_DISALLOW_COPY_ASSIGN(FLogProgressScope)

	FLogProgressScope(uint64 InTotal, ELogProgressUnits InUnits = ELogProgressUnits::Raw, uint64 InPeriodMilliseconds = 500)
	: Current(0)
	, TOTAL(InTotal)
	, PERIOD_MILLISECONDS(InPeriodMilliseconds)
	, UNITS(InUnits)
	, NextProgressLogTime(TimePointNow())
	, bEnabled(true)
	{
		Add(0);
	}

	void Complete() { Add(0, true); }

	void Add(uint64 X, bool bForceComplete = false)
	{
		if (!bEnabled)
		{
			return;
		}

		Current += X;

		std::lock_guard<std::mutex> LockGuard(Mutex);
		const uint64				CurrentClamped = std::min<uint64>(Current, TOTAL);
		const bool					bComplete	   = (CurrentClamped == TOTAL) || bForceComplete;
		if (bEnabled && GLogVerbose && (TimePointNow() > NextProgressLogTime || bComplete))
		{
			const wchar_t* Ending = bComplete ? L"\n" : L"\r";
			switch (UNITS)
			{
				case ELogProgressUnits::GB:
					LogPrintf(ELogLevel::Debug,
							  L"%.2f / %.2f GB%ls",
							  double(CurrentClamped) / double(1_GB),
							  double(TOTAL) / double(1_GB),
							  Ending);
					break;
				case ELogProgressUnits::MB:
					LogPrintf(ELogLevel::Debug,
							  L"%.2f / %.2f MB%ls",
							  double(CurrentClamped) / double(1_MB),
							  double(TOTAL) / double(1_MB),
							  Ending);
					break;
				case ELogProgressUnits::Bytes:
					LogPrintf(ELogLevel::Debug, L"%llu / %llu bytes%ls", (uint64)CurrentClamped, TOTAL, Ending);
					break;
				case ELogProgressUnits::Raw:
				default:
					LogPrintf(ELogLevel::Debug, L"%llu / %llu%ls", (uint64)CurrentClamped, TOTAL, Ending);
					break;
			}

			NextProgressLogTime = TimePointNow() + std::chrono::milliseconds(PERIOD_MILLISECONDS);
			LogGlobalProgress();

			if (bComplete)
			{
				bEnabled = false;
			}
		}
	}

	std::mutex				Mutex;
	std::atomic<uint64>		Current;
	const uint64			TOTAL;
	const uint64			PERIOD_MILLISECONDS;
	const ELogProgressUnits UNITS;
	FTimePoint				NextProgressLogTime;
	std::atomic<bool>		bEnabled;
};

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
FGenericBlockArray
ComputeBlocksVariableT(FIOReader& Reader, uint32 BlockSize, EStrongHashAlgorithmID StrongHasher, FComputeMacroBlockParams* OutMacroBlocks)
{
	const uint64 InputSize = Reader.GetSize();

	const uint32 MinimumBlockSize = ComputeMinVariableBlockSize(BlockSize);
	const uint32 MaximumBlockSize = ComputeMaxVariableBlockSize(BlockSize);

	const uint64 BytesPerTask = std::min<uint64>(256_MB, std::max<uint64>(InputSize, 1ull));  // TODO: handle task boundary overlap
	const uint64 NumTasks	  = DivUp(InputSize, BytesPerTask);

	const uint64 TargetMacroBlockSize	 = OutMacroBlocks ? OutMacroBlocks->TargetBlockSize : 0;
	const uint64 MinimumMacroBlockSize	 = std::max<uint64>(MinimumBlockSize, TargetMacroBlockSize / 8);
	const uint64 MaximumMacroBlockSize	 = OutMacroBlocks ? OutMacroBlocks->MaxBlockSize : 0;
	const uint32 BlocksPerMacroBlock	 = CheckedNarrow(DivUp(TargetMacroBlockSize - MinimumMacroBlockSize, BlockSize));
	const uint32 MacroBlockHashThreshold = BlocksPerMacroBlock ? (0xFFFFFFFF / BlocksPerMacroBlock) : 0;

	struct Task
	{
		uint64			   Offset = 0;
		FGenericBlockArray Blocks;
		FGenericBlockArray MacroBlocks;
	};

	std::vector<Task> Tasks;
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
						 StrongHasher,
						 MinimumBlockSize,
						 MaximumBlockSize,
						 ScanTaskBuffer,
						 TaskIndex,
						 ThisTaskSize,
						 TargetMacroBlockSize,
						 MinimumMacroBlockSize,
						 MaximumMacroBlockSize,
						 MacroBlockHashThreshold]() {
			Task& Task = Tasks[TaskIndex];

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
						   StrongHasher,
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
									  Block.HashStrong = ComputeHash(LastBlockEnd, ThisBlockSize, StrongHasher);

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

											  // Reset macro block state
											  blake3_hasher_init(&MacroBlockHasher);
											  CurrentMacroBlock.Offset += CurrentMacroBlock.Size;
											  CurrentMacroBlock.Size = 0;
										  }
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

	FGenericBlockArray Result;
	for (uint64 I = 0; I < NumTasks; ++I)
	{
		const Task& Task = Tasks[I];
		for (uint64 J = 0; J < Task.Blocks.size(); ++J)
		{
			Result.push_back(Task.Blocks[J]);
		}
	}

	if (OutMacroBlocks)
	{
		for (uint64 I = 0; I < NumTasks; ++I)
		{
			const Task& Task = Tasks[I];
			for (uint64 J = 0; J < Task.MacroBlocks.size(); ++J)
			{
				OutMacroBlocks->Output.push_back(Task.MacroBlocks[J]);
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

	HashSet<FGenericHash> UniqueBlockSet;
	FGenericBlockArray	  UniqueBlocks;
	for (const FGenericBlock& It : Result)
	{
		auto InsertResult = UniqueBlockSet.insert(It.HashStrong);
		if (InsertResult.second)
		{
			if (It.Offset + It.Size < InputSize || Result.size() == 1)
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

	UNSYNC_ASSERT(NumTotalBlocks == Result.size());

	return Result;
}

FGenericBlockArray
ComputeBlocksVariable(FIOReader&				Reader,
					  uint32					BlockSize,
					  EWeakHashAlgorithmID		WeakHasher,
					  EStrongHashAlgorithmID	StrongHasher,
					  FComputeMacroBlockParams* OutMacroBlocks)
{
	switch (WeakHasher)
	{
		case EWeakHashAlgorithmID::Naive:
			return ComputeBlocksVariableT<FRollingChecksum>(Reader, BlockSize, StrongHasher, OutMacroBlocks);
		case EWeakHashAlgorithmID::BuzHash:
			return ComputeBlocksVariableT<FBuzHash>(Reader, BlockSize, StrongHasher, OutMacroBlocks);
		default:
			UNSYNC_FATAL(L"Unsupported weak hash algorithm mode");
			return {};
	}
}

template<typename WeakHasher>
FGenericBlockArray
ComputeBlocksFixedT(FIOReader& Reader, uint32 BlockSize, EStrongHashAlgorithmID StrongHasher, FComputeMacroBlockParams* OutMacroBlocks)
{
	UNSYNC_LOG_INDENT;

	auto TimeBegin = TimePointNow();

	const uint64 NumBlocks = DivUp(Reader.GetSize(), BlockSize);

	FGenericBlockArray Blocks(NumBlocks);
	for (uint64 I = 0; I < NumBlocks; ++I)
	{
		uint64 ChunkSize = CalcChunkSize(I, BlockSize, Reader.GetSize());
		Blocks[I].Offset = I * BlockSize;
		Blocks[I].Size	 = CheckedNarrow(ChunkSize);
	}

	uint64 ReadSize = std::max<uint64>(BlockSize, 8_MB);
	if (OutMacroBlocks)
	{
		UNSYNC_FATAL(L"Macro block generation is not implemented for fixed block mode");
		ReadSize = std::max<uint64>(ReadSize, OutMacroBlocks->TargetBlockSize);
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

			auto ReadCallback = [&NumReadsCompleted, &TaskGroup, &NumBlocksCompleted, &Blocks, &IoSemaphore, StrongHasher, BlockSize](
									FIOBuffer CmdBuffer,
									uint64	  CmdOffset,
									uint64	  CmdReadSize,
									uint64	  CmdUserData) {
				UNSYNC_ASSERT(CmdReadSize);

				TaskGroup.run([&NumReadsCompleted,
							   &NumBlocksCompleted,
							   &Blocks,
							   &IoSemaphore,
							   StrongHasher,
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

						Block.HashStrong = ComputeHash(Buffer + I * BlockSize, Block.Size, StrongHasher);

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

	HashSet<uint32> UniqueHashes;
	for (const auto& It : Blocks)
	{
		UniqueHashes.insert(It.HashWeak);
	}

	return Blocks;
}

FGenericBlockArray
ComputeBlocksFixed(FIOReader&				 Reader,
				   uint32					 BlockSize,
				   EWeakHashAlgorithmID		 WeakHasher,
				   EStrongHashAlgorithmID	 StrongHasher,
				   FComputeMacroBlockParams* OutMacroBlocks)
{
	switch (WeakHasher)
	{
		case EWeakHashAlgorithmID::Naive:
			return ComputeBlocksFixedT<FRollingChecksum>(Reader, BlockSize, StrongHasher, OutMacroBlocks);
		case EWeakHashAlgorithmID::BuzHash:
			return ComputeBlocksFixedT<FBuzHash>(Reader, BlockSize, StrongHasher, OutMacroBlocks);
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
			snprintf(Str, sizeof(Str), "v" UNSYNC_VERSION_STR " [%s:%s]", GitBranch, GitRev);
		}
		else if (strlen(GitRev))
		{
			snprintf(Str, sizeof(Str), "v" UNSYNC_VERSION_STR " [%s]", GitRev);
		}
		else
		{
			snprintf(Str, sizeof(Str), "v" UNSYNC_VERSION_STR);
		}

		return std::string(Str);
	}();

	return Result;
}

FGenericBlockArray
ComputeBlocks(FIOReader& Reader, uint32 BlockSize, FAlgorithmOptions Algorithm, FComputeMacroBlockParams* OutMacroBlocks)
{
	switch (Algorithm.ChunkingAlgorithmId)
	{
		case EChunkingAlgorithmID::FixedBlocks:
			return ComputeBlocksFixed(Reader, BlockSize, Algorithm.WeakHashAlgorithmId, Algorithm.StrongHashAlgorithmId, OutMacroBlocks);
		case EChunkingAlgorithmID::VariableBlocks:
			return ComputeBlocksVariable(Reader, BlockSize, Algorithm.WeakHashAlgorithmId, Algorithm.StrongHashAlgorithmId, OutMacroBlocks);
		default:
			UNSYNC_FATAL(L"Unsupported chunking mode");
			return {};
	}
}

FGenericBlockArray
ComputeBlocks(const uint8* Data, uint64 Size, uint32 BlockSize, FAlgorithmOptions Algorithm, FComputeMacroBlockParams* OutMacroBlocks)
{
	FMemReader DataReader(Data, Size);
	return ComputeBlocks(DataReader, BlockSize, Algorithm, OutMacroBlocks);
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

	HashMap<typename BlockType::StrongHashType, BlockIndexAndCount> BaseBlockMap;
	HashMap<uint64, uint64>											BaseBlockByOffset;

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

	HashSet<uint32, FIdentityHash32>							 SourceWeakHashSet;
	HashSet<FGenericBlock, FBlockStrongHash, FBlockStrongHashEq> SourceStrongHashSet;

	for (uint32 I = 0; I < uint32(SourceBlocks.size()); ++I)
	{
		SourceWeakHashSet.insert(SourceBlocks[I].HashWeak);
		SourceStrongHashSet.insert(SourceBlocks[I]);
	}

	FNeedList NeedList;

	struct FTask
	{
		uint64														 Offset = 0;
		uint64														 Size	= 0;
		std::vector<FHash128>										 Sequence;
		HashSet<FGenericBlock, FBlockStrongHash, FBlockStrongHashEq> BaseStrongHashSet;
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

					const uint32							 MaxWeakHashFalsePositives = 8;
					HashMap<uint32, uint32, FIdentityHash32> WeakHashFalsePositives;
					HashSet<uint32, FIdentityHash32>		 WeakHashBanList;

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

	HashSet<FGenericBlock, FBlockStrongHash, FBlockStrongHashEq> BaseStrongHashSet;

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

	std::sort(Result.begin(), Result.end(), [](const FCopyCommand& A, const FCopyCommand& B) { return A.SourceOffset < B.SourceOffset; });

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

FBuffer
BuildTargetBuffer(const uint8*			 SourceData,
				  uint64				 SourceSize,
				  const uint8*			 BaseData,
				  uint64				 BaseSize,
				  const FNeedList&		 NeedList,
				  EStrongHashAlgorithmID StrongHasher,
				  FProxyPool*			 ProxyPool)
{
	FMemReader SourceReader(SourceData, SourceSize);
	FMemReader BaseReader(BaseData, BaseSize);
	return BuildTargetBuffer(SourceReader, BaseReader, NeedList, StrongHasher, ProxyPool);
}

struct FReadSchedule
{
	std::vector<FCopyCommand> Blocks;
	std::deque<uint64>		  Requests;	 // unique block request indices sorted small to large
};

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

FBuffer
BuildTargetBuffer(FIOReader&			 SourceProvider,
				  FIOReader&			 BaseProvider,
				  const FNeedList&		 NeedList,
				  EStrongHashAlgorithmID StrongHasher,
				  FProxyPool*			 ProxyPool)
{
	FBuffer				Result;
	const FNeedListSize SizeInfo = ComputeNeedListSize(NeedList);
	Result.Resize(SizeInfo.TotalBytes);
	FMemReaderWriter ResultWriter(Result.Data(), Result.Size());
	BuildTarget(ResultWriter, SourceProvider, BaseProvider, NeedList, StrongHasher, ProxyPool);
	return Result;
}

enum EListType
{
	Source,
	Base
};

inline void
AddGlobalProgress(uint64 Size, EListType ListType)
{
	if (ListType == EListType::Source)
	{
		Size *= GLOBAL_PROGRESS_SOURCE_SCALE;
	}
	else
	{
		Size *= GLOBAL_PROGRESS_BASE_SCALE;
	}

	GGlobalProgressCurrent += Size;
}

void
DownloadBlocks(FProxyPool&					  ProxyPool,
			   const std::vector<FNeedBlock>& OriginalUniqueNeedBlocks,
			   const FBlockDownloadCallback&  CompletionCallback)
{
	const uint32		ParentThreadIndent	 = GLogIndent;
	const bool			bParentThreadVerbose = GLogVerbose;
	std::atomic<uint64> NumActiveLogThreads	 = {};

	HashSet<FHash128>			   DownloadedBlocks;
	std::vector<FNeedBlock>		   RemainingNeedBlocks;
	const std::vector<FNeedBlock>* NeedBlocks = &OriginalUniqueNeedBlocks;

	std::atomic<bool> bGotError = false;

	const uint32 MaxAttempts = 30;
	for (uint32 Attempt = 0; Attempt <= MaxAttempts && !bGotError; ++Attempt)
	{
		if (!ProxyPool.IsValid())
		{
			UNSYNC_WARNING(L"FProxy connection is not valid.")
		}

		if (Attempt != 0)
		{
			uint64 NumRemainingBlocks = RemainingNeedBlocks.size();
			UNSYNC_WARNING(L"Failed to download %lld blocks from proxy. Retry attempt %d of %d ...",
						   NumRemainingBlocks,
						   Attempt,
						   MaxAttempts);

			SchedulerSleep(1000);
		}

		struct DownloadBatch
		{
			uint64 Begin;
			uint64 End;
			uint64 SizeBytes;
		};
		std::vector<DownloadBatch> Batches;
		Batches.push_back(DownloadBatch{});

		const uint64 MaxBytesPerBatch = ProxyPool.RemoteDesc.Protocol == EProtocolFlavor::Jupiter ? 16_MB : 128_MB;

		for (uint64 I = 0; I < NeedBlocks->size() && !bGotError; ++I)
		{
			const FNeedBlock& Block = (*NeedBlocks)[I];

			if (Batches.back().SizeBytes + Block.Size < MaxBytesPerBatch)
			{
				Batches.back().End = I + 1;
			}
			else
			{
				UNSYNC_ASSERT(Batches.back().End == I);
				DownloadBatch NewBatch = {};
				NewBatch.Begin		   = I;
				NewBatch.End		   = I + 1;
				NewBatch.SizeBytes	   = 0;
				Batches.push_back(NewBatch);
			}

			Batches.back().SizeBytes += Block.Size;
		}

		UNSYNC_VERBOSE2(L"Download batches: %lld", Batches.size());

		FTaskGroup DownloadTasks;
		std::mutex DownloadedBlocksMutex;

		for (DownloadBatch Batch : Batches)
		{
			ProxyPool.ParallelDownloadSemaphore.Acquire();
			DownloadTasks.run([NeedBlocks,
							   Batch,
							   ParentThreadIndent,
							   bParentThreadVerbose,
							   &DownloadedBlocksMutex,
							   &ProxyPool,
							   &DownloadedBlocks,
							   &CompletionCallback,
							   &NumActiveLogThreads,
							   &bGotError] {
				if (bGotError)
				{
					ProxyPool.ParallelDownloadSemaphore.Release();
					return;
				}

				FThreadElectScope  AllowVerbose(NumActiveLogThreads, bParentThreadVerbose);
				FLogVerbosityScope VerboseScope(AllowVerbose);
				FLogIndentScope	   IndentScope(ParentThreadIndent, true);

				auto Proxy = ProxyPool.Alloc();
				if (Proxy)
				{
					const FNeedBlock*	   Blocks		  = NeedBlocks->data();
					TArrayView<FNeedBlock> NeedBlocksView = {Blocks + Batch.Begin, Blocks + Batch.End};

					FDownloadResult DownloadResult =
						Proxy->Download(NeedBlocksView,
										[&DownloadedBlocksMutex, &CompletionCallback, &DownloadedBlocks](const FDownloadedBlock& Block,
																										 FHash128 BlockHash) {
											{
												std::lock_guard<std::mutex> LockGuard(DownloadedBlocksMutex);
												DownloadedBlocks.insert(BlockHash);
											}
											CompletionCallback(Block, BlockHash);
										});

					if (DownloadResult.IsOk())
					{
						ProxyPool.Dealloc(std::move(Proxy));
					}
					else if (DownloadResult.GetError() == EDownloadRetryMode::Abort)
					{
						bGotError = true;
						ProxyPool.Invalidate();
					}
				}
				ProxyPool.ParallelDownloadSemaphore.Release();
			});
		}

		DownloadTasks.wait();

		if (DownloadedBlocks.size() == OriginalUniqueNeedBlocks.size() || bGotError)
		{
			break;
		}

		RemainingNeedBlocks.clear();
		for (const FNeedBlock& Block : OriginalUniqueNeedBlocks)
		{
			if (DownloadedBlocks.find(Block.Hash.ToHash128()) == DownloadedBlocks.end())  // #wip-widehash
			{
				RemainingNeedBlocks.push_back(Block);
			}
		}
		NeedBlocks = &RemainingNeedBlocks;
	}
}

void
BuildTarget(FIOWriter&			   Result,
			FIOReader&			   Source,
			FIOReader&			   Base,
			const FNeedList&	   NeedList,
			EStrongHashAlgorithmID StrongHasher,
			FProxyPool*			   ProxyPool)
{
	UNSYNC_LOG_INDENT;

	auto TimeBegin = TimePointNow();

	const FNeedListSize SizeInfo = ComputeNeedListSize(NeedList);
	UNSYNC_ASSERT(SizeInfo.TotalBytes == Result.GetSize());

	std::atomic<bool> bGotError = false;

	std::atomic<bool> bBaseDataCopyTaskDone	  = false;
	std::atomic<bool> bSourceDataCopyTaskDone = false;
	std::atomic<bool> bWaitingForBaseData	  = false;

	FSemaphore WriteSemaphore(MAX_ACTIVE_READERS);	// throttle writing tasks to avoid memory bloat
	FTaskGroup WriteTasks;

	// Remember if parent thread has verbose logging and indentation
	const bool	 bAllowVerboseLog = GLogVerbose;
	const uint32 LogIndent		  = GLogIndent;

	auto ProcessNeedList = [bAllowVerboseLog, LogIndent, &Result, &bGotError, &WriteSemaphore, &WriteTasks, &bWaitingForBaseData](
							   FIOReader&					  DataProvider,
							   const std::vector<FNeedBlock>& NeedBlocks,
							   uint64						  TotalCopySize,
							   EListType					  ListType,
							   std::atomic<bool>&			  bCompletionFlag) {
		FLogIndentScope IndentScope(LogIndent, true);
		uint64			ReadBytesTotal = 0;

		const wchar_t* ListName = ListType == EListType::Source ? L"source" : L"base";

		if (!DataProvider.IsValid())
		{
			UNSYNC_ERROR(L"Failed to read blocks from %ls. Stream is invalid.", ListName);
			bGotError = true;
			return;
		}

		if (DataProvider.GetError())
		{
			UNSYNC_ERROR(L"Failed to read blocks from %ls. Error code: %d.", ListName, DataProvider.GetError());
			bGotError = true;
			return;
		}

		UNSYNC_VERBOSE(L"Reading blocks from %ls", ListName);

		FLogProgressScope ProgressLogger(TotalCopySize, ELogProgressUnits::MB);

		FReadSchedule ReadSchedule = BuildReadSchedule(NeedBlocks);

		UNSYNC_LOG_INDENT;
		UNSYNC_VERBOSE(L"Using %d requests for %d total blocks (%.2f MB)",
					   (int)ReadSchedule.Requests.size(),
					   (int)NeedBlocks.size(),
					   SizeMb(TotalCopySize));

		static constexpr uint32 MaxActiveLargeRequests							= 2;
		uint64					ActiveLargeRequestSizes[MaxActiveLargeRequests] = {};

		while (!ReadSchedule.Requests.empty() && !bGotError)
		{
			uint64 BlockIndex;

			uint32 LargeRequestSlot = ~0u;
			for (uint32 I = 0; I < MaxActiveLargeRequests; ++I)
			{
				if (ActiveLargeRequestSizes[I] == 0)
				{
					LargeRequestSlot = I;
					break;
				}
			}

			if (LargeRequestSlot != ~0u)
			{
				BlockIndex = ReadSchedule.Requests.back();
				ReadSchedule.Requests.pop_back();
			}
			else
			{
				BlockIndex = ReadSchedule.Requests.front();
				ReadSchedule.Requests.pop_front();
			}

			const FCopyCommand& Block = ReadSchedule.Blocks[BlockIndex];

			if (LargeRequestSlot != ~0u)
			{
				ActiveLargeRequestSizes[LargeRequestSlot] = Block.Size;
			}

			// UNSYNC_VERBOSE(L"Reading block %d, size %d", (int)block_index, (int)block.size);

			UNSYNC_ASSERT(Block.SourceOffset + Block.Size <= DataProvider.GetSize());
			uint64 ReadBytes = 0;

			auto ReadCallback = [&ReadBytes, &Result, &WriteTasks, &bGotError, &WriteSemaphore, Block, ListType](FIOBuffer CmdBuffer,
																												 uint64	   CmdOffset,
																												 uint64	   CmdReadSize,
																												 uint64	   CmdUserData) {
				WriteSemaphore.Acquire();
				WriteTasks.run([Buffer = MakeShared(std::move(CmdBuffer)), CmdReadSize, Block, &Result, &bGotError, &WriteSemaphore]() {
					uint64 WrittenBytes = Result.Write(Buffer->GetData(), Block.TargetOffset, CmdReadSize);

					WriteSemaphore.Release();

					if (WrittenBytes != CmdReadSize)
					{
						UNSYNC_FATAL(L"Expected to write %llu bytes, but written %llu", CmdReadSize, WrittenBytes);
						bGotError = true;
					}
				});

				ReadBytes = CmdReadSize;

				AddGlobalProgress(CmdReadSize, ListType);
			};

			DataProvider.ReadAsync(Block.SourceOffset, Block.Size, 0, ReadCallback);

			for (uint32 I = 0; I < MaxActiveLargeRequests; ++I)
			{
				if (ActiveLargeRequestSizes[I] == ReadBytes)
				{
					ActiveLargeRequestSizes[I] = 0;
					break;
				}
			}

			ReadBytesTotal += ReadBytes;

			FLogVerbosityScope VerbosityScope(GLogVerbose || (bAllowVerboseLog && (ListType == EListType::Base && bWaitingForBaseData)));

			ProgressLogger.Add(ReadBytes);

			SchedulerYield();
		}

		DataProvider.FlushAll();

		Result.FlushAll();

		ProgressLogger.Complete();

		bCompletionFlag = true;
	};

	FTaskGroup BackgroundTasks;
	BackgroundTasks.run([SizeInfo, ProcessNeedList, &NeedList, &Base, &bBaseDataCopyTaskDone]() {
		// Silence log messages on background task.
		// Logging will be enabled if we're waiting for base data.
		FLogVerbosityScope VerbosityScope(false);

		if (SizeInfo.BaseBytes)
		{
			ProcessNeedList(Base, NeedList.Base, SizeInfo.BaseBytes, EListType::Base, bBaseDataCopyTaskDone);
		}
	});

	if (ProxyPool && ProxyPool->IsValid())
	{
		struct BlockWriteCmd
		{
			uint64 TargetOffset = 0;
			uint64 Size			= 0;
		};
		std::mutex							   DownloadedBlocksMutex;
		HashSet<FHash128>					   DownloadedBlocks;
		HashMap<FHash128, BlockWriteCmd>	   BlockWriteMap;
		HashMap<FHash128, std::vector<uint64>> BlockScatterMap;
		std::vector<FNeedBlock>				   UniqueNeedList;

		uint64 EstimatedDownloadSize = 0;
		for (const FNeedBlock& Block : NeedList.Source)
		{
			BlockWriteCmd Cmd;
			Cmd.TargetOffset  = Block.TargetOffset;
			Cmd.Size		  = Block.Size;
			auto InsertResult = BlockWriteMap.insert(std::make_pair(Block.Hash.ToHash128(), Cmd));	// #wip-widehash

			if (InsertResult.second)
			{
				UniqueNeedList.push_back(Block);
				EstimatedDownloadSize += Block.Size;
			}
			else
			{
				BlockScatterMap[Block.Hash.ToHash128()].push_back(Cmd.TargetOffset);  // #wip-widehash
			}
		}

		UNSYNC_VERBOSE(L"Downloading blocks");
		UNSYNC_LOG_INDENT;

		FTaskGroup DecompressTasks;

		FLogProgressScope DownloadProgressLogger(EstimatedDownloadSize, ELogProgressUnits::MB);

		FSemaphore DecompressionSemaphore(256);	 // limit how many decompression tasks can be queued up to avoid memory bloat

		const bool			bParentThreadVerbose = GLogVerbose;
		const uint32		ParentThreadIndent	 = GLogIndent;
		std::atomic<uint64> NumActiveLogThreads	 = {};
		std::atomic<uint64> NumHashMismatches	 = {};

		DownloadBlocks(
			*ProxyPool,
			UniqueNeedList,
			[&NumActiveLogThreads,
			 &DownloadProgressLogger,
			 &BlockWriteMap,
			 &DecompressionSemaphore,
			 &DecompressTasks,
			 &Result,
			 StrongHasher,
			 &BlockScatterMap,
			 &DownloadedBlocksMutex,
			 &DownloadedBlocks,
			 &bGotError,
			 &NumHashMismatches,
			 &ParentThreadIndent,
			 &bParentThreadVerbose](const FDownloadedBlock& Block, FHash128 BlockHash) {
				FLogIndentScope IndentScope(ParentThreadIndent, true);

				{
					FThreadElectScope  AllowVerbose(NumActiveLogThreads, bParentThreadVerbose);
					FLogVerbosityScope VerboseScope(AllowVerbose);
					DownloadProgressLogger.Add(Block.DecompressedSize);
				}

				auto WriteIt = BlockWriteMap.find(BlockHash);
				if (WriteIt != BlockWriteMap.end())
				{
					BlockWriteCmd Cmd = WriteIt->second;

					// TODO: avoid this copy by storing IOBuffer in DownloadedBlock
					bool	  bCompressed	 = Block.IsCompressed();
					uint64	  DownloadedSize = bCompressed ? Block.CompressedSize : Block.DecompressedSize;
					FIOBuffer DownloadedData = FIOBuffer::Alloc(DownloadedSize, L"downloaded_data");
					memcpy(DownloadedData.GetData(), Block.Data, DownloadedSize);

					DecompressionSemaphore.Acquire();

					DecompressTasks.run([&Result,
										 BlockHashUnaligned = BlockHash,
										 Cmd,
										 DownloadedData	  = std::make_shared<FIOBuffer>(std::move(DownloadedData)),
										 DecompressedSize = Block.DecompressedSize,
										 bCompressed,
										 ParentThreadIndent,
										 StrongHasher,
										 &BlockScatterMap,
										 &DownloadedBlocksMutex,
										 &DownloadedBlocks,
										 &DecompressionSemaphore,
										 &bGotError,
										 &NumHashMismatches]() {
						FLogIndentScope IndentScope(ParentThreadIndent, true);

						// captured objects don't respect large alignment requirements
						FHash128 BlockHash = BlockHashUnaligned;

						bool	  bOk = false;
						FIOBuffer DecompressedData;
						if (DecompressedSize == Cmd.Size)
						{
							if (bCompressed)
							{
								DecompressedData = FIOBuffer::Alloc(Cmd.Size, L"decompressed_data");

								bOk =
									Decompress(DownloadedData->GetData(), DownloadedData->GetSize(), DecompressedData.GetData(), Cmd.Size);
							}
							else
							{
								DecompressedData = std::move(*DownloadedData);

								bOk = true;
							}

							if (bOk)
							{
								FHash128 ActualBlockHash = ComputeHash(DecompressedData.GetData(), Cmd.Size, StrongHasher).ToHash128();

								bOk = ActualBlockHash == BlockHash;

								if (bOk)
								{
									uint64 WrittenBytes = Result.Write(DecompressedData.GetData(), Cmd.TargetOffset, Cmd.Size);
									if (WrittenBytes != Cmd.Size)
									{
										bOk = false;
										UNSYNC_FATAL(L"Expected to write %llu bytes, but written %llu", Cmd.Size, WrittenBytes);
										bGotError = true;
									}
								}
								else
								{
									NumHashMismatches++;
								}
							}
							else
							{
								UNSYNC_FATAL(L"Failed to decompress downloaded block");
								bGotError = true;
							}
						}

						if (bOk)
						{
							auto ScatterIt = BlockScatterMap.find(BlockHash);
							if (ScatterIt != BlockScatterMap.end())
							{
								for (uint64 ScatterOffset : ScatterIt->second)
								{
									uint64 WrittenBytes = Result.Write(DecompressedData.GetData(), ScatterOffset, Cmd.Size);
									if (WrittenBytes != Cmd.Size)
									{
										UNSYNC_FATAL(L"Expected to write %llu bytes, but written %llu", Cmd.Size, WrittenBytes);
										bGotError = true;
									}
								}
							}
						}

						if (bOk)
						{
							std::lock_guard<std::mutex> LockGuard(DownloadedBlocksMutex);
							DownloadedBlocks.insert(BlockHash);

							AddGlobalProgress(Cmd.Size, EListType::Source);
						}

						DecompressionSemaphore.Release();
					});
				}
			});

		DecompressTasks.wait();

		DownloadProgressLogger.Complete();

		if (NumHashMismatches.load() != 0)
		{
			UNSYNC_WARNING(L"Found block hash mismatches while downloading data from proxy. Mismatching blocks: %llu.",
						   NumHashMismatches.load());
		}

		std::vector<FNeedBlock> FilteredNeedList;
		for (const FNeedBlock& Block : NeedList.Source)
		{
			if (DownloadedBlocks.find(Block.Hash.ToHash128()) == DownloadedBlocks.end())  // #wip-widehash
			{
				FilteredNeedList.push_back(Block);
			}
		}

		if (!FilteredNeedList.empty())
		{
			uint64 RemainingBytes = ComputeSize(FilteredNeedList);

			UNSYNC_VERBOSE(L"Did not receive %d blocks from proxy. Must read %.2f MB from source.",
						   FilteredNeedList.size(),
						   SizeMb(RemainingBytes));

			UNSYNC_LOG_INDENT;

			ProcessNeedList(Source, FilteredNeedList, RemainingBytes, EListType::Source, bSourceDataCopyTaskDone);
		}
		else
		{
			bSourceDataCopyTaskDone = true;
		}
	}
	else if (SizeInfo.SourceBytes)
	{
		ProcessNeedList(Source, NeedList.Source, SizeInfo.SourceBytes, EListType::Source, bSourceDataCopyTaskDone);
	}

	if (!bBaseDataCopyTaskDone)
	{
		UNSYNC_VERBOSE(L"Reading blocks from base");
	}

	bWaitingForBaseData = true;
	BackgroundTasks.wait();
	WriteTasks.wait();

	double Duration = DurationSec(TimeBegin, TimePointNow());
	UNSYNC_VERBOSE(L"Done in %.3f sec (%.3f MB / sec)", Duration, SizeMb(double(SizeInfo.TotalBytes) / Duration));

	if (GLogVerbose)
	{
		LogGlobalProgress();
	}
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

FDirectoryManifest
CreateDirectoryManifest(const FPath& Root, uint32 BlockSize, FAlgorithmOptions Algorithm)
{
	UNSYNC_LOG_INDENT;

	FDirectoryManifest Result;

	Result.Options = Algorithm;

	FTimePoint TimeBegin = TimePointNow();

	FTaskGroup	 TaskGroup;
	const uint32 MaxConcurrentFiles = 8;  // quickly diminishing returns past 8 concurrent files
	FSemaphore	 Semaphore(MaxConcurrentFiles);

	std::mutex ResultMutex;
	FPath UnsyncDirName = ".unsync";

	for (const std::filesystem::directory_entry& Dir : RecursiveDirectoryScan(Root))
	{
		if (Dir.is_directory())
		{
			continue;
		}

		FPath RelativePath = std::filesystem::relative(Dir.path(), Root);

		if (RelativePath.native().starts_with(UnsyncDirName.native()))
		{
			continue;
		}

		UNSYNC_VERBOSE2(L"Found '%ls'", RelativePath.wstring().c_str());

		FFileManifest FileManifest;

		FileManifest.Mtime		 = ToWindowsFileTime(Dir.last_write_time());
		FileManifest.Size		 = Dir.file_size();
		FileManifest.CurrentPath = Dir.path();
		FileManifest.BlockSize	 = BlockSize;

		std::wstring Key = RelativePath.wstring();

		{
			std::lock_guard<std::mutex> LockGuard(ResultMutex);
			Result.Files[Key] = std::move(FileManifest);
		}

		if (BlockSize)
		{
			FPath FilePath = Root / RelativePath;
			auto  File	   = std::make_shared<NativeFile>(FilePath, EFileMode::ReadOnlyUnbuffered);
			if (File->IsValid())
			{
				UNSYNC_VERBOSE(L"Computing blocks for '%ls' (%.2f MB)", FilePath.wstring().c_str(), double(File->GetSize()) / (1 << 20));

				Semaphore.Acquire();
				TaskGroup.run([&Semaphore, &ResultMutex, &Result, File = std::move(File), Key = move(Key), BlockSize, Algorithm]() {
					FComputeMacroBlockParams MacroBlocks;

					// TODO: macro block generation is only implemented for variable chunk mode
					const bool bNeedMacroBlocks = Algorithm.ChunkingAlgorithmId == EChunkingAlgorithmID::VariableBlocks;

					FGenericBlockArray Blocks = ComputeBlocks(*File, BlockSize, Algorithm, bNeedMacroBlocks ? &MacroBlocks : nullptr);

					std::lock_guard<std::mutex> LockGuard(ResultMutex);

					std::swap(Result.Files[Key].Blocks, Blocks);

					if (bNeedMacroBlocks)
					{
						std::swap(Result.Files[Key].MacroBlocks, MacroBlocks.Output);
					}

					Semaphore.Release();
				});
			}
			else
			{
				UNSYNC_FATAL(L"Failed to open file '%ls' while computing manifest blocks. System error code: %d.",
							 FilePath.wstring().c_str(),
							 File->GetError());
			}
		}
	}

	TaskGroup.wait();

	if (BlockSize)
	{
		// TODO: move manifest stat printing into a helper

		uint64 NumTotalMacroBlocks = 0;
		uint64 NumTotalBlocks	   = 0;
		uint64 NumTotalBytes	   = 0;

		for (auto& It : Result.Files)
		{
			FFileManifest& FileManifest = It.second;
			NumTotalMacroBlocks += FileManifest.MacroBlocks.size();
			NumTotalBlocks += FileManifest.Blocks.size();
			NumTotalBytes += FileManifest.Size;
		}

		double Duration = DurationSec(TimeBegin, TimePointNow());
		UNSYNC_VERBOSE(L"Total computed %lld block(s) in %.3f sec (%.3f MB, %.3f MB / sec)",
					   (long long)NumTotalBlocks,
					   Duration,
					   SizeMb(NumTotalBytes),
					   SizeMb((double(NumTotalBytes) / Duration)));
		if (NumTotalMacroBlocks)
		{
			UNSYNC_VERBOSE(L"Macro blocks: %lld", NumTotalMacroBlocks);
		}
	}

	return Result;
}

FDirectoryManifest
CreateDirectoryManifestIncremental(const FPath& Root, uint32 BlockSize, FAlgorithmOptions Algorithm)
{
	FPath ManifestRoot			= Root / ".unsync";
	FPath DirectoryManifestPath = ManifestRoot / "manifest.bin";

	FDirectoryManifest OldManifest;
	const bool		   bExistingManifestLoaded = LoadDirectoryManifest(OldManifest, Root, DirectoryManifestPath);

	// Inherit algorithm options from the existing manifest
	if (bExistingManifestLoaded)
	{
		Algorithm = OldManifest.Options;
	}

	// Scan the input directory and gather file metadata, without generating blocks
	FDirectoryManifest NewManifest = CreateDirectoryManifest(Root, 0, Algorithm);

	// Copy file blocks from old manifest, if possible
	for (auto& NewManifestFileEntry : NewManifest.Files)
	{
		const std::wstring& FileName			 = NewManifestFileEntry.first;
		auto				OldManifestFileEntry = OldManifest.Files.find(FileName);
		if (OldManifestFileEntry == OldManifest.Files.end())
		{
			continue;
		}

		FFileManifest& NewEntry = NewManifestFileEntry.second;
		FFileManifest& OldEntry = OldManifestFileEntry->second;

		if (NewEntry.Mtime == OldEntry.Mtime && NewEntry.Size == OldEntry.Size)
		{
			NewEntry.Blocks		 = std::move(OldEntry.Blocks);
			NewEntry.MacroBlocks = std::move(OldEntry.MacroBlocks);
			NewEntry.BlockSize	 = OldEntry.BlockSize;
		}
	}

	// Generate blocks for changed or new files
	UpdateDirectoryManifestBlocks(NewManifest, Root, BlockSize, Algorithm);

	return NewManifest;
}

void
UpdateDirectoryManifestBlocks(FDirectoryManifest& Result, const FPath& Root, uint32 BlockSize, FAlgorithmOptions Algorithm)
{
	UNSYNC_LOG_INDENT;

	UNSYNC_ASSERT(BlockSize != 0);

	FTimePoint TimeBegin = TimePointNow();

	uint32 NumProcessedFiles = 0;

	FTaskGroup TaskGroup;

	const uint32 MaxConcurrentFiles = 8;  // quickly diminishing returns past 8 concurrent files
	FSemaphore	 Semaphore(MaxConcurrentFiles);

	uint64 NumSkippedBlocks = 0;
	uint64 NumSkippedBytes	= 0;

	for (auto& It : Result.Files)
	{
		FFileManifest& FileManifest = It.second;
		if (FileManifest.BlockSize == BlockSize)
		{
			NumSkippedBlocks += FileManifest.Blocks.size();
			NumSkippedBytes += FileManifest.Size;

			continue;
		}

		++NumProcessedFiles;

		FPath FilePath = Root / It.first;
		auto  File	   = std::make_shared<NativeFile>(FilePath, EFileMode::ReadOnlyUnbuffered);
		if (File->IsValid())
		{
			UNSYNC_VERBOSE(L"Computing blocks for '%ls' (%.2f MB)", FilePath.wstring().c_str(), double(File->GetSize()) / (1 << 20));

			Semaphore.Acquire();
			TaskGroup.run([&FileManifest, &Semaphore, File = std::move(File), BlockSize, Algorithm]() {
				FComputeMacroBlockParams MacroBlocks;

				// TODO: macro block generation is only implemented for variable chunk mode
				const bool bNeedMacroBlocks = Algorithm.ChunkingAlgorithmId == EChunkingAlgorithmID::VariableBlocks;

				FileManifest.BlockSize = BlockSize;
				FileManifest.Blocks	   = ComputeBlocks(*File, BlockSize, Algorithm, bNeedMacroBlocks ? &MacroBlocks : nullptr);

				if (bNeedMacroBlocks)
				{
					std::swap(FileManifest.MacroBlocks, MacroBlocks.Output);
				}

				Semaphore.Release();
			});
		}
		else
		{
			UNSYNC_FATAL(L"Failed to open file '%ls' while computing manifest blocks. System error code: %d.",
						 FilePath.wstring().c_str(),
						 File->GetError());
		}
	}

	TaskGroup.wait();

	// TODO: move manifest stat printing into a helper

	uint64 NumTotalMacroBlocks = 0;
	uint64 NumTotalBlocks	   = 0;
	uint64 NumTotalBytes	   = 0;

	for (auto& It : Result.Files)
	{
		FFileManifest& FileManifest = It.second;
		NumTotalMacroBlocks += FileManifest.MacroBlocks.size();
		NumTotalBlocks += FileManifest.Blocks.size();
		NumTotalBytes += FileManifest.Size;
	}

	NumTotalBlocks -= NumSkippedBlocks;
	NumTotalBytes -= NumSkippedBytes;

	double Duration = DurationSec(TimeBegin, TimePointNow());

	if (NumTotalBlocks == 0)
	{
		UNSYNC_VERBOSE(L"No blocks needed to be computed")
	}
	else
	{
		UNSYNC_VERBOSE(L"Total computed %lld block(s) in %.3f sec (%.3f MB, %.3f MB / sec)",
					   (long long)NumTotalBlocks,
					   Duration,
					   SizeMb(NumTotalBytes),
					   SizeMb((double(NumTotalBytes) / Duration)));

		if (NumTotalMacroBlocks)
		{
			UNSYNC_VERBOSE(L"Macro blocks: %lld", NumTotalMacroBlocks);
		}
	}
}

static bool AlgorithmOptionsCompatible(const FAlgorithmOptions& A, const FAlgorithmOptions& B)
{
	return A.StrongHashAlgorithmId == B.StrongHashAlgorithmId
		&& B.WeakHashAlgorithmId == B.WeakHashAlgorithmId
		&& B.ChunkingAlgorithmId == B.ChunkingAlgorithmId;
}

bool
LoadOrCreateDirectoryManifest(FDirectoryManifest& Result, const FPath& Root, uint32 BlockSize, FAlgorithmOptions Algorithm)
{
	UNSYNC_LOG_INDENT;

	FPath ManifestRoot			= Root / ".unsync";
	FPath DirectoryManifestPath = ManifestRoot / "manifest.bin";

	FDirectoryManifest OldDirectoryManifest;
	FDirectoryManifest NewDirectoryManifest;

	const bool bManifestFileExists = PathExists(DirectoryManifestPath);
	if (!bManifestFileExists)
	{
		UNSYNC_VERBOSE(L"Manifest file '%ls' does not exist", DirectoryManifestPath.wstring().c_str());
	}

	const bool bExistingManifestLoaded = bManifestFileExists && LoadDirectoryManifest(OldDirectoryManifest, Root, DirectoryManifestPath);
	const bool bExistingManifestCompatible = AlgorithmOptionsCompatible(OldDirectoryManifest.Options, Algorithm);

	if (bExistingManifestLoaded && bExistingManifestCompatible)
	{
		UNSYNC_VERBOSE(L"Loaded existing manifest from '%ls'", DirectoryManifestPath.wstring().c_str());

		// Verify that manifests match in dry run mode.
		// Otherwise just do a quick manifest generation, without file blocks.
		uint32 NewManifestBlockSize = GDryRun ? BlockSize : 0;

		NewDirectoryManifest = CreateDirectoryManifest(Root, NewManifestBlockSize, Algorithm);
		for (const auto& OldManifestIt : OldDirectoryManifest.Files)
		{
			auto NewManifestIt = NewDirectoryManifest.Files.find(OldManifestIt.first);
			if (NewManifestIt != NewDirectoryManifest.Files.end())
			{
				const FFileManifest& OldFileManifest = OldManifestIt.second;
				FFileManifest&		 NewFileManifest = NewManifestIt->second;
				if (NewFileManifest.Size == OldFileManifest.Size && NewFileManifest.Mtime == OldFileManifest.Mtime)
				{
					if (NewFileManifest.BlockSize)
					{
						UNSYNC_ASSERT(NewFileManifest.BlockSize == OldFileManifest.BlockSize);
						UNSYNC_ASSERT(NewFileManifest.Blocks.size() == OldFileManifest.Blocks.size());
						for (uint64 I = 0; I < NewFileManifest.Blocks.size(); ++I)
						{
							const FGenericBlock& NewBlock = NewFileManifest.Blocks[I];
							const FGenericBlock& OldBlock = OldFileManifest.Blocks[I];
							UNSYNC_ASSERT(NewBlock.Offset == OldBlock.Offset);
							UNSYNC_ASSERT(NewBlock.Size == OldBlock.Size);
							UNSYNC_ASSERT(NewBlock.HashWeak == OldBlock.HashWeak);
							UNSYNC_ASSERT(NewBlock.HashStrong == OldBlock.HashStrong);
						}
					}
					else
					{
						NewFileManifest.BlockSize	= OldFileManifest.BlockSize;
						NewFileManifest.Blocks		= std::move(OldFileManifest.Blocks);
						NewFileManifest.MacroBlocks = std::move(OldFileManifest.MacroBlocks);
					}
				}
			}
		}

		if (BlockSize)
		{
			UpdateDirectoryManifestBlocks(NewDirectoryManifest, Root, BlockSize, Algorithm);
		}
	}
	else
	{
		UNSYNC_VERBOSE(L"Creating manifest for '%ls'", Root.wstring().c_str());
		NewDirectoryManifest = CreateDirectoryManifest(Root, BlockSize, Algorithm);
	}

	std::swap(Result, NewDirectoryManifest);

	return true;
}

template<typename T>
inline auto
UpdateHashT(blake3_hasher& Hasher, const T& V)
{
	blake3_hasher_update(&Hasher, &V, sizeof(T));
}

void
UpdateHashBlocks(blake3_hasher& Hasher, const FGenericBlockArray& Blocks)
{
	for (const FGenericBlock& Block : Blocks)
	{
		UpdateHashT(Hasher, Block.Offset);
		UpdateHashT(Hasher, Block.Size);
		UpdateHashT(Hasher, Block.HashWeak);
		blake3_hasher_update(&Hasher, Block.HashStrong.Data, Block.HashStrong.Size());
	}
}

FHash256
ComputeManifestStableSignature(const FDirectoryManifest& Manifest)
{
	blake3_hasher Hasher;
	blake3_hasher_init(&Hasher);

	UpdateHashT(Hasher, Manifest.Options.ChunkingAlgorithmId);
	UpdateHashT(Hasher, Manifest.Options.WeakHashAlgorithmId);
	UpdateHashT(Hasher, Manifest.Options.StrongHashAlgorithmId);

	std::vector<std::wstring> SortedFiles;
	SortedFiles.reserve(Manifest.Files.size());

	for (const auto& It : Manifest.Files)
	{
		const std::wstring& FileName = It.first;
		SortedFiles.push_back(FileName);
	}

	std::sort(SortedFiles.begin(), SortedFiles.end());

	for (const std::wstring& FileName : SortedFiles)
	{
		// Canonical unsync file paths are utf8 with unix-style separator `/`
		std::string FileNameUtf8 = ConvertWideToUtf8(FileName);
		std::replace(FileNameUtf8.begin(), FileNameUtf8.end(), '\\', '/');

		const FFileManifest& FileManifest = Manifest.Files.at(FileName);

		blake3_hasher_update(&Hasher, FileNameUtf8.c_str(), FileNameUtf8.length());

		UpdateHashT(Hasher, FileManifest.Mtime);
		UpdateHashT(Hasher, FileManifest.Size);
		UpdateHashT(Hasher, FileManifest.BlockSize);

		UpdateHashBlocks(Hasher, FileManifest.Blocks);
		UpdateHashBlocks(Hasher, FileManifest.MacroBlocks);
	}

	FHash256 Result;
	blake3_hasher_finalize(&Hasher, Result.Data, sizeof(Result.Data));

	return Result;
}

FHash160
ComputeManifestStableSignature_160(const FDirectoryManifest& Manifest)
{
	return ToHash160(ComputeManifestStableSignature(Manifest));
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
						UNSYNC_ERROR(L"Found block hash mismatch");
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

	const FileAttributes TargetFileAttributes = GetFileAttrib(TargetFilePath);

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

			auto TargetFile = NativeFile(TargetFilePath, EFileMode::CreateWriteOnly, 0);
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

			TargetFile = std::make_unique<NativeFile>(TempTargetFilePath, EFileMode::CreateWriteOnly, TargetFileSizeInfo.TotalBytes);
			if (TargetFile->GetError() != 0)
			{
				UNSYNC_ERROR(L"Failed to create output file '%ls'. Error code %d.",
							 TempTargetFilePath.wstring().c_str(),
							 TargetFile->GetError());
			}
		}

		FDeferredOpenReader SourceFile([SourceFilePath, TargetFilePath] {
			UNSYNC_VERBOSE(L"Opening source file '%ls'", SourceFilePath.wstring().c_str());
			LogStatus(TargetFilePath.wstring().c_str(), L"Opening source file");

			return std::unique_ptr<NativeFile>(new NativeFile(SourceFilePath, EFileMode::ReadOnlyUnbuffered));
		});

		LogStatus(TargetFilePath.wstring().c_str(), L"Patching");
		BuildTarget(*TargetFile, SourceFile, BaseDataReader, NeedList, Options.Algorithm.StrongHashAlgorithmId, Options.ProxyPool);

		Result.SourceBytes = ComputeSize(NeedList.Source);
		Result.BaseBytes   = ComputeSize(NeedList.Base);

		if (Options.bValidateTargetFiles)
		{
			LogStatus(TargetFilePath.wstring().c_str(), L"Verifying");
			UNSYNC_VERBOSE(L"Verifying patched file '%ls'", TargetFilePath.wstring().c_str());
			UNSYNC_LOG_INDENT;

			if (!GDryRun)
			{
				// Reopen the file in unuffered read mode for optimal reading performance
				TargetFile = nullptr;
				TargetFile = std::make_unique<NativeFile>(TempTargetFilePath, EFileMode::ReadOnlyUnbuffered);
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

			std::error_code Ec = {};
			FileRename(TempTargetFilePath, TargetFilePath, Ec);

			if (Ec.value() == 0)
			{
				Result.Status = EFileSyncStatus::Ok;
			}
			else
			{
				Result.Status		   = EFileSyncStatus::ErrorFinalRename;
				Result.SystemErrorCode = Ec;
			}
		}

		if (Result.Succeeded())
		{
			LogStatus(TargetFilePath.wstring().c_str(), L"Succeeded");
		}
		else
		{
			LogStatus(TargetFilePath.wstring().c_str(), L"Failed");
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

	NativeFile BaseFile(BaseFilePath, EFileMode::ReadOnlyUnbuffered);
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
	FileAttributes	SourceAttr = GetFileAttrib(Source);
	FileAttributes	TargetAttr = GetFileAttrib(Target);
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
// Internally we always perform case-sensitive path comparisons, however on non-case-sensitive filesystems some local files may be renamed to a mismatching case.
// We can update the locally-generated manifest to take the case from the reference manifest for equivalent paths.
// Returns a list of files that should be renamed on disk.
static std::vector<FPendingFileRename>
FixManifestFileNameCases(
	FDirectoryManifest& TargetDirectoryManifest,
	const FDirectoryManifest& ReferenceManifest)
{
	// Build a lookup table of lowercase -> original file names and detect potential case conflicts (which will explode on Windows and Mac)

	std::unordered_map<std::wstring, std::wstring> ReferenceFileNamesLowerCase;
	bool bFoundCaseConflicts = false;
	for (auto& ReferenceManifestEntry : ReferenceManifest.Files)
	{
		std::wstring FileNameLowerCase = StringToLower(ReferenceManifestEntry.first);
		auto InsertResult = ReferenceFileNamesLowerCase.insert(std::pair<std::wstring, std::wstring>(FileNameLowerCase, ReferenceManifestEntry.first));

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
			auto ReferenceIt = ReferenceFileNamesLowerCase.find(TargetFileNameLowerCase);
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
	std::vector<FPendingFileRename> UniqueRenames;
	std::unordered_set<FPath::string_type> UniqueRenamesSet;

	// Build a rename schedule, with only unique entries (taking subdirectories into account)

	for (const FPendingFileRename& Entry : PendingRenames)
	{
		UNSYNC_ASSERTF(StringToLower(Entry.Old) == StringToLower(Entry.New),
			L"FixFileNameCases expects inputs that are different only by case. Old: '%ls', New: '%ls'",
			Entry.Old.c_str(), Entry.New.c_str());

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

	std::sort(UniqueRenames.begin(), UniqueRenames.end(), [](const FPendingFileRename& A, const FPendingFileRename& B)
	{
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

	if (!AlgorithmOptionsCompatible(Existing.Options, Other.Options))
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
			auto LowerCaseEntry = ExistingFileNamesLowerCase.find(OtherNameLowerCase);
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
		if (ReferenceManifest.Files.find(TargetFileName) == ReferenceManifest.Files.end())
		{
			FPath FilePath = TargetDirectory / TargetFileName;

			if (!ShouldCleanup(FilePath))
			{
				UNSYNC_VERBOSE2(L"Skipped deleting '%ls' (excluded by filter)", FilePath.wstring().c_str());
				continue;
			}

			if (GDryRun)
			{
				UNSYNC_VERBOSE(L"Deleting '%ls' (skipped due to dry run mode)", FilePath.wstring().c_str());
			}
			else
			{
				UNSYNC_VERBOSE(L"Deleting '%ls'", FilePath.wstring().c_str());
				std::error_code ErrorCode = {};
				FileRemove(FilePath, ErrorCode);
				if (ErrorCode)
				{
					UNSYNC_VERBOSE(L"System error code %d: %hs", ErrorCode.value(), ErrorCode.message().c_str());
				}
			}
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

static bool
LoadAndMergeSourceManifest(FDirectoryManifest& Output,
						   const FPath&		   SourcePath,
						   const FPath&		   TempPath,
						   FSyncFilter*		   SyncFilter,
						   const FPath&		   SourceManifestOverride,
						   bool				   bCaseSensitiveTargetFileSystem)
{
	auto ResolvePath = [SyncFilter](const FPath& Filename) -> FPath { return SyncFilter ? SyncFilter->Resolve(Filename) : Filename; };

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

	if (!PathExists(SourceManifestPath))
	{
		UNSYNC_ERROR(L"Source manifest '%ls' does not exist", SourceManifestPath.wstring().c_str());
		return false;
	}

	LogGlobalStatus(L"Caching source manifest");

	UNSYNC_VERBOSE(L"Caching source manifest");
	UNSYNC_VERBOSE(L" Source '%ls'", SourceManifestPath.wstring().c_str());
	UNSYNC_VERBOSE(L" Target '%ls'", SourceManifestTempPath.wstring().c_str());
	std::error_code CopyErrorCode = CopyFileIfNewer(SourceManifestPath, SourceManifestTempPath);
	if (CopyErrorCode)
	{
		UNSYNC_LOG(L"Failed to copy manifest '%ls' to '%ls'",
				   SourceManifestPath.wstring().c_str(),
				   SourceManifestTempPath.wstring().c_str());
		UNSYNC_ERROR(L"%hs (%d)", CopyErrorCode.message().c_str(), CopyErrorCode.value());
		return false;
	}

	if (!LoadDirectoryManifest(LoadedManifest, SourcePath, SourceManifestTempPath))
	{
		UNSYNC_ERROR(L"Failed to load source directory manifest '%ls'", SourceManifestPath.wstring().c_str());

		return false;
	}

	if (Output.IsValid() && !AlgorithmOptionsCompatible(Output.Options, LoadedManifest.Options))
	{
		UNSYNC_ERROR(L"Can't merge manifest '%ls' as it uses different algorithm options", SourcePath.wstring().c_str());
		return false;
	}

	return MergeManifests(Output, LoadedManifest, bCaseSensitiveTargetFileSystem);
}

bool  // TODO: return a TResult
SyncDirectory(const FSyncDirectoryOptions& SyncOptions)
{
	FTimePoint TimeBegin = TimePointNow();

	const bool bFileSystemSource = SyncOptions.SourceType == ESyncSourceType::FileSystem;
	const bool bServerSource	 = SyncOptions.SourceType == ESyncSourceType::Server;

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
		UNSYNC_VERBOSE(L"Unnecessary files will be deleted after sync (cleanup mode)");
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

	FRemoteDesc ProxySettings = SyncOptions.Remote ? *SyncOptions.Remote : FRemoteDesc();
	FProxyPool	ProxyPool(ProxySettings);

	FDirectoryManifest SourceDirectoryManifest;
	FPath			   SourceManifestTempPath;

	const bool bCaseSensitiveTargetFileSystem = IsCaseSensitiveFileSystem(TargetTempPath);

	if (bFileSystemSource || !SourceManifestOverride.empty())
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
	else
	{
		if (!ProxyPool.IsValid())
		{
			UNSYNC_ERROR(L"Remote server connection is required when syncing by manifest hash");
			return false;
		}

		UNSYNC_VERBOSE(L"Downloading manifest ...");

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

	{
		UNSYNC_VERBOSE(L"Loaded manifest properties:");
		UNSYNC_LOG_INDENT;
		FDirectoryManifestInfo ManifestInfo = GetManifestInfo(SourceDirectoryManifest);
		LogManifestInfo(ELogLevel::Debug, ManifestInfo);
		if (SyncOptions.Remote->Protocol == EProtocolFlavor::Jupiter && ManifestInfo.NumMacroBlocks == 0)
		{
			UNSYNC_ERROR(L"Manifest must contain macro blocks when using Jupiter");
			return false;
		}
	}

	// Propagate algorithm selection from source
	FAlgorithmOptions  Algorithm			   = SourceDirectoryManifest.Options;
	FDirectoryManifest TargetDirectoryManifest = CreateDirectoryManifest(TargetPath, 0, Algorithm);

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

	struct FileSyncTask
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
	std::deque<FileSyncTask> SyncTaskList;

	LogGlobalStatus(L"Scanning base directory");
	UNSYNC_VERBOSE(L"Scanning base directory");
	FFileAttributeCache BaseAttribCache = CreateFileAttributeCache(BasePath, SyncFilter);
	UNSYNC_VERBOSE(L"Base files: %d", (uint32)BaseAttribCache.Map.size());

	FFileAttributeCache SourceAttribCache;
	if (bFileSystemSource && SyncOptions.bValidateSourceFiles)
	{
		LogGlobalStatus(L"Scanning source directory");
		UNSYNC_VERBOSE(L"Scanning source directory");
		SourceAttribCache = CreateFileAttributeCache(SourcePath, SyncFilter);
	}

	// If variable blocks are used and we already have a manifest file from previous sync,
	// then we can compute difference quickly based only on file timestamps and previously computed chunks.

	FDirectoryManifest BaseDirectoryManifest;
	bool			   bBaseDirectoryManifestValid = false;
	bool			   bQuickDifferencePossible	   = false;

	if (!SyncOptions.bFullDifference && SourceDirectoryManifest.Options.ChunkingAlgorithmId == EChunkingAlgorithmID::VariableBlocks
		&& PathExists(BaseManifestPath))
	{
		bBaseDirectoryManifestValid = LoadDirectoryManifest(BaseDirectoryManifest, BasePath, BaseManifestPath);
		if (bBaseDirectoryManifestValid && AlgorithmOptionsCompatible(SourceDirectoryManifest.Options, TargetDirectoryManifest.Options))
		{
			bQuickDifferencePossible = true;
		}
	}

	if (bQuickDifferencePossible)
	{
		UNSYNC_VERBOSE(L"Quick file difference is allowed (use --full-diff option to override)");
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

		auto TargetManifestIt = TargetDirectoryManifest.Files.find(SourceFilename);
		if (TargetManifestIt != TargetDirectoryManifest.Files.end())
		{
			const FFileManifest& TargetFileManifest = TargetManifestIt->second;

			if (SourceFileManifest.Size == TargetFileManifest.Size && SourceFileManifest.Mtime == TargetFileManifest.Mtime)
			{
				StatSkipped++;
				UNSYNC_VERBOSE2(L"Skipped '%ls' (up to date)", SourceManifestIt.first.c_str());
				continue;
			}
		}

		FPath SourceFilePath = SourceManifestIt.second.CurrentPath;
		FPath BaseFilePath	 = BasePath / ToPath(SourceManifestIt.first);
		FPath TargetFilePath = TargetPath / ToPath(SourceManifestIt.first);

		FPath ResolvedSourceFilePath = ResolvePath(SourceFilePath);

		if (bFileSystemSource && SyncOptions.bValidateSourceFiles)
		{
			FileAttributes SourceFileAttrib = GetFileAttrib(ResolvedSourceFilePath, &SourceAttribCache);

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
			FileAttributes BaseFileAttrib = GetFileAttrib(BaseFilePath, &BaseAttribCache);

			if (!BaseFileAttrib.bValid)
			{
				UNSYNC_VERBOSE2(L"Dirty file: '%ls' (no base data)", SourceFilename.c_str());
				StatFullCopy++;
			}
			else
			{
				UNSYNC_VERBOSE2(L"Dirty file: '%ls'", SourceManifestIt.first.c_str());
				StatPartialCopy++;

				if (bFileSystemSource && SyncOptions.bValidateSourceFiles && !SourceAttribCache.Exists(ResolvedSourceFilePath) &&
					!PathExists(ResolvedSourceFilePath))
				{
					UNSYNC_VERBOSE(L"Source file '%ls' does not exist", SourceFilePath.wstring().c_str());
					continue;
				}
			}

			FileSyncTask Task;
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

			SyncTaskList.push_back(Task);
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
	UNSYNC_VERBOSE(L"Computing difference ...");

	uint64 EstimatedNeedBytesFromSource = 0;
	uint64 EstimatedNeedBytesFromBase	= 0;

	{
		UNSYNC_LOG_INDENT;
		auto TimeDiffBegin = TimePointNow();

		auto DiffTask = [Algorithm, bQuickDifferencePossible](FileSyncTask& Item) {
			FLogVerbosityScope VerbosityScope(false);  // turn off logging from threads

			const FGenericBlockArray& SourceBlocks = Item.SourceManifest->Blocks;

			if (Item.IsBaseValid() && PathExists(Item.BaseFilePath))
			{
				NativeFile BaseFile(Item.BaseFilePath, EFileMode::ReadOnlyUnbuffered);
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

		ParallelForEach(SyncTaskList.begin(), SyncTaskList.end(), DiffTask);

		auto TimeDiffEnd = TimePointNow();

		double Duration = DurationSec(TimeDiffBegin, TimeDiffEnd);
		UNSYNC_VERBOSE(L"Difference complete in %.3f sec", Duration);

		HashSet<FHash128> UniqueSourceBlocks;
		uint64			  UniqueSourceBytes = 0;

		for (FileSyncTask& Item : SyncTaskList)
		{
			EstimatedNeedBytesFromSource += Item.NeedBytesFromSource;
			EstimatedNeedBytesFromBase += Item.NeedBytesFromBase;
			for (const auto& Block : Item.NeedList.Source)
			{
				auto InsertRes = UniqueSourceBlocks.insert(Block.Hash.ToHash128());	 // #wip-widehash
				if (InsertRes.second)
				{
					UniqueSourceBytes += Block.Size;
				}
			}
		}

		UNSYNC_VERBOSE(L"Total need from source: %.2f MB", SizeMb(EstimatedNeedBytesFromSource));
		UNSYNC_VERBOSE(L"Total need from base: %.2f MB", SizeMb(EstimatedNeedBytesFromBase));
	}

	GGlobalProgressCurrent = 0;
	GGlobalProgressTotal =
		EstimatedNeedBytesFromSource * GLOBAL_PROGRESS_SOURCE_SCALE + EstimatedNeedBytesFromBase * GLOBAL_PROGRESS_BASE_SCALE;

	LogGlobalProgress();

	if (ProxySettings.IsValid() && ProxyPool.IsValid())
	{
		LogGlobalStatus(L"Connecting to server");
		UNSYNC_VERBOSE(L"Connecting to %hs server '%hs:%d' ...",
					   ToString(ProxySettings.Protocol),
					   ProxySettings.HostAddress.c_str(),
					   ProxySettings.HostPort);
		UNSYNC_LOG_INDENT;

		std::unique_ptr<FProxy> Proxy = ProxyPool.Alloc();

		if (Proxy.get() && Proxy->IsValid())
		{
			// TODO: report TLS status
			// ESocketSecurity security = proxy->get_socket_security();
			// UNSYNC_VERBOSE(L"Connection established (security: %hs)", ToString(security));

			UNSYNC_VERBOSE(L"Connection established");
			UNSYNC_VERBOSE(L"Building block request map");

			bool bProxyHasData = Proxy->Contains(SourceDirectoryManifest);

			ProxyPool.Dealloc(std::move(Proxy));

			if (bProxyHasData)
			{
				ProxyPool.InitRequestMap(SourceDirectoryManifest.Options.StrongHashAlgorithmId);

				for (const FileSyncTask& Task : SyncTaskList)
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

	if (ProxySettings.IsValid() && !ProxyPool.IsValid())
	{
		// TODO: bail out if remote connection is required for the download,
		// such as when downloading data purely from Jupiter.
		UNSYNC_WARNING(L"Attempting to sync without remote server connection");
	}

	LogGlobalStatus(L"Copying files");
	UNSYNC_VERBOSE(L"Copying files");

	{
		struct BackgroundTaskResult
		{
			FPath			TargetFilePath;
			FFileSyncResult SyncResult;
			bool			bIsPartialCopy = false;
		};

		std::mutex						  BackgroundTaskStatMutex;
		std::vector<BackgroundTaskResult> BackgroundTaskResults;

		// Tasks are sorted by download size and processed by multiple threads.
		// Large downloads are processed on the foreground thread and small ones on the background.
		std::sort(SyncTaskList.begin(), SyncTaskList.end(), [](const FileSyncTask& A, const FileSyncTask& B) {
			return A.NeedBytesFromSource < B.NeedBytesFromSource;
		});

		auto SyncTaskBody = [Algorithm,
							 &SyncOptions,
							 &StatSourceBytes,
							 &StatBaseBytes,
							 &BackgroundTaskStatMutex,
							 &BackgroundTaskResults,
							 &NumFailedTasks,
							 &ProxyPool](const FileSyncTask& Item, bool bBackground) {
			UNSYNC_VERBOSE(L"Copy '%ls' (%ls)", Item.TargetFilePath.wstring().c_str(), (Item.NeedBytesFromBase) ? L"partial" : L"full");

			std::unique_ptr<NativeFile> BaseFile;
			if (Item.IsBaseValid())
			{
				BaseFile = std::make_unique<NativeFile>(Item.BaseFilePath, EFileMode::ReadOnlyUnbuffered);
			}

			const FGenericBlockArray& SourceBlocks	  = Item.SourceManifest->Blocks;
			uint32					  SourceBlockSize = Item.SourceManifest->BlockSize;

			FSyncFileOptions SyncFileOptions;
			SyncFileOptions.Algorithm			 = Algorithm;
			SyncFileOptions.BlockSize			 = SourceBlockSize;
			SyncFileOptions.ProxyPool			 = &ProxyPool;
			SyncFileOptions.bValidateTargetFiles = SyncOptions.bValidateTargetFiles;

			FFileSyncResult SyncResult =
				SyncFile(Item.NeedList, Item.ResolvedSourceFilePath, SourceBlocks, *BaseFile.get(), Item.TargetFilePath, SyncFileOptions);

			StatSourceBytes += SyncResult.SourceBytes;
			StatBaseBytes += SyncResult.BaseBytes;
			UNSYNC_ASSERT(SyncResult.SourceBytes + SyncResult.BaseBytes == Item.TotalSizeBytes);

			if (SyncResult.Succeeded() && !GDryRun)
			{
				BaseFile = nullptr;
				SetFileMtime(Item.TargetFilePath, Item.SourceManifest->Mtime);
			}

			if (bBackground)
			{
				BackgroundTaskResult Result;
				Result.TargetFilePath = Item.TargetFilePath;
				Result.SyncResult	  = SyncResult;
				Result.bIsPartialCopy = Item.NeedBytesFromBase != 0;

				std::lock_guard<std::mutex> LockGuard(BackgroundTaskStatMutex);
				BackgroundTaskResults.push_back(Result);
			}

			if (!SyncResult.Succeeded())
			{
				UNSYNC_ERROR(L"Sync failed from '%ls' to '%ls'. Status: %ls, system error code: %d",
							 Item.ResolvedSourceFilePath.wstring().c_str(),
							 Item.TargetFilePath.wstring().c_str(),
							 ToString(SyncResult.Status),
							 SyncResult.SystemErrorCode.value());

				NumFailedTasks++;
			}
		};

		std::atomic<uint64> NumBackgroundTasks = {};
		std::atomic<uint64> NumForegroundTasks = {};

		FTaskGroup BackgroundTaskGroup;
		FTaskGroup ForegroundTaskGroup;

		// Throttle background tasks by trying to keep them to some sensible memory budget. Best effort only, not a hard limit.
		const uint64		BackgroundTaskMemoryBudget = 2_GB;
		std::atomic<uint64> BackgroundTaskMemory	   = {};
		std::atomic<uint64> RemainingSourceBytes	   = EstimatedNeedBytesFromSource;

		std::mutex SchedulerMutex;
		std::condition_variable SchedulerEvent;

		while (!SyncTaskList.empty())
		{
			if (NumForegroundTasks == 0)
			{
				FileSyncTask LocalTask = std::move(SyncTaskList.back());
				SyncTaskList.pop_back();
				++NumForegroundTasks;

				RemainingSourceBytes -= LocalTask.NeedBytesFromSource;

				ForegroundTaskGroup.run(
					[Task = std::move(LocalTask), &SchedulerEvent, &NumForegroundTasks, &SyncTaskBody, LogVerbose = GLogVerbose]() {
						FLogVerbosityScope VerbosityScope(LogVerbose);
						SyncTaskBody(Task, false);
						--NumForegroundTasks;
						SchedulerEvent.notify_one();
					});
				continue;
			}

			const uint32 MaxBackgroundTasks = std::min<uint32>(8, GMaxThreads - 1);

			if (NumBackgroundTasks < MaxBackgroundTasks && (SyncTaskList.front().NeedBytesFromSource < RemainingSourceBytes / 4) &&
				(BackgroundTaskMemory + SyncTaskList.front().TotalSizeBytes < BackgroundTaskMemoryBudget))
			{
				FileSyncTask LocalTask = std::move(SyncTaskList.front());
				SyncTaskList.pop_front();

				BackgroundTaskMemory += LocalTask.TotalSizeBytes;
				++NumBackgroundTasks;

				RemainingSourceBytes -= LocalTask.NeedBytesFromSource;

				BackgroundTaskGroup.run(
					[Task = std::move(LocalTask), &SchedulerEvent, &NumBackgroundTasks, &SyncTaskBody, &BackgroundTaskMemory]() {
						FLogVerbosityScope VerbosityScope(false);  // turn off logging from background threads
						SyncTaskBody(Task, true);
						--NumBackgroundTasks;
						BackgroundTaskMemory -= Task.TotalSizeBytes;
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
			UNSYNC_VERBOSE(L"Waiting for background tasks to complete");
		}
		BackgroundTaskGroup.wait();

		UNSYNC_ASSERT(RemainingSourceBytes == 0);

		bool bAllBackgroundTasksSucceeded = true;
		uint32 NumBackgroundSyncFiles = 0;
		uint64 DownloadedBackgroundBytes = 0;
		for (const BackgroundTaskResult& Item : BackgroundTaskResults)
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
			for (const BackgroundTaskResult& Item : BackgroundTaskResults)
			{
				if (!Item.SyncResult.Succeeded())
				{
					UNSYNC_ERROR(L"Failed to copy file '%ls' on background task. Status: %ls, system error code: %d",
								 Item.TargetFilePath.wstring().c_str(),
								 ToString(Item.SyncResult.Status),
								 Item.SyncResult.SystemErrorCode.value());
				}
			}
			UNSYNC_ERROR(L"Background file copy process failed!");
		}
	}

	const bool bSyncSucceeded = NumFailedTasks.load() == 0;

	if (bSyncSucceeded && SyncOptions.bCleanup)
	{
		UNSYNC_VERBOSE(L"Deleting unnecessary files");
		UNSYNC_LOG_INDENT;
		DeleteUnnecessaryFiles(TargetPath, TargetDirectoryManifest, SourceDirectoryManifest, SyncFilter);
	}

	// Save the source directory manifest on success.
	// It can be used to speed up the diffing process during next sync.
	if (bFileSystemSource && bSyncSucceeded && !GDryRun)
	{
		bool bSaveOk = SaveDirectoryManifest(SourceDirectoryManifest, TargetManifestPath);

		if (!bSaveOk)
		{
			UNSYNC_ERROR(L"Failed to save manifest after sync");
		}
	}

	UNSYNC_VERBOSE(L"Skipped files: %d, full copies: %d, partial copies: %d", StatSkipped, StatFullCopy, StatPartialCopy);
	UNSYNC_VERBOSE(L"Copied from source: %.2f MB, copied from base: %.2f MB", SizeMb(StatSourceBytes), SizeMb(StatBaseBytes));
	UNSYNC_VERBOSE(L"Sync completed %ls", bSyncSucceeded ? L"successfully" : L"with errors (see log for details)");

	double ElapsedSeconds = DurationSec(TimeBegin, TimePointNow());
	UNSYNC_VERBOSE2(L"Sync time: %.2f seconds", ElapsedSeconds);

	if (ProxyPool.IsValid() && ProxyPool.GetFeatures().bTelemetry)
	{
		FTelemetryEventSyncComplete Event;

		Event.ClientVersion	   = GetVersionString();
		Event.Session		   = ProxyPool.GetSessionId();
		Event.Source		   = ConvertWideToUtf8(SourcePath.wstring());
		Event.TotalBytes	   = TotalSourceSize;
		Event.SourceBytes	   = StatSourceBytes;
		Event.BaseBytes		   = StatBaseBytes;
		Event.SkippedFiles	   = StatSkipped;
		Event.FullCopyFiles	   = StatFullCopy;
		Event.PartialCopyFiles = StatPartialCopy;
		Event.Elapsed		   = ElapsedSeconds;
		Event.bSuccess		   = bSyncSucceeded;

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

struct FPatchHeader
{
	static constexpr uint64 VALIDATION_BLOCK_SIZE = 16_MB;

	static constexpr uint64 MAGIC	= 0x3E63942C4C9ECE16ull;
	static constexpr uint64 VERSION = 2;

	uint64				   Magic					 = MAGIC;
	uint64				   Version					 = VERSION;
	uint64				   SourceSize				 = 0;
	uint64				   BaseSize					 = 0;
	uint64				   NumSourceValidationBlocks = 0;
	uint64				   NumBaseValidationBlocks	 = 0;
	uint64				   NumSourceBlocks			 = 0;
	uint64				   NumBaseBlocks			 = 0;
	uint64				   BlockSize				 = 0;
	EWeakHashAlgorithmID   WeakHashAlgorithmId		 = EWeakHashAlgorithmID::Naive;
	EStrongHashAlgorithmID StrongHashAlgorithmId	 = EStrongHashAlgorithmID::Blake3_128;
};

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

template<typename BlockType>
static bool
BlocksEqual(const std::vector<BlockType>& A, std::vector<BlockType>& B)
{
	if (A.size() != B.size())
	{
		return false;
	}

	for (uint64 I = 0; I < A.size(); ++I)
	{
		if (A[I].HashStrong != B[I].HashStrong || A[I].HashWeak != B[I].HashWeak || A[I].Size != B[I].Size || A[I].Offset != B[I].Offset)
		{
			return false;
		}
	}

	return true;
}

struct FMemStream
{
	FMemStream(const uint8* InData, uint64 InDataSize) : Data(InData), DataRw(nullptr), SIZE(InDataSize), Offset(0) {}

	FMemStream(uint8* InData, uint64 InDataSize) : Data(InData), DataRw(InData), SIZE(InDataSize), Offset(0) {}

	bool Read(void* Dest, uint64 ReadSize)
	{
		if (Offset + ReadSize <= SIZE)
		{
			uint8* DestBytes = reinterpret_cast<uint8*>(Dest);
			memcpy(DestBytes, Data + Offset, ReadSize);
			Offset += ReadSize;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool Write(const void* Source, uint64 WriteSize)
	{
		if (Offset + WriteSize <= SIZE)
		{
			const uint8* SourceBytes = reinterpret_cast<const uint8*>(Source);
			memcpy(DataRw + Offset, SourceBytes, WriteSize);
			Offset += WriteSize;
			return true;
		}
		else
		{
			return false;
		}
	}

	const uint8* Data;
	uint8*		 DataRw;
	const uint64 SIZE;
	uint64		 Offset;
};

FBuffer
BuildTargetWithPatch(const uint8* PatchData, uint64 PatchSize, const uint8* BaseData, uint64 BaseDataSize)
{
	const EChunkingAlgorithmID ChunkingMode = EChunkingAlgorithmID::FixedBlocks;

	FBuffer Result;

	FBuffer	   DecompressedPatch = Decompress(PatchData, PatchSize);
	FMemStream Stream(DecompressedPatch.Data(), DecompressedPatch.Size());

	FPatchHeader Header;
	Stream.Read(&Header, sizeof(Header));

	if (Header.Magic != FPatchHeader::MAGIC)
	{
		UNSYNC_ERROR(L"Patch file header mismatch. Expected %llX, got %llX", (long long)FPatchHeader::MAGIC, (long long)Header.Magic);
		return Result;
	}

	if (Header.Version != FPatchHeader::VERSION)
	{
		UNSYNC_ERROR(L"Patch file version mismatch. Expected %lld, got %lld", (long long)FPatchHeader::VERSION, (long long)Header.Version);
		return Result;
	}

	FHash128 ExpectedHeaderHash = HashBlake3Bytes<FHash128>(Stream.Data, Stream.Offset);
	FHash128 HeaderHash;
	Stream.Read(&HeaderHash, sizeof(HeaderHash));
	if (HeaderHash != ExpectedHeaderHash)
	{
		UNSYNC_ERROR(L"Patch header hash mismatch");
		return Result;
	}

	FGenericBlockArray SourceValidationExpected(Header.NumSourceValidationBlocks);
	FGenericBlockArray BaseValidationExpected(Header.NumBaseValidationBlocks);

	Stream.Read(&SourceValidationExpected[0], SourceValidationExpected.size() * sizeof(SourceValidationExpected[0]));
	Stream.Read(&BaseValidationExpected[0], BaseValidationExpected.size() * sizeof(BaseValidationExpected[0]));

	FGenericBlockArray BaseValidation;

	{
		FLogVerbosityScope VerbosityScope(false);
		FAlgorithmOptions  Algorithm;
		Algorithm.ChunkingAlgorithmId	= ChunkingMode;
		Algorithm.WeakHashAlgorithmId	= Header.WeakHashAlgorithmId;
		Algorithm.StrongHashAlgorithmId = Header.StrongHashAlgorithmId;

		BaseValidation = ComputeBlocks(BaseData, BaseDataSize, FPatchHeader::VALIDATION_BLOCK_SIZE, Algorithm);
	}

	if (!BlocksEqual(BaseValidation, BaseValidationExpected))
	{
		UNSYNC_ERROR(L"Base file mismatch");
		return Result;
	}

	FPatchCommandList PatchCommands;
	PatchCommands.Source.resize(Header.NumSourceBlocks);
	PatchCommands.Base.resize(Header.NumBaseBlocks);

	Stream.Read(&PatchCommands.Source[0], Header.NumSourceBlocks * sizeof(PatchCommands.Source[0]));
	Stream.Read(&PatchCommands.Base[0], Header.NumBaseBlocks * sizeof(PatchCommands.Base[0]));

	FHash128 ExpectedBlockHash = HashBlake3Bytes<FHash128>(Stream.Data, Stream.Offset);
	FHash128 BlockHash;
	Stream.Read(&BlockHash, sizeof(BlockHash));
	if (BlockHash != ExpectedBlockHash)
	{
		UNSYNC_ERROR(L"Patch block hash mismatch");
		return Result;
	}

	const uint8* SourceData		= Stream.Data + Stream.Offset;
	uint64		 SourceDataSize = Stream.SIZE - Stream.Offset;

	// TODO: generate the proper source need list from the start
	uint64 SourceOffset = 0;
	for (FCopyCommand& Block : PatchCommands.Source)
	{
		Block.SourceOffset = SourceOffset;
		SourceOffset += Block.Size;
	}

	FNeedList NeedList;
	NeedList.Base.reserve(PatchCommands.Base.size());
	NeedList.Source.reserve(PatchCommands.Source.size());
	NeedList.Sequence.reserve(PatchCommands.Sequence.size());

	for (const FCopyCommand& Cmd : PatchCommands.Base)
	{
		FNeedBlock Block;
		static_cast<FCopyCommand&>(Block) = Cmd;
		NeedList.Base.push_back(Block);
	}

	for (const FCopyCommand& Cmd : PatchCommands.Source)
	{
		FNeedBlock Block;
		static_cast<FCopyCommand&>(Block) = Cmd;
		NeedList.Source.push_back(Block);
	}

	for (const FHash128& Hash : PatchCommands.Sequence)
	{
		NeedList.Sequence.push_back(Hash);
	}

	Result = BuildTargetBuffer(SourceData, SourceDataSize, BaseData, BaseDataSize, NeedList, Header.StrongHashAlgorithmId);

	FGenericBlockArray SourceValidation;

	{
		FLogVerbosityScope VerbosityScope(false);

		FAlgorithmOptions Algorithm;
		Algorithm.ChunkingAlgorithmId	= ChunkingMode;
		Algorithm.WeakHashAlgorithmId	= Header.WeakHashAlgorithmId;
		Algorithm.StrongHashAlgorithmId = Header.StrongHashAlgorithmId;

		SourceValidation = ComputeBlocks(Result.Data(), Result.Size(), FPatchHeader::VALIDATION_BLOCK_SIZE, Algorithm);
	}

	if (!BlocksEqual(SourceValidation, SourceValidationExpected))
	{
		UNSYNC_ERROR(L"Patched size file mismatch");
		Result.Clear();
		return Result;
	}

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
	bool bInclude = SyncIncludedWords.empty(); // Include everything if there are no specific inclusions
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
	}
}

FDirectoryManifestInfo
GetManifestInfo(const FDirectoryManifest& Manifest)
{
	FDirectoryManifestInfo Result = {};

	Result.NumBlocks	  = 0;
	Result.NumMacroBlocks = 0;
	Result.TotalSize	  = 0;

	for (const auto& It : Manifest.Files)
	{
		const FFileManifest& File = It.second;
		Result.NumBlocks += File.Blocks.size();
		Result.NumMacroBlocks += File.MacroBlocks.size();
		Result.TotalSize += File.Size;
	}

	Result.NumFiles		  = Manifest.Files.size();
	Result.Algorithm	  = Manifest.Options;
	Result.SerializedHash = ComputeSerializedManifestHash(Manifest);
	Result.Signature	  = ComputeManifestStableSignature(Manifest);

	return Result;
}

void
LogManifestInfo(ELogLevel LogLevel, const FDirectoryManifestInfo& Info)
{
	FHash160	ManifestSignature = ToHash160(Info.Signature);
	std::string SignatureHexStr	  = HashToHexString(ManifestSignature);

	LogPrintf(LogLevel, L"Manifest signature: %hs\n", SignatureHexStr.c_str());
	LogPrintf(LogLevel, L"Chunking mode: %hs\n", ToString(Info.Algorithm.ChunkingAlgorithmId));
	LogPrintf(LogLevel, L"Weak hash: %hs\n", ToString(Info.Algorithm.WeakHashAlgorithmId));
	LogPrintf(LogLevel, L"Strong hash: %hs\n", ToString(Info.Algorithm.StrongHashAlgorithmId));
	LogPrintf(LogLevel, L"Files: %llu\n", llu(Info.NumFiles));
	LogPrintf(LogLevel, L"Blocks: %llu\n", llu(Info.NumBlocks));
	LogPrintf(LogLevel, L"Macro blocks: %llu\n", llu(Info.NumMacroBlocks));
	LogPrintf(LogLevel, L"Total data size: %.2f MB (%llu bytes)\n", SizeMb(Info.TotalSize), llu(Info.TotalSize));

	// TODO: block size distribution histogram
}

void
LogManifestInfo(ELogLevel LogLevel, const FDirectoryManifest& Manifest)
{
	FDirectoryManifestInfo Info = GetManifestInfo(Manifest);
	LogManifestInfo(LogLevel, Info);
}

void
LogManifestFiles(ELogLevel LogLevel, const FDirectoryManifest& Manifest)
{
	std::vector<std::wstring> Files;
	for (const auto& ManifestIt : Manifest.Files)
	{
		Files.push_back(ManifestIt.first);
	}

	std::sort(Files.begin(), Files.end());

	for (const std::wstring& Filename : Files)
	{
		const FFileManifest& Info = Manifest.Files.at(Filename);
		LogPrintf(LogLevel, L"%s : %llu\n", Filename.c_str(), Info.Size);
	}
}

HashMap<FGenericHash, FGenericBlock>
BuildBlockMap(const FDirectoryManifest& Manifest, bool bNeedMacroBlocks)
{
	HashMap<FGenericHash, FGenericBlock> Result;
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
	HashMap<FGenericHash, FGenericBlock> BlocksA = BuildBlockMap(ManifestA, false);
	HashMap<FGenericHash, FGenericBlock> BlocksB = BuildBlockMap(ManifestB, false);

	HashMap<FGenericHash, FGenericBlock> MacroBlocksA = BuildBlockMap(ManifestA, true);
	HashMap<FGenericHash, FGenericBlock> MacroBlocksB = BuildBlockMap(ManifestB, true);

	uint32 NumCommonBlocks		= 0;
	uint64 TotalCommonBlockSize = 0;
	for (const auto& ItA : BlocksA)
	{
		auto ItB = BlocksB.find(ItA.first);
		if (ItB != BlocksB.end())
		{
			NumCommonBlocks++;
			TotalCommonBlockSize += ItA.second.Size;
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

	LogPrintf(LogLevel, L"Common blocks: %d, %.2f MB\n", NumCommonBlocks, SizeMb(TotalCommonBlockSize));
	LogPrintf(LogLevel, L"Common macro blocks: %d, %.2f MB\n", NumCommonMacroBlocks, SizeMb(TotalCommonMacroBlockSize));
}

int32
CmdInfo(const FPath& InputA, const FPath& InputB, bool bListFiles)
{
	FPath DirectoryManifestPathA = InputA / ".unsync" / "manifest.bin";
	FPath DirectoryManifestPathB = InputB / ".unsync" / "manifest.bin";

	FDirectoryManifest ManifestA;
	bool			   bManifestAValid = LoadDirectoryManifest(ManifestA, InputA, DirectoryManifestPathA);

	if (!bManifestAValid)
	{
		return 1;
	}

	LogPrintf(ELogLevel::Info, L"Manifest A: %ls\n", DirectoryManifestPathA.wstring().c_str());

	{
		UNSYNC_LOG_INDENT;
		LogManifestInfo(ELogLevel::Info, ManifestA);
	}

	if (bListFiles)
	{
		UNSYNC_LOG_INDENT;
		LogManifestFiles(ELogLevel::Info, ManifestA);
	}

	if (InputB.empty())
	{
		return 0;
	}

	LogPrintf(ELogLevel::Info, L"\n");

	FDirectoryManifest ManifestB;
	bool			   bManifestBValid = LoadDirectoryManifest(ManifestB, InputB, DirectoryManifestPathB);
	if (!bManifestBValid)
	{
		return 1;
	}

	LogPrintf(ELogLevel::Info, L"Manifest B: %ls\n", DirectoryManifestPathB.wstring().c_str());

	{
		UNSYNC_LOG_INDENT;
		LogManifestInfo(ELogLevel::Info, ManifestB);
	}
	if (bListFiles)
	{
		UNSYNC_LOG_INDENT;
		LogManifestFiles(ELogLevel::Info, ManifestB);
	}

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
ComputeSerializedManifestHash_160(const FDirectoryManifest& Manifest)
{
	return ToHash160(ComputeSerializedManifestHash(Manifest));
}

}  // namespace unsync
