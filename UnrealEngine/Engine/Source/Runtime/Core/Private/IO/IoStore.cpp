// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoStore.h"

#include "Async/Async.h"
#include "Async/AsyncFileHandle.h"
#include "Async/Future.h"
#include "Async/ParallelFor.h"
#include "Containers/Map.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataPluginInterface.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoDirectoryIndex.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/Compression.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Serialization/BufferWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryWriter.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"

DEFINE_LOG_CATEGORY(LogIoStore);

//////////////////////////////////////////////////////////////////////////

//TRACE_DECLARE_INT_COUNTER(IoStoreAvailableCompressionBuffers, TEXT("IoStore/AvailableCompressionBuffers"));

template<typename ArrayType>
bool WriteArray(IFileHandle* FileHandle, const ArrayType& Array)
{
	return FileHandle->Write(reinterpret_cast<const uint8*>(Array.GetData()), Array.GetTypeSize() * Array.Num());
}

static IEngineCrypto* GetEngineCrypto()
{
	static bool bFeaturesInitialized = false;
	static TArray<IEngineCrypto*> Features;
	if (!bFeaturesInitialized)
	{
		IModularFeatures::FScopedLockModularFeatureList ScopedLockModularFeatureList;
		// bFeaturesInitialized is not atomic so it can have potentially become true since we last checked (except now we're under a lock: IModularFeatures::FScopedLockModularFeatureList, so it will be fine now):
		if (!bFeaturesInitialized)
		{
		    Features = IModularFeatures::Get().GetModularFeatureImplementations<IEngineCrypto>(IEngineCrypto::GetFeatureName());
		    bFeaturesInitialized = true;
		}
	}
	checkf(Features.Num() > 0, TEXT("RSA functionality was used but no modular feature was registered to provide it. Please make sure your project has the PlatformCrypto plugin enabled!"));
	return Features[0];
}

static bool IsSigningEnabled()
{
#if UE_BUILD_SHIPPING
	return FCoreDelegates::GetPakSigningKeysDelegate().IsBound();
#else
	return false;
#endif
}

static FRSAKeyHandle GetPublicSigningKey()
{
	static FRSAKeyHandle PublicKey = InvalidRSAKeyHandle;
	static bool bInitializedPublicKey = false;
	if (!bInitializedPublicKey)
	{
		TDelegate<void(TArray<uint8>&, TArray<uint8>&)>& Delegate = FCoreDelegates::GetPakSigningKeysDelegate();
		if (Delegate.IsBound())
		{
			TArray<uint8> Exponent;
			TArray<uint8> Modulus;
			Delegate.Execute(Exponent, Modulus);
			PublicKey = GetEngineCrypto()->CreateRSAKey(Exponent, TArray<uint8>(), Modulus);
		}
		bInitializedPublicKey = true;
	}

	return PublicKey;
}

static FIoStatus CreateContainerSignature(
	const FRSAKeyHandle PrivateKey,
	const FIoStoreTocHeader& TocHeader,
	TArrayView<const FSHAHash> BlockSignatureHashes,
	TArray<uint8>& OutTocSignature,
	TArray<uint8>& OutBlockSignature)
{
	if (PrivateKey == InvalidRSAKeyHandle)
	{
		return FIoStatus(EIoErrorCode::SignatureError, TEXT("Invalid signing key"));
	}

	FSHAHash TocHash, BlocksHash;

	FSHA1::HashBuffer(reinterpret_cast<const uint8*>(&TocHeader), sizeof(FIoStoreTocHeader), TocHash.Hash);
	FSHA1::HashBuffer(BlockSignatureHashes.GetData(), BlockSignatureHashes.Num() * sizeof(FSHAHash), BlocksHash.Hash);

	int32 BytesEncrypted = GetEngineCrypto()->EncryptPrivate(TArrayView<const uint8>(TocHash.Hash, UE_ARRAY_COUNT(FSHAHash::Hash)), OutTocSignature, PrivateKey);

	if (BytesEncrypted < 1)
	{
		return FIoStatus(EIoErrorCode::SignatureError, TEXT("Failed to encrypt TOC signature"));
	}

	BytesEncrypted = GetEngineCrypto()->EncryptPrivate(TArrayView<const uint8>(BlocksHash.Hash, UE_ARRAY_COUNT(FSHAHash::Hash)), OutBlockSignature, PrivateKey);

	return BytesEncrypted > 0 ? FIoStatus::Ok : FIoStatus(EIoErrorCode::SignatureError, TEXT("Failed to encrypt block signature"));
}

static FIoStatus ValidateContainerSignature(
	const FRSAKeyHandle PublicKey,
	const FIoStoreTocHeader& TocHeader,
	TArrayView<const FSHAHash> BlockSignatureHashes,
	TArrayView<const uint8> TocSignature,
	TArrayView<const uint8> BlockSignature)
{
	if (PublicKey == InvalidRSAKeyHandle)
	{
		return FIoStatus(EIoErrorCode::SignatureError, TEXT("Invalid signing key"));
	}

	TArray<uint8> DecryptedTocHash, DecryptedBlocksHash;

	int32 BytesDecrypted = GetEngineCrypto()->DecryptPublic(TocSignature, DecryptedTocHash, PublicKey);
	if (BytesDecrypted != UE_ARRAY_COUNT(FSHAHash::Hash))
	{
		return FIoStatus(EIoErrorCode::SignatureError, TEXT("Failed to decrypt TOC signature"));
	}

	BytesDecrypted = GetEngineCrypto()->DecryptPublic(BlockSignature, DecryptedBlocksHash, PublicKey);
	if (BytesDecrypted != UE_ARRAY_COUNT(FSHAHash::Hash))
	{
		return FIoStatus(EIoErrorCode::SignatureError, TEXT("Failed to decrypt block signature"));
	}

	FSHAHash TocHash, BlocksHash;
	FSHA1::HashBuffer(reinterpret_cast<const uint8*>(&TocHeader), sizeof(FIoStoreTocHeader), TocHash.Hash);
	FSHA1::HashBuffer(BlockSignatureHashes.GetData(), BlockSignatureHashes.Num() * sizeof(FSHAHash), BlocksHash.Hash);

	if (FMemory::Memcmp(DecryptedTocHash.GetData(), TocHash.Hash, DecryptedTocHash.Num()) != 0)
	{
		return FIoStatus(EIoErrorCode::SignatureError, TEXT("Invalid TOC signature"));
	}

	if (FMemory::Memcmp(DecryptedBlocksHash.GetData(), BlocksHash.Hash, DecryptedBlocksHash.Num()) != 0)
	{
		return FIoStatus(EIoErrorCode::SignatureError, TEXT("Invalid block signature"));
	}

	return FIoStatus::Ok;
}

struct FChunkBlock
{
	const uint8* UncompressedData = nullptr;
	FIoBuffer* IoBuffer = nullptr;

	// This is the size of the actual block after encryption alignment, and is
	// set in EncryptAndSign. This happens whether or not the container is encrypted.
	uint64 Size = 0;
	uint64 CompressedSize = 0;
	uint64 UncompressedSize = 0;
	FName CompressionMethod = NAME_None;
	FSHAHash Signature;

	/** Hash of the block data as it would be found on disk - this includes encryption alignment padding */
	FIoHash DiskHash;
};

struct FIoStoreWriteQueueEntry
{
	FIoStoreWriteQueueEntry* Next = nullptr;
	class FIoStoreWriter* Writer = nullptr;
	IIoStoreWriteRequest* Request = nullptr;
	FIoChunkId ChunkId;
	FIoChunkHash ChunkHash;
	/** Hash of the block data as it would be found on disk after compression and encryption */
	FIoHash ChunkDiskHash;

	uint64 Sequence = 0;
	
	// We make this optional because at the latest it might not be valid until FinishCompressionBarrior
	// completes and we'd like to have a check() on that.
	TOptional<uint64> UncompressedSize = 0;

	// this is not filled out until after encryption completes and *includes the alignment padding for encryption*!
	// think of this as "size on disk".
	uint64 CompressedSize = 0; 

	uint64 Padding = 0;
	uint64 Offset = 0;
	TArray<FChunkBlock> ChunkBlocks;
	FIoWriteOptions Options;
	FGraphEventRef HashBarrier;
	FGraphEventRef HashTask;
	FGraphEventRef BeginCompressionBarrier;
	FGraphEventRef FinishCompressionBarrier;
	FGraphEventRef FinishEncryptionAndSigningBarrier;
	FGraphEventRef BeginOnDemandDataBarrier;
	FGraphEventRef BeginWriteBarrier;
	FGraphEventRef WriteFinishedEvent;
	TAtomic<int32> CompressedBlocksCount{ 0 };
	TAtomic<int32> FinishedBlocksCount{ 0 };
	int32 PartitionIndex = -1;
	FString DDCCacheKey;
	bool bAdded = false;
	bool bModified = false;
	bool bStoreCompressedDataInDDC = false;
	
	bool bCouldBeFromReferenceDb = false; // Whether the chunk is a valid candidate for the reference db.
	bool bLoadingFromReferenceDb = false;
	// When we know we're loading from the reference chunk db, we don't read the source
	// buffer but we still need to know the number of chunks which we get from the refdb.
	uint32 NumChunkBlocksFromRefDb = 0; 
};

class FIoStoreWriteQueue
{
public:
	FIoStoreWriteQueue()
		: Event(FPlatformProcess::GetSynchEventFromPool(false))
	{ }
	
	~FIoStoreWriteQueue()
	{
		check(Head == nullptr && Tail == nullptr);
		FPlatformProcess::ReturnSynchEventToPool(Event);
	}

	void Enqueue(FIoStoreWriteQueueEntry* Entry)
	{
		check(!bIsDoneAdding);
		{
			FScopeLock _(&CriticalSection);

			if (!Tail)
			{
				Head = Tail = Entry;
			}
			else
			{
				Tail->Next = Entry;
				Tail = Entry;
			}
			Entry->Next = nullptr;
		}

		Event->Trigger();
	}

	FIoStoreWriteQueueEntry* DequeueOrWait()
	{
		for (;;)
		{
			{
				FScopeLock _(&CriticalSection);
				if (Head)
				{
					FIoStoreWriteQueueEntry* Entry = Head;
					Head = Tail = nullptr;
					return Entry;
				}
			}

			if (bIsDoneAdding)
			{
				break;
			}

			Event->Wait();
		}

		return nullptr;
	}

	void CompleteAdding()
	{
		bIsDoneAdding = true;
		Event->Trigger();
	}

	bool IsEmpty() const
	{
		FScopeLock _(&CriticalSection);
		return Head == nullptr;
	}

private:
	mutable FCriticalSection CriticalSection;
	FEvent* Event = nullptr;
	FIoStoreWriteQueueEntry* Head = nullptr;
	FIoStoreWriteQueueEntry* Tail = nullptr;
	TAtomic<bool> bIsDoneAdding { false };
};

class FIoStoreWriterContextImpl
{
	static constexpr uint64 DefaultMemoryLimit = 2ull << 30ull;
public:
	FIoStoreWriterContextImpl()
	{
	}

	~FIoStoreWriterContextImpl()
	{
		BeginCompressionQueue.CompleteAdding();
		BeginEncryptionAndSigningQueue.CompleteAdding();
		FinishEncryptionAndSigningQueue.CompleteAdding();
		OnDemandDataQueue.CompleteAdding();
		WriterQueue.CompleteAdding();
		BeginCompressionThread.Wait();
		BeginEncryptionAndSigningThread.Wait();
		FinishEncryptionAndSigningThread.Wait();
		BeginOnDemandDataThread.Wait();
		WriterThread.Wait();
		if (CompressionBufferAvailableEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(CompressionBufferAvailableEvent);
		}
		check(AvailableCompressionBuffers.Num() == TotalCompressionBufferCount);
		for (FIoBuffer* IoBuffer : AvailableCompressionBuffers)
		{
			delete IoBuffer;
		}
	}

	[[nodiscard]] FIoStatus Initialize(const FIoStoreWriterSettings& InWriterSettings)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FIoStoreWriterContext::Initialize);
		WriterSettings = InWriterSettings;
		CompressionBufferAvailableEvent = FPlatformProcess::GetSynchEventFromPool(false);

		if (WriterSettings.bCompressionEnableDDC)
		{
			DDC = GetDerivedDataCache();
		}

		if (WriterSettings.CompressionMethod != NAME_None)
		{
			CompressionBufferSize = FCompression::CompressMemoryBound(WriterSettings.CompressionMethod, static_cast<int32>(WriterSettings.CompressionBlockSize));
		}
		CompressionBufferSize = FMath::Max(CompressionBufferSize, static_cast<int32>(WriterSettings.CompressionBlockSize));
		CompressionBufferSize = Align(CompressionBufferSize, FAES::AESBlockSize);

		TotalCompressionBufferCount = int32(DefaultMemoryLimit / CompressionBufferSize);
		AvailableCompressionBuffers.Reserve(TotalCompressionBufferCount);
		for (int32 BufferIndex = 0; BufferIndex < TotalCompressionBufferCount; ++BufferIndex)
		{
			AvailableCompressionBuffers.Add(new FIoBuffer(CompressionBufferSize));
		}
		//TRACE_COUNTER_SET(IoStoreAvailableCompressionBuffers, AvailableCompressionBuffers.Num());

		BeginCompressionThread = Async(EAsyncExecution::Thread, [this]() { BeginCompressionThreadFunc(); });
		BeginEncryptionAndSigningThread = Async(EAsyncExecution::Thread, [this]() { BeginEncryptionAndSigningThreadFunc(); });
		FinishEncryptionAndSigningThread = Async(EAsyncExecution::Thread, [this]() { FinishEncryptionAndSigningThreadFunc(); });
		BeginOnDemandDataThread = Async(EAsyncExecution::Thread, [this]() { BeginOnDemandDataThreadFunc(); });
		WriterThread = Async(EAsyncExecution::Thread, [this]() { WriterThreadFunc(); });

		return FIoStatus::Ok;
	}

	TSharedPtr<IIoStoreWriter> CreateContainer(const TCHAR* InContainerPathAndBaseFileName, const FIoContainerSettings& InContainerSettings);

	void Flush();

	FIoStoreWriterContext::FProgress GetProgress() const
	{
		FIoStoreWriterContext::FProgress Progress;
		Progress.HashDbChunksCount = HashDbChunksCount.Load();
		for (uint8 i=0; i<(uint8)EIoChunkType::MAX; i++)
		{
			Progress.HashDbChunksByType[i] = HashDbChunksByType[i].Load();
		}

		Progress.TotalChunksCount = TotalChunksCount.Load();
		Progress.HashedChunksCount = HashedChunksCount.Load();
		Progress.CompressedChunksCount = CompressedChunksCount.Load();
		for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
		{
			Progress.CompressedChunksByType[i] = CompressedChunksByType[i].Load();
		}
		for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
		{
			Progress.BeginCompressChunksByType[i] = BeginCompressChunksByType[i].Load();
		}
		Progress.SerializedChunksCount = SerializedChunksCount.Load();
		Progress.ScheduledCompressionTasksCount = ScheduledCompressionTasksCount.Load();
		Progress.CompressionDDCHitCount = CompressionDDCHitCount.Load();
		Progress.CompressionDDCMissCount = CompressionDDCMissCount.Load();
		Progress.RefDbChunksCount = RefDbChunksCount.Load();
		for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
		{
			Progress.RefDbChunksByType[i] = RefDbChunksByType[i].Load();
		}

		return Progress;
	}

	const FIoStoreWriterSettings& GetSettings() const
	{
		return WriterSettings;
	}

	void ScheduleCompression(FIoStoreWriteQueueEntry* QueueEntry)
	{
		BeginCompressionQueue.Enqueue(QueueEntry);
		if (QueueEntry->bLoadingFromReferenceDb == false)
		{
			QueueEntry->Request->PrepareSourceBufferAsync(QueueEntry->BeginCompressionBarrier);
		}
		else
		{
			// We don't need to wait on a read so we can kick directly.
			QueueEntry->BeginCompressionBarrier->DispatchSubsequents();
		}
	}

	//
	// This must be called prior to the consumer being dispatched in order to prevent resource
	// contention deadlock (even if using retracting waits)
	//
	FIoBuffer* AllocCompressionBuffer(int32 TotalEntryChunkBlocksCount)
	{
		if (TotalEntryChunkBlocksCount > TotalCompressionBufferCount)
		{
			return new FIoBuffer(CompressionBufferSize);
		}
		FIoBuffer* AllocatedBuffer = nullptr;
		while (!AllocatedBuffer)
		{
			{
				FScopeLock Lock(&AvailableCompressionBuffersCritical);
				if (AvailableCompressionBuffers.Num())
				{
					AllocatedBuffer = AvailableCompressionBuffers.Pop();
					//TRACE_COUNTER_DECREMENT(IoStoreAvailableCompressionBuffers);
				}
			}
			if (!AllocatedBuffer)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(WaitForCompressionBuffer);
				CompressionBufferAvailableEvent->Wait();
			}
		}
		return AllocatedBuffer;
	}

	void FreeCompressionBuffer(FIoBuffer* Buffer, int32 TotalEntryChunkBlocksCount)
	{
		if (TotalEntryChunkBlocksCount > TotalCompressionBufferCount)
		{
			delete Buffer;
			return;
		}
		bool bTriggerEvent;
		{
			FScopeLock Lock(&AvailableCompressionBuffersCritical);
			bTriggerEvent = AvailableCompressionBuffers.Num() == 0;
			AvailableCompressionBuffers.Push(Buffer);
			//TRACE_COUNTER_INCREMENT(IoStoreAvailableCompressionBuffers);
		}
		if (bTriggerEvent)
		{
			CompressionBufferAvailableEvent->Trigger();
		}
	}

private:
	void BeginCompressionThreadFunc();
	void BeginEncryptionAndSigningThreadFunc();
	void FinishEncryptionAndSigningThreadFunc();
	void BeginOnDemandDataThreadFunc();
	void WriterThreadFunc();

	FIoStoreWriterSettings WriterSettings;
	FDerivedDataCacheInterface* DDC = nullptr;
	FEvent* CompressionBufferAvailableEvent = nullptr;
	TFuture<void> BeginCompressionThread;
	TFuture<void> BeginEncryptionAndSigningThread;
	TFuture<void> FinishEncryptionAndSigningThread;
	TFuture<void> BeginOnDemandDataThread;
	TFuture<void> WriterThread;
	FIoStoreWriteQueue BeginCompressionQueue;
	FIoStoreWriteQueue BeginEncryptionAndSigningQueue;
	FIoStoreWriteQueue FinishEncryptionAndSigningQueue;
	FIoStoreWriteQueue OnDemandDataQueue;
	FIoStoreWriteQueue WriterQueue;
	TAtomic<uint64> TotalChunksCount{ 0 };
	TAtomic<uint64> HashedChunksCount{ 0 };
	TAtomic<uint64> HashDbChunksCount{ 0 };
	TAtomic<uint64> HashDbChunksByType[(int8)EIoChunkType::MAX] = {0};
	TAtomic<uint64> RefDbChunksCount{ 0 };
	TAtomic<uint64> RefDbChunksByType[(int8)EIoChunkType::MAX] = { 0 };
	TAtomic<uint64> CompressedChunksCount{ 0 };
	TAtomic<uint64> CompressedChunksByType[(int8)EIoChunkType::MAX] = { 0 };
	TAtomic<uint64> BeginCompressChunksByType[(int8)EIoChunkType::MAX] = { 0 };
	TAtomic<uint64> SerializedChunksCount{ 0 };
	TAtomic<uint64> WriteCycleCount{ 0 };
	TAtomic<uint64> WriteByteCount{ 0 };
	TAtomic<uint64> ScheduledCompressionTasksCount{ 0 };
	TAtomic<uint64> CompressionDDCHitCount{ 0 };
	TAtomic<uint64> CompressionDDCMissCount{ 0 };
	FCriticalSection AvailableCompressionBuffersCritical;
	TArray<FIoBuffer*> AvailableCompressionBuffers;
	int32 CompressionBufferSize = -1;
	int32 TotalCompressionBufferCount = -1;
	TArray<TSharedPtr<FIoStoreWriter>> IoStoreWriters;

	friend class FIoStoreWriter;
};

FIoStoreWriterContext::FIoStoreWriterContext()
	: Impl(new FIoStoreWriterContextImpl())
{

}

FIoStoreWriterContext::~FIoStoreWriterContext()
{
	delete Impl;
}

[[nodiscard]] FIoStatus FIoStoreWriterContext::Initialize(const FIoStoreWriterSettings& InWriterSettings)
{
	return Impl->Initialize(InWriterSettings);
}

TSharedPtr<IIoStoreWriter> FIoStoreWriterContext::CreateContainer(const TCHAR* InContainerPath, const FIoContainerSettings& InContainerSettings)
{
	return Impl->CreateContainer(InContainerPath, InContainerSettings);
}

void FIoStoreWriterContext::Flush()
{
	Impl->Flush();
}

FIoStoreWriterContext::FProgress FIoStoreWriterContext::GetProgress() const
{
	return Impl->GetProgress();
}

class FIoStoreToc
{
public:
	FIoStoreToc()
	{
		FMemory::Memzero(&Toc.Header, sizeof(FIoStoreTocHeader));
	}

	void Initialize()
	{
		ChunkIdToIndex.Empty(Toc.ChunkIds.Num());

		for (int32 ChunkIndex = 0; ChunkIndex < Toc.ChunkIds.Num(); ++ChunkIndex)
		{
			ChunkIdToIndex.Add(Toc.ChunkIds[ChunkIndex], ChunkIndex);
		}
	}

	int32 AddChunkEntry(const FIoChunkId& ChunkId, const FIoOffsetAndLength& OffsetLength, const FIoStoreTocEntryMeta& Meta, const FIoStoreTocOnDemandChunkMeta& OnDemandData)
	{
		int32& Index = ChunkIdToIndex.FindOrAdd(ChunkId);

		if (!Index)
		{
			Index = Toc.ChunkIds.Add(ChunkId);
			Toc.ChunkOffsetLengths.Add(OffsetLength);
			Toc.ChunkMetas.Add(Meta);
			Toc.OnDemandChunkMeta.Add(OnDemandData);

			return Index;
		}

		return INDEX_NONE;
	}

	FIoStoreTocCompressedBlockEntry& AddCompressionBlockEntry()
	{
		return Toc.CompressionBlocks.AddDefaulted_GetRef();
	}

	FSHAHash& AddBlockSignatureEntry()
	{
		return Toc.ChunkBlockSignatures.AddDefaulted_GetRef();
	}

	FIoStoreTocOnDemandCompressedBlockMeta& AddCompressionBlockMetaEntry()
	{
		return Toc.OnDemandCompressedBlockMeta.AddDefaulted_GetRef();
	}

	uint8 AddCompressionMethodEntry(FName CompressionMethod)
	{
		if (CompressionMethod == NAME_None)
		{
			return 0;
		}

		uint8 Index = 1;
		for (const FName& Name : Toc.CompressionMethods)
		{
			if (Name == CompressionMethod)
			{
				return Index;
			}
			++Index;
		}

		return 1 + uint8(Toc.CompressionMethods.Add(CompressionMethod));
	}

	void AddToFileIndex(const FIoChunkId& ChunkId, const FString& FileName)
	{
		ChunkIdToFileName.Emplace(ChunkId, FileName);
	}

	FIoStoreTocResource& GetTocResource()
	{
		return Toc;
	}

	const FIoStoreTocResource& GetTocResource() const
	{
		return Toc;
	}

	const int32* GetTocEntryIndex(const FIoChunkId& ChunkId) const
	{
		return ChunkIdToIndex.Find(ChunkId);
	}

	const FIoOffsetAndLength* GetOffsetAndLength(const FIoChunkId& ChunkId) const
	{
		if (const int32* Index = ChunkIdToIndex.Find(ChunkId))
		{
			return &Toc.ChunkOffsetLengths[*Index];
		}

		return nullptr;
	}

	void GetFileNamesToIndex(TArray<FStringView>& OutFileNames) const
	{
		OutFileNames.Empty(ChunkIdToFileName.Num());
		for (auto& ChinkIdAndFileName : ChunkIdToFileName)
		{
			OutFileNames.Emplace(ChinkIdAndFileName.Value);
		}
	}

	const FString* GetFileName(const FIoChunkId& ChunkId) const
	{
		return ChunkIdToFileName.Find(ChunkId);
	}

	FIoStoreTocChunkInfo GetTocChunkInfo(const int32 TocEntryIndex, const TMap<int32, FString>* InChunkFileNamesMap) const
	{
		const FIoStoreTocResource& TocResource = GetTocResource();
		const FIoStoreTocEntryMeta& Meta = TocResource.ChunkMetas[TocEntryIndex];
		const FIoOffsetAndLength& OffsetLength = TocResource.ChunkOffsetLengths[TocEntryIndex];

		const bool bIsContainerCompressed = EnumHasAnyFlags(TocResource.Header.ContainerFlags, EIoContainerFlags::Compressed);

		FIoStoreTocChunkInfo ChunkInfo;
		ChunkInfo.Id = TocResource.ChunkIds[TocEntryIndex];
		ChunkInfo.ChunkType = ChunkInfo.Id.GetChunkType();
		ChunkInfo.Hash = Meta.ChunkHash;
		ChunkInfo.bIsCompressed = EnumHasAnyFlags(Meta.Flags, FIoStoreTocEntryMetaFlags::Compressed);
		ChunkInfo.bIsMemoryMapped = EnumHasAnyFlags(Meta.Flags, FIoStoreTocEntryMetaFlags::MemoryMapped);
		ChunkInfo.bForceUncompressed = bIsContainerCompressed && !EnumHasAnyFlags(Meta.Flags, FIoStoreTocEntryMetaFlags::Compressed);
		ChunkInfo.Offset = OffsetLength.GetOffset();
		ChunkInfo.Size = OffsetLength.GetLength();

		const FString* FindFileName = 0;
		if (InChunkFileNamesMap &&
			(FindFileName = InChunkFileNamesMap->Find(TocEntryIndex)) != nullptr)
		{
			ChunkInfo.FileName = *FindFileName;
			ChunkInfo.bHasValidFileName = true;
		}
		else
		{
			ChunkInfo.FileName = FString::Printf(TEXT("<%s>"), *LexToString(ChunkInfo.ChunkType));
			ChunkInfo.bHasValidFileName = false;
		}

		const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;
		int32 FirstBlockIndex = int32(ChunkInfo.Offset / CompressionBlockSize);
		int32 LastBlockIndex = int32((Align(ChunkInfo.Offset + ChunkInfo.Size, CompressionBlockSize) - 1) / CompressionBlockSize);

		ChunkInfo.NumCompressedBlocks = LastBlockIndex - FirstBlockIndex + 1;
		ChunkInfo.OffsetOnDisk = TocResource.CompressionBlocks[FirstBlockIndex].GetOffset();
		ChunkInfo.CompressedSize = 0;
		ChunkInfo.PartitionIndex = -1;
		for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
		{
			const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
			ChunkInfo.CompressedSize += CompressionBlock.GetCompressedSize();
			if (ChunkInfo.PartitionIndex < 0)
			{
				ChunkInfo.PartitionIndex = int32(CompressionBlock.GetOffset() / TocResource.Header.PartitionSize);
			}
		}
		return ChunkInfo;
	}

private:
	TMap<FIoChunkId, int32> ChunkIdToIndex;
	FIoStoreTocResource Toc;
	TMap<FIoChunkId, FString> ChunkIdToFileName;
};

class FIoStoreWriter
	: public IIoStoreWriter
{
public:
	FIoStoreWriter(const TCHAR* InContainerPathAndBaseFileName)
		: ContainerPathAndBaseFileName(InContainerPathAndBaseFileName)
	{
	}

	void SetReferenceChunkDatabase(TSharedPtr<IIoStoreWriterReferenceChunkDatabase> InReferenceChunkDatabase)
	{
		if (InReferenceChunkDatabase.IsValid() == false)
		{
			ReferenceChunkDatabase = InReferenceChunkDatabase;
			return;
		}

		if (InReferenceChunkDatabase->GetCompressionBlockSize() != WriterContext->GetSettings().CompressionBlockSize)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Reference chunk database has a different compression block size than the current writer!"));
			UE_LOG(LogIoStore, Warning, TEXT("No chunks will match, so ignoring. ReferenceChunkDb: %d, IoStoreWriter: %d"), InReferenceChunkDatabase->GetCompressionBlockSize(), WriterContext->GetSettings().CompressionBlockSize);
			return;
		}
		ReferenceChunkDatabase = InReferenceChunkDatabase;
		
		// Add ourselves to the reference chunk db's list of possibles
		ReferenceChunkDatabase->NotifyAddedToWriter(ContainerSettings.ContainerId);
	}
	void SetHashDatabase(TSharedPtr<IIoStoreWriterHashDatabase> InHashDatabase, bool bInVerifyHashDatabase)
	{
		HashDatabase = InHashDatabase;
		bVerifyHashDatabase = bInVerifyHashDatabase;
	}

	void EnumerateChunks(TFunction<bool(FIoStoreTocChunkInfo&&)>&& Callback) const
	{
		const FIoStoreTocResource& TocResource = Toc.GetTocResource();

		for (int32 ChunkIndex = 0; ChunkIndex < TocResource.ChunkIds.Num(); ++ChunkIndex)
		{
			FIoStoreTocChunkInfo ChunkInfo = Toc.GetTocChunkInfo(ChunkIndex, 0);
			if (!Callback(MoveTemp(ChunkInfo)))
			{
				break;
			}
		}
	}

	[[nodiscard]] FIoStatus Initialize(FIoStoreWriterContextImpl& InContext, const FIoContainerSettings& InContainerSettings)
	{
		WriterContext = &InContext;
		ContainerSettings = InContainerSettings;

		TocFilePath = ContainerPathAndBaseFileName + TEXT(".utoc");
		
		IPlatformFile& Ipf = IPlatformFile::GetPlatformPhysical();
		Ipf.CreateDirectoryTree(*FPaths::GetPath(TocFilePath));

		FPartition& Partition = Partitions.AddDefaulted_GetRef();
		Partition.Index = 0;

		return FIoStatus::Ok;
	}

	virtual void EnableDiskLayoutOrdering(const TArray<TUniquePtr<FIoStoreReader>>& PatchSourceReaders) override
	{
		check(!LayoutEntriesHead);
		check(!Entries.Num());
		LayoutEntriesHead = new FLayoutEntry();
		LayoutEntries.Add(LayoutEntriesHead);
		FLayoutEntry* PrevEntryLink = LayoutEntriesHead;

		for (const TUniquePtr<FIoStoreReader>& PatchSourceReader : PatchSourceReaders)
		{
			TArray<TPair<uint64, FLayoutEntry*>> LayoutEntriesWithOffsets;
			PatchSourceReader->EnumerateChunks([this, &PrevEntryLink, &LayoutEntriesWithOffsets](const FIoStoreTocChunkInfo& ChunkInfo)
				{
					FLayoutEntry* PreviousBuildEntry = new FLayoutEntry();
					PreviousBuildEntry->Hash = ChunkInfo.Hash;
					PreviousBuildEntry->PartitionIndex = ChunkInfo.PartitionIndex;
					PreviousBuildEntry->CompressedSize = ChunkInfo.CompressedSize;
					LayoutEntriesWithOffsets.Emplace(ChunkInfo.Offset, PreviousBuildEntry);
					PreviousBuildLayoutEntryByChunkId.Add(ChunkInfo.Id, PreviousBuildEntry);
					return true;
				});

			// Sort entries by offset
			Algo::Sort(LayoutEntriesWithOffsets, [](const TPair<uint64, FLayoutEntry*>& A, const TPair<uint64, FLayoutEntry*>& B)
				{
					return A.Get<0>() < B.Get<0>();
				});

			for (const TPair<uint64, FLayoutEntry*>& EntryWithOffset : LayoutEntriesWithOffsets)
			{
				FLayoutEntry* PreviousBuildEntry = EntryWithOffset.Get<1>();
				LayoutEntries.Add(PreviousBuildEntry);
				PrevEntryLink->Next = PreviousBuildEntry;
				PreviousBuildEntry->Prev = PrevEntryLink;
				PrevEntryLink = PreviousBuildEntry;
			}
			if (!ContainerSettings.bGenerateDiffPatch)
			{
				break;
			}
		}

		LayoutEntriesTail = new FLayoutEntry();
		LayoutEntries.Add(LayoutEntriesTail);
		PrevEntryLink->Next = LayoutEntriesTail;
		LayoutEntriesTail->Prev = PrevEntryLink;
	}

	virtual void Append(const FIoChunkId& ChunkId, IIoStoreWriteRequest* Request, const FIoWriteOptions& WriteOptions) override
	{
		//
		// This function sets up the sequence of events that takes a chunk from source data on disc
		// to written to a container. The first thing that happens is the source data is read in order
		// to hash it to detect whether or not it's modified as well as look up in reference databases.
		// Load the data -> PrepareSourceBufferAsync
		// Hash the data -> HashTask lambda
		//
		// The hash task itself doesn't continue to the next steps - the Flush() call
		// waits for all hashes to be complete before kicking the next steps.
		//
		TRACE_CPUPROFILER_EVENT_SCOPE(AppendWriteRequest);
		check(!bHasFlushed);
		checkf(ChunkId.IsValid(), TEXT("ChunkId is not valid!"));

		FIoStoreWriteQueueEntry* Entry = new FIoStoreWriteQueueEntry();
		Entry->Writer = this;
		Entry->Sequence = Entries.Num();
		WriterContext->TotalChunksCount.IncrementExchange();
		Entries.Add(Entry);
		Entry->ChunkId = ChunkId;
		Entry->Options = WriteOptions;
		Entry->Request = Request;		
		Entry->BeginCompressionBarrier = FGraphEvent::CreateGraphEvent();
		Entry->FinishCompressionBarrier = FGraphEvent::CreateGraphEvent();
		Entry->FinishEncryptionAndSigningBarrier = FGraphEvent::CreateGraphEvent();
		Entry->BeginOnDemandDataBarrier = FGraphEvent::CreateGraphEvent();
		Entry->BeginWriteBarrier = FGraphEvent::CreateGraphEvent();
		Entry->WriteFinishedEvent = FGraphEvent::CreateGraphEvent();
		
		bool bTestHashValid = false;
		FIoChunkHash TestHash;
		// If we can get the hash without reading the whole thing and hashing it, do so
		// to avoid the IO.
		if (HashDatabase.IsValid() &&
			HashDatabase->FindHashForChunkId(ChunkId, Entry->ChunkHash))
		{
			if (bVerifyHashDatabase == false)
			{
				// If we aren't validating then we just use it and bail.
				WriterContext->HashDbChunksCount.IncrementExchange();
				WriterContext->HashDbChunksByType[(int8)Entry->ChunkId.GetChunkType()].IncrementExchange();
				WriterContext->HashedChunksCount.IncrementExchange();

				if (ReferenceChunkDatabase.IsValid() && CompressionMethodForEntry(Entry) != NAME_None)
				{
					TPair<FIoContainerId, FIoChunkHash> ChunkKey(ContainerSettings.ContainerId, Entry->ChunkHash);
					Entry->bLoadingFromReferenceDb = ReferenceChunkDatabase->ChunkExists(ChunkKey, Entry->ChunkId, Entry->NumChunkBlocksFromRefDb);
					Entry->bCouldBeFromReferenceDb = true;
				}
				return;
			}

			// If we are validating, copy the hash out and run the normal path
			// to check against it.
			TestHash = Entry->ChunkHash;
			bTestHashValid = true;
		}
		// Otherwise, we have to do the load & hash

		Entry->HashBarrier = FGraphEvent::CreateGraphEvent();

		FGraphEventArray HashPrereqs;
		HashPrereqs.Add(Entry->HashBarrier);
		Entry->HashTask = FFunctionGraphTask::CreateAndDispatchWhenReady([this, Entry, TestHash, bTestHashValid]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(HashChunk);
			const FIoBuffer* SourceBuffer = Entry->Request->GetSourceBuffer();
			Entry->ChunkHash = FIoChunkHash::HashBuffer(SourceBuffer->Data(), SourceBuffer->DataSize());
			WriterContext->HashedChunksCount.IncrementExchange();

			if (bVerifyHashDatabase && bTestHashValid)
			{
				if (TestHash != Entry->ChunkHash)
				{
					UE_LOG(LogIoStore, Warning, TEXT("HashDb Validation Failed: ChunkId %s has mismatching hash"), *LexToString(Entry->ChunkId));
				}
			}


			if (ReferenceChunkDatabase.IsValid() && CompressionMethodForEntry(Entry) != NAME_None)
			{
				TPair<FIoContainerId, FIoChunkHash> ChunkKey(ContainerSettings.ContainerId, Entry->ChunkHash);
				Entry->bLoadingFromReferenceDb = ReferenceChunkDatabase->ChunkExists(ChunkKey, Entry->ChunkId, Entry->NumChunkBlocksFromRefDb);
				Entry->bCouldBeFromReferenceDb = true;
			}

			// Release the source data buffer, it will be reloaded later when we start compressing the chunk
			Entry->Request->FreeSourceBuffer();
		}, TStatId(), &HashPrereqs, ENamedThreads::AnyHiPriThreadHiPriTask);
		Entry->Request->PrepareSourceBufferAsync(Entry->HashBarrier);
	}

	virtual void Append(const FIoChunkId& ChunkId, FIoBuffer Chunk, const FIoWriteOptions& WriteOptions, uint64 OrderHint) override
	{
		struct FWriteRequest
			: IIoStoreWriteRequest
		{
			FWriteRequest(FIoBuffer InSourceBuffer, uint64 InOrderHint)
				: OrderHint(InOrderHint)
			{
				SourceBuffer = InSourceBuffer;
				SourceBuffer.MakeOwned();
			}

			virtual ~FWriteRequest() = default;

			void PrepareSourceBufferAsync(FGraphEventRef CompletionEvent) override
			{
				CompletionEvent->DispatchSubsequents();
			}

			const FIoBuffer* GetSourceBuffer() override
			{
				return &SourceBuffer;
			}

			void FreeSourceBuffer() override
			{
			}

			uint64 GetOrderHint() override
			{
				return OrderHint;
			}

			TArrayView<const FFileRegion> GetRegions()
			{
				return TArrayView<const FFileRegion>();
			}

			FIoBuffer SourceBuffer;
			uint64 OrderHint;
		};

		Append(ChunkId, new FWriteRequest(Chunk, OrderHint), WriteOptions);
	}

	bool GeneratePerfectHashes(FIoStoreTocResource& TocResource, const TCHAR* ContainerDebugName)
	{
		// https://en.wikipedia.org/wiki/Perfect_hash_function
		TRACE_CPUPROFILER_EVENT_SCOPE(TocGeneratePerfectHashes);
		uint32 ChunkCount = TocResource.ChunkIds.Num();
		uint32 SeedCount = FMath::Max(1, FMath::RoundToInt32(ChunkCount / 2.0));
		check(TocResource.ChunkOffsetLengths.Num() == ChunkCount);
		
		TArray<FIoChunkId> OutTocChunkIds;
		OutTocChunkIds.SetNum(ChunkCount);
		TArray<FIoOffsetAndLength> OutTocOffsetAndLengths;
		OutTocOffsetAndLengths.SetNum(ChunkCount);
		TArray<FIoStoreTocEntryMeta> OutTocChunkMetas;
		OutTocChunkMetas.SetNum(ChunkCount);
		TArray<FIoStoreTocOnDemandChunkMeta> OutTocOnDemandChunkMeta;
		OutTocOnDemandChunkMeta.SetNum(ChunkCount);
		TArray<int32> OutTocChunkHashSeeds;
		OutTocChunkHashSeeds.SetNumZeroed(SeedCount);
		TArray<int32> OutTocChunkIndicesWithoutPerfectHash;

		TArray<TArray<int32>> Buckets;
		Buckets.SetNum(SeedCount);

		TBitArray<> FreeSlots(true, ChunkCount);
		// Put each chunk in a bucket, each bucket contains the chunk ids that have colliding hashes
		for (uint32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			const FIoChunkId& ChunkId = TocResource.ChunkIds[ChunkIndex];
			Buckets[FIoStoreTocResource::HashChunkIdWithSeed(0, ChunkId) % SeedCount].Add(ChunkIndex);
		}

		uint64 TotalIterationCount = 0;
		uint64 TotalOverflowBucketsCount = 0;
		
		// For each bucket containing more than one chunk id find a seed that makes its chunk ids
		// hash to unused slots in the output array
		Algo::Sort(Buckets, [](const TArray<int32>& A, const TArray<int32>& B)
			{
				return A.Num() > B.Num();
			});
		for (uint32 BucketIndex = 0; BucketIndex < SeedCount; ++BucketIndex)
		{
			const TArray<int32>& Bucket = Buckets[BucketIndex];
			if (Bucket.Num() <= 1)
			{
				break;
			}
			uint64 BucketHash = FIoStoreTocResource::HashChunkIdWithSeed(0, TocResource.ChunkIds[Bucket[0]]);

			static constexpr uint32 Primes[] = {
				2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79,
				83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167,
				173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263,
				269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367,
				373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449, 457, 461, 463,
				467, 479, 487, 491, 499, 503, 509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587,
				593, 599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683,
				691, 701, 709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811,
				821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919, 929,
				937, 941, 947, 953, 967, 971, 977, 983, 991, 997, 1009, 1013, 1019, 1021, 1031, 1033,
				1039, 1049, 1051, 1061, 1063, 1069, 1087, 1091, 1093, 1097, 1103, 1109, 1117, 1123,
				1129, 1151, 1153, 1163, 1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223, 1229, 1231,
				1237, 1249, 1259, 1277, 1279, 1283, 1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321,
				1327, 1361, 1367, 1373, 1381, 1399, 1409, 1423, 1427, 1429, 1433, 1439, 1447, 1451,
				1453, 1459, 1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511, 1523, 1531, 1543, 1549,
				1553, 1559, 1567, 1571, 1579, 1583, 1597, 1601, 1607, 1609, 1613, 1619, 1621, 1627,
				1637, 1657, 1663, 1667, 1669, 1693, 1697, 1699, 1709, 1721, 1723, 1733, 1741, 1747,
				1753, 1759, 1777, 1783, 1787, 1789, 1801, 1811, 1823, 1831, 1847, 1861, 1867, 1871,
				1873, 1877, 1879, 1889, 1901, 1907, 1913, 1931, 1933, 1949, 1951, 1973, 1979, 1987,
				1993, 1997, 1999, 2003, 2011, 2017, 2027, 2029, 2039, 2053, 2063, 2069, 2081, 2083,
				2087, 2089, 2099, 2111, 2113, 2129, 2131, 2137, 2141, 2143, 2153, 2161, 2179, 2203,
				2207, 2213, 2221, 2237, 2239, 2243, 2251, 2267, 2269, 2273, 2281, 2287, 2293, 2297,
				2309, 2311, 2333, 2339, 2341, 2347, 2351, 2357, 2371, 2377, 2381, 2383, 2389, 2393,
				2399, 2411, 2417, 2423, 2437, 2441, 2447, 2459, 2467, 2473, 2477, 2503, 2521, 2531,
				2539, 2543, 2549, 2551, 2557, 2579, 2591, 2593, 2609, 2617, 2621, 2633, 2647, 2657,
				2659, 2663, 2671, 2677, 2683, 2687, 2689, 2693, 2699, 2707, 2711, 2713, 2719, 2729,
				2731, 2741, 2749, 2753, 2767, 2777, 2789, 2791, 2797, 2801, 2803, 2819, 2833, 2837,
				2843, 2851, 2857, 2861, 2879, 2887, 2897, 2903, 2909, 2917, 2927, 2939, 2953, 2957,
				2963, 2969, 2971, 2999, 3001, 3011, 3019, 3023, 3037, 3041, 3049, 3061, 3067, 3079,
				3083, 3089, 3109, 3119, 3121, 3137, 3163, 3167, 3169, 3181, 3187, 3191, 3203, 3209,
				3217, 3221, 3229, 3251, 3253, 3257, 3259, 3271, 3299, 3301, 3307, 3313, 3319, 3323,
				3329, 3331, 3343, 3347, 3359, 3361, 3371, 3373, 3389, 3391, 3407, 3413, 3433, 3449,
				3457, 3461, 3463, 3467, 3469, 3491, 3499, 3511, 3517, 3527, 3529, 3533, 3539, 3541,
				3547, 3557, 3559, 3571, 3581, 3583, 3593, 3607, 3613, 3617, 3623, 3631, 3637, 3643,
				3659, 3671, 3673, 3677, 3691, 3697, 3701, 3709, 3719, 3727, 3733, 3739, 3761, 3767,
				3769, 3779, 3793, 3797, 3803, 3821, 3823, 3833, 3847, 3851, 3853, 3863, 3877, 3881,
				3889, 3907, 3911, 3917, 3919, 3923, 3929, 3931, 3943, 3947, 3967, 3989, 4001, 4003,
				4007, 4013, 4019, 4021, 4027, 4049, 4051, 4057, 4073, 4079, 4091, 4093, 4099, 4111,
				4127, 4129, 4133, 4139, 4153, 4157, 4159, 4177, 4201, 4211, 4217, 4219, 4229, 4231,
				4241, 4243, 4253, 4259, 4261, 4271, 4273, 4283, 4289, 4297, 4327, 4337, 4339, 4349,
				4357, 4363, 4373, 4391, 4397, 4409, 4421, 4423, 4441, 4447, 4451, 4457, 4463, 4481,
				4483, 4493, 4507, 4513, 4517, 4519, 4523, 4547, 4549, 4561, 4567, 4583, 4591, 4597,
				4603, 4621, 4637, 4639, 4643, 4649, 4651, 4657, 4663, 4673, 4679, 4691, 4703, 4721,
				4723, 4729, 4733, 4751, 4759, 4783, 4787, 4789, 4793, 4799, 4801, 4813, 4817, 4831,
				4861, 4871, 4877, 4889, 4903, 4909, 4919, 4931, 4933, 4937, 4943, 4951, 4957, 4967,
				4969, 4973, 4987, 4993, 4999, 5003, 5009, 5011, 5021, 5023, 5039, 5051, 5059, 5077,
				5081, 5087, 5099, 5101, 5107, 5113, 5119, 5147, 5153, 5167, 5171, 5179, 5189, 5197,
				5209, 5227, 5231, 5233, 5237, 5261, 5273, 5279, 5281, 5297, 5303, 5309, 5323, 5333,
				5347, 5351, 5381, 5387, 5393, 5399, 5407, 5413, 5417, 5419, 5431, 5437, 5441, 5443,
				5449, 5471, 5477, 5479, 5483, 5501, 5503, 5507, 5519, 5521, 5527, 5531, 5557, 5563,
				5569, 5573, 5581, 5591, 5623, 5639, 5641, 5647, 5651, 5653, 5657, 5659, 5669, 5683,
				5689, 5693, 5701, 5711, 5717, 5737, 5741, 5743, 5749, 5779, 5783, 5791, 5801, 5807,
				5813, 5821, 5827, 5839, 5843, 5849, 5851, 5857, 5861, 5867, 5869, 5879, 5881, 5897,
				5903, 5923, 5927, 5939, 5953, 5981, 5987, 6007, 6011, 6029, 6037, 6043, 6047, 6053,
				6067, 6073, 6079, 6089, 6091, 6101, 6113, 6121, 6131, 6133, 6143, 6151, 6163, 6173,
				6197, 6199, 6203, 6211, 6217, 6221, 6229, 6247, 6257, 6263, 6269, 6271, 6277, 6287,
				6299, 6301, 6311, 6317, 6323, 6329, 6337, 6343, 6353, 6359, 6361, 6367, 6373, 6379,
				6389, 6397, 6421, 6427, 6449, 6451, 6469, 6473, 6481, 6491, 6521, 6529, 6547, 6551,
				6553, 6563, 6569, 6571, 6577, 6581, 6599, 6607, 6619, 6637, 6653, 6659, 6661, 6673,
				6679, 6689, 6691, 6701, 6703, 6709, 6719, 6733, 6737, 6761, 6763, 6779, 6781, 6791,
				6793, 6803, 6823, 6827, 6829, 6833, 6841, 6857, 6863, 6869, 6871, 6883, 6899, 6907,
				6911, 6917, 6947, 6949, 6959, 6961, 6967, 6971, 6977, 6983, 6991, 6997, 7001, 7013,
				7019, 7027, 7039, 7043, 7057, 7069, 7079, 7103, 7109, 7121, 7127, 7129, 7151, 7159,
				7177, 7187, 7193, 7207, 7211, 7213, 7219, 7229, 7237, 7243, 7247, 7253, 7283, 7297,
				7307, 7309, 7321, 7331, 7333, 7349, 7351, 7369, 7393, 7411, 7417, 7433, 7451, 7457,
				7459, 7477, 7481, 7487, 7489, 7499, 7507, 7517, 7523, 7529, 7537, 7541, 7547, 7549,
				7559, 7561, 7573, 7577, 7583, 7589, 7591, 7603, 7607, 7621, 7639, 7643, 7649, 7669,
				7673, 7681, 7687, 7691, 7699, 7703, 7717, 7723, 7727, 7741, 7753, 7757, 7759, 7789,
				7793, 7817, 7823, 7829, 7841, 7853, 7867, 7873, 7877, 7879, 7883, 7901, 7907, 7919
			};
			static constexpr uint32 MaxIterations = UE_ARRAY_COUNT(Primes);

			uint32 PrimeIndex = 0;
			TBitArray<> BucketUsedSlots(false, ChunkCount);
			int32 IndexInBucket = 0;
			bool bFoundSeedForBucket = true;
			uint64 BucketIterationCount = 0;
			while (IndexInBucket < Bucket.Num())
			{
				++BucketIterationCount;
				const FIoChunkId& ChunkId = TocResource.ChunkIds[Bucket[IndexInBucket]];
				uint32 Seed = Primes[PrimeIndex];
				uint32 Slot = FIoStoreTocResource::HashChunkIdWithSeed(Seed, ChunkId) % ChunkCount;
				if (!FreeSlots[Slot] || BucketUsedSlots[Slot])
				{
					++PrimeIndex;
					if (PrimeIndex == MaxIterations)
					{
						// Unable to resolve collisions for this bucket, put items in the overflow list and
						// save the negative index of the first item in the bucket as the seed
						// (-ChunkCount - 1 to separate from the single item buckets below)
						UE_LOG(LogIoStore, Verbose, TEXT("%s: Failed finding seed for bucket with %d items after %d iterations."), ContainerDebugName, Bucket.Num(), BucketIterationCount);
						bFoundSeedForBucket = false;
						OutTocChunkHashSeeds[BucketHash % SeedCount] = -OutTocChunkIndicesWithoutPerfectHash.Num() - ChunkCount - 1;
						OutTocChunkIndicesWithoutPerfectHash.Append(Bucket);
						++TotalOverflowBucketsCount;
						break;

					}
					IndexInBucket = 0;
					BucketUsedSlots.Init(false, ChunkCount);
				}
				else
				{
					BucketUsedSlots[Slot] = true;
					++IndexInBucket;
				}
			}

			TotalIterationCount += BucketIterationCount;

			if (bFoundSeedForBucket)
			{
				uint32 Seed = Primes[PrimeIndex];
				OutTocChunkHashSeeds[BucketHash % SeedCount] = Seed;
				for (IndexInBucket = 0; IndexInBucket < Bucket.Num(); ++IndexInBucket)
				{
					int32 ChunkIndex = Bucket[IndexInBucket];
					const FIoChunkId& ChunkId = TocResource.ChunkIds[ChunkIndex];
					uint32 Slot = FIoStoreTocResource::HashChunkIdWithSeed(Seed, ChunkId) % ChunkCount;
					check(FreeSlots[Slot]);
					FreeSlots[Slot] = false;
					OutTocChunkIds[Slot] = ChunkId;
					OutTocOffsetAndLengths[Slot] = TocResource.ChunkOffsetLengths[ChunkIndex];
					OutTocChunkMetas[Slot] = TocResource.ChunkMetas[ChunkIndex];
					OutTocOnDemandChunkMeta[Slot] = TocResource.OnDemandChunkMeta[ChunkIndex];
				}
			}
		}

		// For the remaining buckets with only one chunk id put that chunk id in the first empty position in
		// the output array and store the index as a negative seed for the bucket (-1 to allow use of slot 0)
		TConstSetBitIterator<> FreeSlotIt(FreeSlots);
		for (uint32 BucketIndex = 0; BucketIndex < SeedCount; ++BucketIndex)
		{
			const TArray<int32>& Bucket = Buckets[BucketIndex];
			if (Bucket.Num() == 1)
			{
				uint32 Slot = FreeSlotIt.GetIndex();
				++FreeSlotIt;
				int32 ChunkIndex = Bucket[0];
				const FIoChunkId& ChunkId = TocResource.ChunkIds[ChunkIndex];
				uint64 BucketHash = FIoStoreTocResource::HashChunkIdWithSeed(0, ChunkId);
				OutTocChunkHashSeeds[BucketHash % SeedCount] = -static_cast<int32>(Slot) - 1;
				OutTocChunkIds[Slot] = ChunkId;
				OutTocOffsetAndLengths[Slot] = TocResource.ChunkOffsetLengths[ChunkIndex];
				OutTocChunkMetas[Slot] = TocResource.ChunkMetas[ChunkIndex];
				OutTocOnDemandChunkMeta[Slot] = TocResource.OnDemandChunkMeta[ChunkIndex];
			}
		}

		if (!OutTocChunkIndicesWithoutPerfectHash.IsEmpty())
		{
			// Put overflow items in the remaining free slots and update the index for each overflow entry
			UE_LOG(LogIoStore, Display, TEXT("%s: Failed finding perfect hashmap for %d items. %d overflow buckets with %d items."), ContainerDebugName, ChunkCount, TotalOverflowBucketsCount, OutTocChunkIndicesWithoutPerfectHash.Num());
			for (int32& OverflowEntryIndex : OutTocChunkIndicesWithoutPerfectHash)
			{
				uint32 Slot = FreeSlotIt.GetIndex();
				++FreeSlotIt;
				const FIoChunkId& ChunkId = TocResource.ChunkIds[OverflowEntryIndex];
				OutTocChunkIds[Slot] = ChunkId;
				OutTocOffsetAndLengths[Slot] = TocResource.ChunkOffsetLengths[OverflowEntryIndex];
				OutTocChunkMetas[Slot] = TocResource.ChunkMetas[OverflowEntryIndex];
				OutTocOnDemandChunkMeta[Slot] = TocResource.OnDemandChunkMeta[OverflowEntryIndex];
				OverflowEntryIndex = Slot;
			}
		}
		else
		{
			UE_LOG(LogIoStore, Display, TEXT("%s: Found perfect hashmap for %d items."), ContainerDebugName, ChunkCount);
		}
		double AverageIterationCount = ChunkCount > 0 ? static_cast<double>(TotalIterationCount) / ChunkCount : 0.0;
		UE_LOG(LogIoStore, Verbose, TEXT("%s: %f iterations/chunk"), ContainerDebugName, AverageIterationCount);

		TocResource.ChunkIds = MoveTemp(OutTocChunkIds);
		TocResource.ChunkOffsetLengths = MoveTemp(OutTocOffsetAndLengths);
		TocResource.ChunkMetas = MoveTemp(OutTocChunkMetas);
		TocResource.OnDemandChunkMeta = MoveTemp(OutTocOnDemandChunkMeta);
		TocResource.ChunkPerfectHashSeeds = MoveTemp(OutTocChunkHashSeeds);
		TocResource.ChunkIndicesWithoutPerfectHash = MoveTemp(OutTocChunkIndicesWithoutPerfectHash);

		return true;
	}

	void Finalize()
	{
		check(bHasFlushed);

		UncompressedContainerSize = TotalEntryUncompressedSize + TotalPaddingSize;
		CompressedContainerSize = 0;
		const FIoStoreWriterSettings& WriterSettings = WriterContext->GetSettings();
		for (FPartition& Partition : Partitions)
		{
			CompressedContainerSize += Partition.Offset;

			if (bHasMemoryMappedEntry)
			{
				uint64 ExtraPaddingBytes = Align(Partition.Offset, WriterSettings.MemoryMappingAlignment) - Partition.Offset;
				if (ExtraPaddingBytes)
				{
					TArray<uint8> Padding;
					Padding.SetNumZeroed(int32(ExtraPaddingBytes));
					Partition.ContainerFileHandle->Serialize(Padding.GetData(), ExtraPaddingBytes);
					CompressedContainerSize += ExtraPaddingBytes;
					UncompressedContainerSize += ExtraPaddingBytes;
					Partition.Offset += ExtraPaddingBytes;
					TotalPaddingSize += ExtraPaddingBytes;
				}
			}
			
			if (Partition.ContainerFileHandle)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FlushContainerFile);
				Partition.ContainerFileHandle->Flush();
				check(Partition.ContainerFileHandle->Tell() == Partition.Offset);
			}

			if (Partition.RegionsArchive)
			{
				FFileRegion::SerializeFileRegions(*Partition.RegionsArchive.Get(), Partition.AllFileRegions);
				Partition.RegionsArchive->Flush();
			}
		}

		FIoStoreTocResource& TocResource = Toc.GetTocResource();

		GeneratePerfectHashes(TocResource, *FPaths::GetBaseFilename(TocFilePath));

		if (ContainerSettings.IsIndexed())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BuildIndex);
			TArray<FStringView> FilesToIndex;
			Toc.GetFileNamesToIndex(FilesToIndex);

			FString MountPoint = IoDirectoryIndexUtils::GetCommonRootPath(FilesToIndex);
			FIoDirectoryIndexWriter DirectoryIndexWriter;
			DirectoryIndexWriter.SetMountPoint(MountPoint);

			uint32 TocEntryIndex = 0;
			for (const FIoChunkId& ChunkId : TocResource.ChunkIds)
			{
				const FString* ChunkFileName = Toc.GetFileName(ChunkId);
				if (ChunkFileName)
				{
					const uint32 FileEntryIndex = DirectoryIndexWriter.AddFile(*ChunkFileName);
					check(FileEntryIndex != ~uint32(0));
					DirectoryIndexWriter.SetFileUserData(FileEntryIndex, TocEntryIndex);
				}
				++TocEntryIndex;
			}

			DirectoryIndexWriter.Flush(
				TocResource.DirectoryIndexBuffer,
				ContainerSettings.IsEncrypted() ? ContainerSettings.EncryptionKey : FAES::FAESKey());
		}

		TIoStatusOr<uint64> TocSize = FIoStoreTocResource::Write(*TocFilePath, TocResource, static_cast<uint32>(WriterSettings.CompressionBlockSize), WriterSettings.MaxPartitionSize, ContainerSettings);
		check(TocSize.IsOk());
		
		Result.ContainerId = ContainerSettings.ContainerId;
		Result.ContainerName = FPaths::GetBaseFilename(TocFilePath);
		Result.ContainerFlags = ContainerSettings.ContainerFlags;
		Result.TocSize = TocSize.ConsumeValueOrDie();
		Result.TocEntryCount = TocResource.Header.TocEntryCount;
		Result.PaddingSize = TotalPaddingSize;
		Result.UncompressedContainerSize = UncompressedContainerSize;
		Result.CompressedContainerSize = CompressedContainerSize;
		Result.TotalEntryCompressedSize = TotalEntryCompressedSize;
		Result.ReferenceCacheMissBytes = ReferenceCacheMissBytes;
		Result.DirectoryIndexSize = TocResource.Header.DirectoryIndexSize;
		Result.CompressionMethod = EnumHasAnyFlags(ContainerSettings.ContainerFlags, EIoContainerFlags::Compressed)
			? WriterSettings.CompressionMethod
			: NAME_None;
		Result.ModifiedChunksCount = 0;
		Result.AddedChunksCount = 0;
		Result.ModifiedChunksSize= 0;
		Result.AddedChunksSize = 0;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Cleanup);
			for (FIoStoreWriteQueueEntry* Entry : Entries)
			{
				if (Entry->bModified)
				{
					++Result.ModifiedChunksCount;
					Result.ModifiedChunksSize += Entry->CompressedSize;
				}
				else if (Entry->bAdded)
				{
					++Result.AddedChunksCount;
					Result.AddedChunksSize += Entry->CompressedSize;
				}
				delete Entry;
			}
		}

		Entries.Empty();
		bHasResult = true;
	}

	TIoStatusOr<FIoStoreWriterResult> GetResult()
	{
		if (!bHasResult)
		{
			return FIoStatus::Invalid;
		}
		return Result;
	}

private:
	struct FPartition
	{
		TUniquePtr<FArchive> ContainerFileHandle;
		TUniquePtr<FArchive> RegionsArchive;
		uint64 Offset = 0;
		uint64 ReservedSpace = 0;
		TArray<FFileRegion> AllFileRegions;
		int32 Index = -1;
	};

	struct FLayoutEntry
	{
		FLayoutEntry* Prev = nullptr;
		FLayoutEntry* Next = nullptr;
		uint64 IdealOrder = 0;
		uint64 CompressedSize = uint64(-1);
		FIoChunkHash Hash;
		FIoStoreWriteQueueEntry* QueueEntry = nullptr;
		int32 PartitionIndex = -1;
	};

	void FinalizeLayout()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FinalizeLayout);
		
		Algo::Sort(Entries, [](const FIoStoreWriteQueueEntry* A, const FIoStoreWriteQueueEntry* B)
		{
			uint64 AOrderHint = A->Request->GetOrderHint();
			uint64 BOrderHint = B->Request->GetOrderHint();
			if (AOrderHint != BOrderHint)
			{
				return AOrderHint < BOrderHint;
			}
			return A->Sequence < B->Sequence;
		});

		TMap<int64, FLayoutEntry*> LayoutEntriesByOrderMap;
		int64 IdealOrder = 0;
		TArray<FLayoutEntry*> UnassignedEntries;
		for (FIoStoreWriteQueueEntry* WriteQueueEntry : Entries)
		{
			FLayoutEntry* FindPreviousEntry = PreviousBuildLayoutEntryByChunkId.FindRef(WriteQueueEntry->ChunkId);
			if (FindPreviousEntry)
			{
				if (FindPreviousEntry->Hash != WriteQueueEntry->ChunkHash)
				{
					WriteQueueEntry->bModified = true;
				}
				else
				{
					FindPreviousEntry->QueueEntry = WriteQueueEntry;
					FindPreviousEntry->IdealOrder = IdealOrder;
					WriteQueueEntry->PartitionIndex = FindPreviousEntry->PartitionIndex;
				}
			}
			else
			{
				WriteQueueEntry->bAdded = true;
			}
			if (WriteQueueEntry->bModified || WriteQueueEntry->bAdded)
			{
				FLayoutEntry* NewLayoutEntry = new FLayoutEntry();
				NewLayoutEntry->QueueEntry = WriteQueueEntry;
				NewLayoutEntry->IdealOrder = IdealOrder;
				LayoutEntries.Add(NewLayoutEntry);
				UnassignedEntries.Add(NewLayoutEntry);
			}
			++IdealOrder;
		}
			
		if (ContainerSettings.bGenerateDiffPatch)
		{
			LayoutEntriesHead->Next = LayoutEntriesTail;
			LayoutEntriesTail->Prev = LayoutEntriesHead;
		}
		else
		{
			for (FLayoutEntry* EntryIt = LayoutEntriesHead->Next; EntryIt != LayoutEntriesTail; EntryIt = EntryIt->Next)
			{
				if (!EntryIt->QueueEntry)
				{
					EntryIt->Prev->Next = EntryIt->Next;
					EntryIt->Next->Prev = EntryIt->Prev;
				}
				else
				{
					LayoutEntriesByOrderMap.Add(EntryIt->IdealOrder, EntryIt);
				}
			}
		}
		FLayoutEntry* LastAddedEntry = LayoutEntriesHead;
		for (FLayoutEntry* UnassignedEntry : UnassignedEntries)
		{
			check(UnassignedEntry->QueueEntry);
			FLayoutEntry* PutAfterEntry = LayoutEntriesByOrderMap.FindRef(UnassignedEntry->IdealOrder - 1);
			if (!PutAfterEntry)
			{
				PutAfterEntry = LastAddedEntry;
			}

			UnassignedEntry->Prev = PutAfterEntry;
			UnassignedEntry->Next = PutAfterEntry->Next;
			PutAfterEntry->Next->Prev = UnassignedEntry;
			PutAfterEntry->Next = UnassignedEntry;
			LayoutEntriesByOrderMap.Add(UnassignedEntry->IdealOrder, UnassignedEntry);
			LastAddedEntry = UnassignedEntry;
		}

		TArray<FIoStoreWriteQueueEntry*> IncludedQueueEntries;
		for (FLayoutEntry* EntryIt = LayoutEntriesHead->Next; EntryIt != LayoutEntriesTail; EntryIt = EntryIt->Next)
		{
			check(EntryIt->QueueEntry);
			IncludedQueueEntries.Add(EntryIt->QueueEntry);
			int32 ReserveInPartitionIndex = EntryIt->QueueEntry->PartitionIndex;
			if (ReserveInPartitionIndex >= 0)
			{
				while (Partitions.Num() <= ReserveInPartitionIndex)
				{
					FPartition& NewPartition = Partitions.AddDefaulted_GetRef();
					NewPartition.Index = Partitions.Num() - 1;
				}
				FPartition& ReserveInPartition = Partitions[ReserveInPartitionIndex];
				check(EntryIt->CompressedSize != uint64(-1));
				ReserveInPartition.ReservedSpace += EntryIt->CompressedSize;
			}
		}
		Swap(Entries, IncludedQueueEntries);

		LayoutEntriesHead = nullptr;
		LayoutEntriesTail = nullptr;
		PreviousBuildLayoutEntryByChunkId.Empty();
		for (FLayoutEntry* Entry : LayoutEntries)
		{
			delete Entry;
		}
		LayoutEntries.Empty();
	}

	FIoStatus CreatePartitionContainerFile(FPartition& Partition)
	{
		check(!Partition.ContainerFileHandle);
		FString ContainerFilePath = ContainerPathAndBaseFileName;
		if (Partition.Index > 0)
		{
			ContainerFilePath += FString::Printf(TEXT("_s%d"), Partition.Index);
		}
		ContainerFilePath += TEXT(".ucas");
		
		Partition.ContainerFileHandle.Reset(IFileManager::Get().CreateFileWriter(*ContainerFilePath));
		if (!Partition.ContainerFileHandle)
		{
			return FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Failed to open IoStore container file '") << *ContainerFilePath << TEXT("'");
		}
		if (WriterContext->GetSettings().bEnableFileRegions)
		{
			FString RegionsFilePath = ContainerFilePath + FFileRegion::RegionsFileExtension;
			Partition.RegionsArchive.Reset(IFileManager::Get().CreateFileWriter(*RegionsFilePath));
			if (!Partition.RegionsArchive)
			{
				return FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Failed to open IoStore regions file '") << *RegionsFilePath << TEXT("'");
			}
		}

		return FIoStatus::Ok;
	}

	void CompressBlock(FChunkBlock* Block)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CompressBlock);
		check(Block->CompressionMethod != NAME_None);
		int32 CompressedBlockSize = int32(Block->IoBuffer->DataSize());
		bool bCompressed;
		{
			bCompressed = FCompression::CompressMemoryIfWorthDecompressing(
				Block->CompressionMethod,
				WriterContext->WriterSettings.CompressionMinBytesSaved,
				WriterContext->WriterSettings.CompressionMinPercentSaved,
				Block->IoBuffer->Data(),
				CompressedBlockSize,
				Block->UncompressedData,
				static_cast<int32>(Block->UncompressedSize),
				COMPRESS_ForPackaging);
		}
		if (!bCompressed)
		{
			Block->CompressionMethod = NAME_None;
			Block->CompressedSize = Block->UncompressedSize;
			FMemory::Memcpy(Block->IoBuffer->Data(), Block->UncompressedData, Block->UncompressedSize);
		}
		else
		{
			check(CompressedBlockSize > 0);
			check(CompressedBlockSize < Block->UncompressedSize);
			Block->CompressedSize = CompressedBlockSize;
		}
	}

	bool SerializeCompressedDDCData(FIoStoreWriteQueueEntry* Entry, FArchive& Ar)
	{
		uint32 NumBlocks = Entry->ChunkBlocks.Num();
		Ar << NumBlocks;
		if (NumBlocks != Entry->ChunkBlocks.Num())
		{
			return false;
		}
		for (FChunkBlock& Block : Entry->ChunkBlocks)
		{
			Ar << Block.CompressedSize;
			if (Block.CompressedSize > Block.UncompressedSize)
			{
				return false;
			}
			if (Ar.IsLoading() && Block.CompressedSize == Block.UncompressedSize)
			{
				Block.CompressionMethod = NAME_None;
			}
			if (Block.IoBuffer->DataSize() < Block.CompressedSize)
			{
				return false;
			}
			Ar.Serialize(Block.IoBuffer->Data(), Block.CompressedSize);
		}
		return !Ar.IsError();
	}

	FName CompressionMethodForEntry(FIoStoreWriteQueueEntry* Entry) const
	{
		FName CompressionMethod = NAME_None;
		const FIoStoreWriterSettings& WriterSettings = WriterContext->WriterSettings;
		if (ContainerSettings.IsCompressed() && !Entry->Options.bForceUncompressed && !Entry->Options.bIsMemoryMapped)
		{
			CompressionMethod = WriterSettings.CompressionMethod;
		}
		return CompressionMethod;
	}



	void BeginCompress(FIoStoreWriteQueueEntry* Entry)
	{
		WriterContext->BeginCompressChunksByType[(int8)Entry->ChunkId.GetChunkType()].IncrementExchange();
		
		if (Entry->bLoadingFromReferenceDb)
		{
			if (Entry->NumChunkBlocksFromRefDb == 0)
			{
				Entry->FinishCompressionBarrier->DispatchSubsequents();
				return;
			}

			// Allocate resources before launching the read tasks to prevent deadlock. Note this will
			// allocate iobuffers big enough for uncompressed size, when we only actually need it for
			// compressed size.
			int32 NumChunkBlocks32 = IntCastChecked<int32>(Entry->NumChunkBlocksFromRefDb);
			Entry->ChunkBlocks.SetNum(NumChunkBlocks32);
			for (int32 BlockIndex = 0; BlockIndex < NumChunkBlocks32; ++BlockIndex)
			{
				FChunkBlock& Block = Entry->ChunkBlocks[BlockIndex];
				Block.IoBuffer = WriterContext->AllocCompressionBuffer(NumChunkBlocks32);
				// Everything else in a block gets filled out from the refdb.
			}

			TPair<FIoContainerId, FIoChunkHash> ChunkKey(ContainerSettings.ContainerId, Entry->ChunkHash);

			// Valid chunks must create the same decompressed bits, but can have different compressed bits.
			// Since we are on a lightweight dispatch thread, the actual read is async, as is the processing
			// of the results.
			bool bChunkExists = ReferenceChunkDatabase->RetrieveChunk(ChunkKey, [this, Entry](TIoStatusOr<FIoStoreCompressedReadResult> InReadResult)
			{

				// If we fail here, in order to recover we effectively need to re-kick this chunk's
				// BeginCompress() as well as source buffer read... however, this is just a direct read and should only fail
				// in catastrophic scenarios (loss of connection on a network drive?).
				FIoStoreCompressedReadResult ReadResult = InReadResult.ValueOrDie();

				uint64 TotalUncompressedSize = 0;
				uint8* ReferenceData = ReadResult.IoBuffer.GetData();
				uint64 TotalAlignedSize = 0;
				for (int32 BlockIndex = 0; BlockIndex < ReadResult.Blocks.Num(); ++BlockIndex)
				{
					FIoStoreCompressedBlockInfo& ReferenceBlock = ReadResult.Blocks[BlockIndex];
					FChunkBlock& Block = Entry->ChunkBlocks[BlockIndex];
					Block.CompressionMethod = ReferenceBlock.CompressionMethod;
					Block.CompressedSize = ReferenceBlock.CompressedSize;
					Block.UncompressedSize = ReferenceBlock.UncompressedSize;
					TotalUncompressedSize += ReferenceBlock.UncompressedSize;

					// Future optimization: ReadCompressed returns the memory ready to encrypt in one
					// large contiguous buffer (i.e. padded). We could use the FIoBuffer functionality of referencing a 
					// sub block from a parent buffer, however this would mean that we need to add support
					// for tracking the memory usage in order to remain within our prescribed limits. To do this
					// requires releasing the entire chunk's memory at once after WriteEntry.
					// As it stands, we temporarily use untracked memory in the ReadCompressed call (in RetrieveChunk),
					// then immediately copy it to tracked memory. There's some waste as tracked memory is mod CompressionBlockSize
					// and we are post compression, so with the average 50% compression rate, we're using double the memory
					// we "could".
					FMemory::Memcpy(Block.IoBuffer->GetData(), ReferenceData, Block.CompressedSize);
					ReferenceData += ReferenceBlock.AlignedSize;
					TotalAlignedSize += ReferenceBlock.AlignedSize;
				}

				if (TotalAlignedSize != ReadResult.IoBuffer.GetSize())
				{
					// If we hit this, we might have read garbage memory above! This is very bad.
					UE_LOG(LogIoStore, Error, TEXT("Block aligned size does not match iobuffer source size! Blocks: %s source size: %s"),
						*FText::AsNumber(TotalAlignedSize).ToString(),
						*FText::AsNumber(ReadResult.IoBuffer.GetSize()).ToString());
				}

				Entry->UncompressedSize.Emplace(TotalUncompressedSize);
				Entry->FinishCompressionBarrier->DispatchSubsequents();
			});

			check(bChunkExists); // Sanity - should never return false as we can only get here if ChunkExists() returns true.

			WriterContext->RefDbChunksCount.IncrementExchange();
			WriterContext->RefDbChunksByType[(int8)Entry->ChunkId.GetChunkType()].IncrementExchange();
			// Lambda handles dispatch subsequents
			return;
		}

		const FIoStoreWriterSettings& WriterSettings = WriterContext->WriterSettings;

		FName CompressionMethod = CompressionMethodForEntry(Entry);

		const FIoBuffer* SourceBuffer = Entry->Request->GetSourceBuffer();
		Entry->UncompressedSize.Emplace(SourceBuffer->DataSize());

		check(WriterSettings.CompressionBlockSize > 0);
		const uint64 NumChunkBlocks64 = Align(Entry->UncompressedSize.GetValue(), WriterSettings.CompressionBlockSize) / WriterSettings.CompressionBlockSize;
		if (NumChunkBlocks64 == 0)
		{
			Entry->FinishCompressionBarrier->DispatchSubsequents();
			return;
		}

		int32 NumChunkBlocks = IntCastChecked<int32>(NumChunkBlocks64);
		
		Entry->ChunkBlocks.SetNum(NumChunkBlocks);
		{
			// We must allocate resources for our tasks up front to prevent resource deadlock.
			uint64 BytesToProcess = Entry->UncompressedSize.GetValue();
			const uint8* UncompressedData = SourceBuffer->Data();
			for (int32 BlockIndex = 0; BlockIndex < NumChunkBlocks; ++BlockIndex)
			{
				FChunkBlock& Block = Entry->ChunkBlocks[BlockIndex];
				Block.IoBuffer = WriterContext->AllocCompressionBuffer(NumChunkBlocks);
				Block.CompressionMethod = CompressionMethod;
				Block.UncompressedSize = FMath::Min(BytesToProcess, WriterSettings.CompressionBlockSize);
				Block.UncompressedData = UncompressedData;
				BytesToProcess -= Block.UncompressedSize;
				UncompressedData += Block.UncompressedSize;
			}
		}

		if (CompressionMethod == NAME_None)
		{
			for (FChunkBlock& Block : Entry->ChunkBlocks)
			{
				Block.CompressionMethod = NAME_None;
				Block.CompressedSize = Block.UncompressedSize;
				FMemory::Memcpy(Block.IoBuffer->Data(), Block.UncompressedData, Block.UncompressedSize);
			}
			Entry->FinishCompressionBarrier->DispatchSubsequents();
			return;
		}

		bool bUseDDCCompression =
			WriterContext->DDC &&
			WriterSettings.bCompressionEnableDDC &&
			Entry->UncompressedSize.GetValue() > WriterSettings.CompressionMinSizeToConsiderDDC;
		if (bUseDDCCompression)
		{
			TStringBuilder<256> CacheKeySuffix;
			CacheKeySuffix.Append(Entry->ChunkHash.ToString());
			WriterSettings.CompressionMethod.AppendString(CacheKeySuffix);
			CacheKeySuffix.Append(FCompression::GetCompressorDDCSuffix(WriterSettings.CompressionMethod));
			CacheKeySuffix.Appendf(TEXT("%d_%d"),
				WriterSettings.CompressionMinBytesSaved,
				WriterSettings.CompressionMinPercentSaved);
			Entry->DDCCacheKey = FDerivedDataCacheInterface::BuildCacheKey(
				TEXT("IoStoreCompression"),
				TEXT("985D3FAD-71D0-4758-A777-A910B49CC4BF"),
				CacheKeySuffix.ToString());

			TArray<uint8> DDCData;
			TRACE_CPUPROFILER_EVENT_SCOPE(ReadFromDDC);
			bool bFoundInDDC = false;
			if (WriterContext->DDC->GetSynchronous(*Entry->DDCCacheKey, DDCData, Entry->Options.FileName))
			{
				FLargeMemoryReader DDCDataReader(DDCData.GetData(), DDCData.Num());
				bFoundInDDC = SerializeCompressedDDCData(Entry, DDCDataReader);
			}
			if (bFoundInDDC)
			{
				WriterContext->CompressionDDCHitCount.IncrementExchange();
				Entry->FinishCompressionBarrier->DispatchSubsequents();
				return;
			}
			else
			{
				WriterContext->CompressionDDCMissCount.IncrementExchange();
				Entry->bStoreCompressedDataInDDC = true;
			}
		}
		
		for (FChunkBlock& Block : Entry->ChunkBlocks)
		{
			WriterContext->ScheduledCompressionTasksCount.IncrementExchange();
			FFunctionGraphTask::CreateAndDispatchWhenReady([this, Entry, BlockPtr = &Block]()
			{
				CompressBlock(BlockPtr);
				WriterContext->ScheduledCompressionTasksCount.DecrementExchange();
				int32 CompressedBlocksCount = Entry->CompressedBlocksCount.IncrementExchange();
				if (CompressedBlocksCount + 1 == Entry->ChunkBlocks.Num())
				{
					WriterContext->CompressedChunksByType[(int8)Entry->ChunkId.GetChunkType()].IncrementExchange();
					WriterContext->CompressedChunksCount.IncrementExchange();
					Entry->FinishCompressionBarrier->DispatchSubsequents();
				}
			}, TStatId(), nullptr, ENamedThreads::AnyHiPriThreadNormalTask);
		}
	}

	void BeginEncryptAndSign(FIoStoreWriteQueueEntry* Entry)
	{
		if (ContainerSettings.IsEncrypted() || ContainerSettings.IsSigned())
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady([this, Entry]()
			{
				EncryptAndSign(Entry);
				Entry->FinishEncryptionAndSigningBarrier->DispatchSubsequents();
			}, TStatId(), nullptr, ENamedThreads::AnyHiPriThreadHiPriTask);
		}
		else
		{
			EncryptAndSign(Entry);
			Entry->FinishEncryptionAndSigningBarrier->DispatchSubsequents();
		}
	}

	void EncryptAndSign(FIoStoreWriteQueueEntry* Entry)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EncryptAndSignChunk);
		for (FChunkBlock& Block : Entry->ChunkBlocks)
		{
			// Always align each compressed block to AES block size but store the compressed block size in the TOC
			Block.Size = Block.CompressedSize;
			if (!IsAligned(Block.Size, FAES::AESBlockSize))
			{
				uint64 AlignedCompressedBlockSize = Align(Block.Size, FAES::AESBlockSize);
				uint8* CompressedData = Block.IoBuffer->Data();
				for (uint64 FillIndex = Block.Size; FillIndex < AlignedCompressedBlockSize; ++FillIndex)
				{
					check(FillIndex < Block.IoBuffer->DataSize());
					CompressedData[FillIndex] = CompressedData[(FillIndex - Block.Size) % Block.Size];
				}
				Block.Size = AlignedCompressedBlockSize;
			}

			if (ContainerSettings.IsEncrypted())
			{
				FAES::EncryptData(Block.IoBuffer->Data(), static_cast<uint32>(Block.Size), ContainerSettings.EncryptionKey);
			}

			if (ContainerSettings.IsSigned())
			{
				FSHA1::HashBuffer(Block.IoBuffer->Data(), Block.Size, Block.Signature.Hash);
			}
		}
	}

	void BeginOnDemandData(FIoStoreWriteQueueEntry* Entry)
	{
		if (ContainerSettings.IsOnDemand())
		{
			// TODO: Worth threading this branch? (needs profiling on larger data sets)
			FIoHashBuilder HashBuilder;
			for (FChunkBlock& Block : Entry->ChunkBlocks)
			{
				check(Align(Block.CompressedSize, FAES::AESBlockSize) == Block.Size);

				// Note that the IoBuffer size is not the size of the data!
				FMemoryView BlockData = MakeMemoryView(Block.IoBuffer->Data(), Block.Size);
		
				HashBuilder.Update(BlockData);
				Block.DiskHash = FIoHash::HashBuffer(BlockData);
			}

			Entry->ChunkDiskHash = HashBuilder.Finalize();

			Entry->BeginWriteBarrier->DispatchSubsequents();
		}
		else
		{
			Entry->BeginWriteBarrier->DispatchSubsequents();
		}
	}

	void WriteEntry(FIoStoreWriteQueueEntry* Entry)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WriteEntry);
		const int32* FindExistingIndex = Toc.GetTocEntryIndex(Entry->ChunkId);
		if (FindExistingIndex)
		{
			checkf(Toc.GetTocResource().ChunkMetas[*FindExistingIndex].ChunkHash == Entry->ChunkHash, TEXT("Chunk id has already been added with different content"));
			for (FChunkBlock& ChunkBlock : Entry->ChunkBlocks)
			{
				WriterContext->FreeCompressionBuffer(ChunkBlock.IoBuffer, Entry->ChunkBlocks.Num());
				ChunkBlock.IoBuffer = nullptr;
			}
			return;
		}

		FPartition* TargetPartition = &Partitions[CurrentPartitionIndex];
		int32 NextPartitionIndexToTry = CurrentPartitionIndex + 1;
		if (Entry->PartitionIndex >= 0)
		{
			TargetPartition = &Partitions[Entry->PartitionIndex];
			if (TargetPartition->ReservedSpace > Entry->CompressedSize)
			{
				TargetPartition->ReservedSpace -= Entry->CompressedSize;
			}
			else
			{
				TargetPartition->ReservedSpace = 0;
			}
			NextPartitionIndexToTry = CurrentPartitionIndex;
		}

		const FIoStoreWriterSettings& WriterSettings = WriterContext->WriterSettings;
		bHasMemoryMappedEntry |= Entry->Options.bIsMemoryMapped;
		const uint64 ChunkAlignment = Entry->Options.bIsMemoryMapped ? WriterSettings.MemoryMappingAlignment : 0;
		const uint64 PartitionSizeLimit = WriterSettings.MaxPartitionSize > 0 ? WriterSettings.MaxPartitionSize : MAX_uint64;
		checkf(Entry->CompressedSize <= PartitionSizeLimit, TEXT("Chunk is too large, increase max partition size!"));
		for (;;)
		{
			uint64 OffsetBeforePadding = TargetPartition->Offset;
			if (ChunkAlignment)
			{
				TargetPartition->Offset = Align(TargetPartition->Offset, ChunkAlignment);
			}
			if (WriterSettings.CompressionBlockAlignment)
			{
				// Try and prevent entries from crossing compression alignment blocks if possible. This is to avoid
				// small entries from causing multiple file system block reads afaict. Large entries necesarily get
				// aligned to prevent things like a blocksize + 2 entry being at alignment -1, causing 3 low level reads.
				// ...I think.
				bool bCrossesBlockBoundary = Align(TargetPartition->Offset, WriterSettings.CompressionBlockAlignment) != Align(TargetPartition->Offset + Entry->CompressedSize - 1, WriterSettings.CompressionBlockAlignment);
				if (bCrossesBlockBoundary)
				{
					TargetPartition->Offset = Align(TargetPartition->Offset, WriterSettings.CompressionBlockAlignment);
				}
			}

			if (TargetPartition->Offset + Entry->CompressedSize + TargetPartition->ReservedSpace > PartitionSizeLimit)
			{
				TargetPartition->Offset = OffsetBeforePadding;
				while (Partitions.Num() <= NextPartitionIndexToTry)
				{
					FPartition& NewPartition = Partitions.AddDefaulted_GetRef();
					NewPartition.Index = Partitions.Num() - 1;
				}
				CurrentPartitionIndex = NextPartitionIndexToTry;
				TargetPartition = &Partitions[CurrentPartitionIndex];
				++NextPartitionIndexToTry;
			}
			else
			{
				Entry->Padding = TargetPartition->Offset - OffsetBeforePadding;
				TotalPaddingSize += Entry->Padding;
				break;
			}
		}

		if (!TargetPartition->ContainerFileHandle)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(CreatePartitionContainerFile);
			CreatePartitionContainerFile(*TargetPartition);
		}
		Entry->Offset = TargetPartition->Offset;

		FIoOffsetAndLength OffsetLength;
		OffsetLength.SetOffset(UncompressedFileOffset);
		OffsetLength.SetLength(Entry->UncompressedSize.GetValue());

		FIoStoreTocEntryMeta ChunkMeta{ Entry->ChunkHash, FIoStoreTocEntryMetaFlags::None };
		if (Entry->Options.bIsMemoryMapped)
		{
			ChunkMeta.Flags |= FIoStoreTocEntryMetaFlags::MemoryMapped;
		}

		uint64 OffsetInChunk = 0;
		for (const FChunkBlock& ChunkBlock : Entry->ChunkBlocks)
		{
			FIoStoreTocCompressedBlockEntry& BlockEntry = Toc.AddCompressionBlockEntry();
			BlockEntry.SetOffset(TargetPartition->Index * WriterSettings.MaxPartitionSize + TargetPartition->Offset + OffsetInChunk);
			OffsetInChunk += ChunkBlock.Size;
			BlockEntry.SetCompressedSize(uint32(ChunkBlock.CompressedSize));
			BlockEntry.SetUncompressedSize(uint32(ChunkBlock.UncompressedSize));
			BlockEntry.SetCompressionMethodIndex(Toc.AddCompressionMethodEntry(ChunkBlock.CompressionMethod));

			// We do this here so that we get the total size of data excluding the encryption alignment
			TotalEntryCompressedSize += ChunkBlock.CompressedSize;
			if (Entry->bCouldBeFromReferenceDb && !Entry->bLoadingFromReferenceDb)
			{
				ReferenceCacheMissBytes += ChunkBlock.CompressedSize;
			}

			if (!ChunkBlock.CompressionMethod.IsNone())
			{
				ChunkMeta.Flags |= FIoStoreTocEntryMetaFlags::Compressed;
			}

			if (ContainerSettings.IsSigned())
			{
				FSHAHash& Signature = Toc.AddBlockSignatureEntry();
				Signature = ChunkBlock.Signature;
			}

			if (ContainerSettings.IsOnDemand())
			{
				FIoStoreTocOnDemandCompressedBlockMeta& Meta = Toc.AddCompressionBlockMetaEntry();
				Meta.DiskHash = ChunkBlock.DiskHash;
			}
		}

		FIoStoreTocOnDemandChunkMeta OnDemandChunkMeta{ Entry->ChunkDiskHash };

		const int32 TocEntryIndex = Toc.AddChunkEntry(Entry->ChunkId, OffsetLength, ChunkMeta, OnDemandChunkMeta);
		check(TocEntryIndex != INDEX_NONE);

		if (ContainerSettings.IsIndexed() && Entry->Options.FileName.Len() > 0)
		{
			Toc.AddToFileIndex(Entry->ChunkId, Entry->Options.FileName);
		}

		const uint64 RegionStartOffset = TargetPartition->Offset;
		TargetPartition->Offset += Entry->CompressedSize;
		UncompressedFileOffset += Align(Entry->UncompressedSize.GetValue(), WriterSettings.CompressionBlockSize);
		TotalEntryUncompressedSize += Entry->UncompressedSize.GetValue();

		if (WriterSettings.bEnableFileRegions)
		{
			FFileRegion::AccumulateFileRegions(TargetPartition->AllFileRegions, RegionStartOffset, RegionStartOffset, TargetPartition->Offset, Entry->Request->GetRegions());
		}
		delete Entry->Request;
		Entry->Request = nullptr;
		uint64 WriteStartCycles = FPlatformTime::Cycles64();
		uint64 WriteBytes = 0;
		if (Entry->Padding > 0)
		{
			if (PaddingBuffer.Num() < Entry->Padding)
			{
				PaddingBuffer.SetNumZeroed(int32(Entry->Padding));
			}
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(WritePaddingToContainer);
				TargetPartition->ContainerFileHandle->Serialize(PaddingBuffer.GetData(), Entry->Padding);
				WriteBytes += Entry->Padding;
			}
		}
		check(Entry->Offset == TargetPartition->ContainerFileHandle->Tell());
		for (FChunkBlock& ChunkBlock : Entry->ChunkBlocks)
		{
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(WriteBlockToContainer);
				TargetPartition->ContainerFileHandle->Serialize(ChunkBlock.IoBuffer->Data(), ChunkBlock.Size);
				WriteBytes += ChunkBlock.Size;
			}
			WriterContext->FreeCompressionBuffer(ChunkBlock.IoBuffer, Entry->ChunkBlocks.Num());
			ChunkBlock.IoBuffer = nullptr;
		}
		uint64 WriteEndCycles = FPlatformTime::Cycles64();
		WriterContext->WriteCycleCount.AddExchange(WriteEndCycles - WriteStartCycles);
		WriterContext->WriteByteCount.AddExchange(WriteBytes);
		WriterContext->SerializedChunksCount.IncrementExchange();
	}

	const FString				ContainerPathAndBaseFileName;
	FIoStoreWriterContextImpl*	WriterContext = nullptr;
	FIoContainerSettings		ContainerSettings;
	FString						TocFilePath;
	FIoStoreToc					Toc;
	TArray<uint8>				PaddingBuffer;
	TArray<FPartition>			Partitions;
	TArray<FIoStoreWriteQueueEntry*> Entries;
	TArray<FLayoutEntry*>		LayoutEntries;
	FLayoutEntry*				LayoutEntriesHead = nullptr;
	FLayoutEntry*				LayoutEntriesTail = nullptr;
	TMap<FIoChunkId, FLayoutEntry*> PreviousBuildLayoutEntryByChunkId;
	TUniquePtr<FArchive>		CsvArchive;
	FIoStoreWriterResult		Result;
	uint64						UncompressedFileOffset = 0;
	uint64						TotalEntryUncompressedSize = 0; // sum of all entry source buffer sizes
	uint64						TotalEntryCompressedSize = 0; // entry compressed size excluding encryption alignment
	uint64						ReferenceCacheMissBytes = 0; // number of compressed bytes excluding alignment that could have been from refcache but weren't.
	uint64						TotalPaddingSize = 0;
	uint64						UncompressedContainerSize = 0; // this is the size the container would be if it were uncompressed.
	uint64						CompressedContainerSize = 0; // this is the size of the container with the given compression (which may be none).
	int32						CurrentPartitionIndex = 0;
	bool						bHasMemoryMappedEntry = false;
	bool						bHasFlushed = false;
	bool						bHasResult = false;
	TSharedPtr<IIoStoreWriterReferenceChunkDatabase> ReferenceChunkDatabase;
	TSharedPtr<IIoStoreWriterHashDatabase> HashDatabase;
	bool						bVerifyHashDatabase = false;


	friend class FIoStoreWriterContextImpl;
};

// InContainerPathAndBaseFileName: the utoc file will just be this with .utoc appended.
// The base filename ends up getting returned as the container name in the writer results.
TSharedPtr<IIoStoreWriter> FIoStoreWriterContextImpl::CreateContainer(const TCHAR* InContainerPathAndBaseFileName, const FIoContainerSettings& InContainerSettings)
{
	TSharedPtr<FIoStoreWriter> IoStoreWriter = MakeShared<FIoStoreWriter>(InContainerPathAndBaseFileName);
	FIoStatus IoStatus = IoStoreWriter->Initialize(*this, InContainerSettings);
	check(IoStatus.IsOk());
	IoStoreWriters.Add(IoStoreWriter);
	return IoStoreWriter;
}

void FIoStoreWriterContextImpl::Flush()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FIoStoreWriterContext::Flush);
	TArray<FIoStoreWriteQueueEntry*> AllEntries;
	for (TSharedPtr<FIoStoreWriter> IoStoreWriter : IoStoreWriters)
	{
		IoStoreWriter->bHasFlushed = true;
		AllEntries.Append(IoStoreWriter->Entries);
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaitForChunkHashes);
		for (int32 EntryIndex = AllEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(AllEntries[EntryIndex]->HashTask);
		}
	}
	for (TSharedPtr<FIoStoreWriter> IoStoreWriter : IoStoreWriters)
	{
		if (IoStoreWriter->LayoutEntriesHead)
		{
			IoStoreWriter->FinalizeLayout();
		}
	}
	// Update list of all entries after having the finilized layouts of each container
	AllEntries.Reset();
	for (TSharedPtr<FIoStoreWriter> IoStoreWriter : IoStoreWriters)
	{
		AllEntries.Append(IoStoreWriter->Entries);
	}

	double WritesStart = FPlatformTime::Seconds();

	for (FIoStoreWriteQueueEntry* Entry : AllEntries)
	{
		ScheduleCompression(Entry);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaitForWritesToComplete);
		for (int32 EntryIndex = AllEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
		{
			AllEntries[EntryIndex]->WriteFinishedEvent->Wait();
		}
	}

	double WritesEnd = FPlatformTime::Seconds();
	double WritesSeconds = FPlatformTime::ToSeconds64(WriteCycleCount.Load());
	UE_LOG(LogIoStore, Display, TEXT("Writing and compressing took %.2f seconds, writes to disk took %.2f seconds for %s bytes @ %s bytes per second."), 
		WritesEnd - WritesStart,
		WritesSeconds,
		*FText::AsNumber(WriteByteCount.Load()).ToString(),
		*FText::AsNumber((int64)((double)WriteByteCount.Load() / FMath::Max(.0001f, WritesSeconds))).ToString()
		);

	AllEntries.Empty();

	// Classically there were so few writers that this didn't need to be multi threaded, but it
	// involves writing files, and with content on demand this ends up being thousands of iterations. 
	double FinalizeStart = FPlatformTime::Seconds();	
	ParallelFor(TEXT("IoStoreWriter::Finalize.PF"), IoStoreWriters.Num(), 1, [this](int Index)
	{ 
		IoStoreWriters[Index]->Finalize(); 
	});
	double FinalizeEnd = FPlatformTime::Seconds();
	int64 TotalTocSize = 0;
	for (TSharedPtr<IIoStoreWriter> Writer : IoStoreWriters)
	{
		if (Writer->GetResult().IsOk())
		{
			TotalTocSize += Writer->GetResult().ValueOrDie().TocSize;
		}
	}

	UE_LOG(LogIoStore, Display, TEXT("Finalize took %.1f seconds for %d writers to write %s bytes, %s bytes per second"), 
		FinalizeEnd - FinalizeStart, 
		IoStoreWriters.Num(), 
		*FText::AsNumber(TotalTocSize).ToString(), 
		*FText::AsNumber((int64)((double)TotalTocSize / FMath::Max(.0001f, FinalizeEnd - FinalizeStart))).ToString()
		);
}

void FIoStoreWriterContextImpl::BeginCompressionThreadFunc()
{
	for (;;)
	{
		FIoStoreWriteQueueEntry* Entry = BeginCompressionQueue.DequeueOrWait();
		if (!Entry)
		{
			return;
		}
		while (Entry)
		{
			FIoStoreWriteQueueEntry* Next = Entry->Next;
			Entry->BeginCompressionBarrier->Wait();
			Entry->Writer->BeginCompress(Entry);
			BeginEncryptionAndSigningQueue.Enqueue(Entry);
			Entry = Next;
		}
	}
}

void FIoStoreWriterContextImpl::BeginEncryptionAndSigningThreadFunc()
{
	for (;;)
	{
		FIoStoreWriteQueueEntry* Entry = BeginEncryptionAndSigningQueue.DequeueOrWait();
		if (!Entry)
		{
			return;
		}
		while (Entry)
		{
			FIoStoreWriteQueueEntry* Next = Entry->Next;
			Entry->FinishCompressionBarrier->Wait();
			if (Entry->bLoadingFromReferenceDb == false)
			{
				Entry->Request->FreeSourceBuffer();
			}
			if (Entry->bStoreCompressedDataInDDC)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(StoreInDDC);
				TArray<uint8> DDCData;
				FMemoryWriter DDCDataWriter(DDCData, true);
				if (Entry->Writer->SerializeCompressedDDCData(Entry, DDCDataWriter))
				{
					DDC->Put(*Entry->DDCCacheKey, DDCData, Entry->Options.FileName);
				}
			}
			FinishEncryptionAndSigningQueue.Enqueue(Entry);
			Entry->Writer->BeginEncryptAndSign(Entry);
			Entry = Next;
		}
	}
}

void FIoStoreWriterContextImpl::FinishEncryptionAndSigningThreadFunc()
{
	for (;;)
	{
		FIoStoreWriteQueueEntry* Entry = FinishEncryptionAndSigningQueue.DequeueOrWait();
		if (!Entry)
		{
			return;
		}
		while (Entry)
		{
			FIoStoreWriteQueueEntry* Next = Entry->Next;
			Entry->FinishEncryptionAndSigningBarrier->Wait();
			OnDemandDataQueue.Enqueue(Entry);
			Entry->CompressedSize = 0;
			for (const FChunkBlock& ChunkBlock : Entry->ChunkBlocks)
			{
				Entry->CompressedSize += ChunkBlock.Size;
			}
			Entry->BeginOnDemandDataBarrier->DispatchSubsequents();

			Entry = Next;
		}
	}
}

void FIoStoreWriterContextImpl::BeginOnDemandDataThreadFunc()
{
	for (;;)
	{
		FIoStoreWriteQueueEntry* Entry = OnDemandDataQueue.DequeueOrWait();
		if (!Entry)
		{
			return;
		}

		while (Entry)
		{
			FIoStoreWriteQueueEntry* Next = Entry->Next;
			Entry->BeginOnDemandDataBarrier->Wait();

			WriterQueue.Enqueue(Entry);

			Entry->Writer->BeginOnDemandData(Entry);

			Entry = Next;
		}
	}
}

void FIoStoreWriterContextImpl::WriterThreadFunc()
{
	for (;;)
	{
		FIoStoreWriteQueueEntry* Entry = WriterQueue.DequeueOrWait();
		if (!Entry)
		{
			return;
		}
		while (Entry)
		{
			FIoStoreWriteQueueEntry* Next = Entry->Next;
			Entry->BeginWriteBarrier->Wait();
			Entry->Writer->WriteEntry(Entry);
			Entry->WriteFinishedEvent->DispatchSubsequents();
			Entry = Next;
		}
	}
}

class FIoStoreReaderImpl
{
public:
	FIoStoreReaderImpl()
	{

	}

	//
	// GenericPlatformFile isn't designed around a lot of jobs throwing accesses at it, so instead we 
	// use IFileHandle directly and round robin between a number of file handles in order to saturate
	// year 2022 ssd drives. For a file hot in the windows file cache, you can get 4+ GB/s with as few as 
	// 4 file handles, however for a cold file you need upwards of 32 in order to reach ~1.5 GB/s. This is
	// low because IoStoreReader (note: not IoDispatcher!) reads are comparatively small - at most you're reading compression block sized
	// chunks when uncompressed, however with Oodle those get cut by ~half, so with a default block size
	// of 64kb, reads are generally less than 32kb, which is tough to use and get full ssd bandwidth out of.
	//
	static constexpr uint32 NumHandlesPerFile = 12;
	struct FContainerFileAccess
	{
		FCriticalSection HandleLock[NumHandlesPerFile];
		IFileHandle* Handle[NumHandlesPerFile];
		std::atomic_uint32_t NextHandleIndex{0};
		bool bValid = false;

		FContainerFileAccess(IPlatformFile& Ipf, const TCHAR* ContainerFileName)
		{
			bValid = true;
			for (uint32 i=0; i < NumHandlesPerFile; i++)
			{
				Handle[i] = Ipf.OpenRead(ContainerFileName);
				if (Handle[i] == nullptr)
				{
					bValid = false;
				}
			}
		}

		~FContainerFileAccess()
		{
			for (int32 Index = 0; Index < NumHandlesPerFile; Index++)
			{
				if (Handle[Index] != nullptr)
				{
					delete Handle[Index];
					Handle[Index] = nullptr;
				}
			}
		}

		bool IsValid() const { return bValid; }
	};
	

	// Kick off an async read from the iostore container, rotating between the file handles for the partition.
	UE::Tasks::FTask StartAsyncRead(int32 InPartitionIndex, int64 InPartitionOffset, int64 InReadAmount, uint8* OutBuffer, std::atomic_bool* OutSuccess) const
	{
		return UE::Tasks::Launch(TEXT("FIoStoreReader_AsyncRead"), [this, InPartitionIndex, InPartitionOffset, OutBuffer, InReadAmount, OutSuccess]() mutable
		{
			FContainerFileAccess* ContainerFileAccess = this->ContainerFileAccessors[InPartitionIndex].Get();

			// Round robin between the file handles. Since we are always reading blocks, everything is ~roughly~ the same
			// size so we don't have to worry about a single huge read backing up one handle.
			uint32 OurIndex = ContainerFileAccess->NextHandleIndex.fetch_add(1);
			OurIndex %= NumHandlesPerFile;

			// Each file handle can only be touched by one task at a time. We use an OS lock so that the OS scheduler
			// knows we're in a wait state and who we're waiting on.
			//
			// CAUTION if any overload of IFileHandle launches tasks (... unlikely ...) this could deadlock if NumHandlesPerFile is more
			// than the number of worker threads, as the OS lock will not do task retraction.
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FIoStoreReader_StartAsyncRead_Lock);
				ContainerFileAccess->HandleLock[OurIndex].Lock();
			}

			bool bReadSucceeded;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FIoStoreReader_StartAsyncRead_SeekAndRead);
				ContainerFileAccess->Handle[OurIndex]->Seek(InPartitionOffset);
				bReadSucceeded = ContainerFileAccess->Handle[OurIndex]->Read(OutBuffer, InReadAmount);
			}

			OutSuccess->store(bReadSucceeded);
			ContainerFileAccess->HandleLock[OurIndex].Unlock();
		});
	}

	[[nodiscard]] FIoStatus Initialize(FStringView InContainerPath, const TMap<FGuid, FAES::FAESKey>& InDecryptionKeys)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FIoStoreReader::Initialize);
		ContainerPath = InContainerPath;

		TStringBuilder<256> TocFilePath;
		TocFilePath.Append(InContainerPath);
		TocFilePath.Append(TEXT(".utoc"));

		FIoStoreTocResource& TocResource = Toc.GetTocResource();
		FIoStatus TocStatus = FIoStoreTocResource::Read(*TocFilePath, EIoStoreTocReadOptions::ReadAll, TocResource);
		if (!TocStatus.IsOk())
		{
			return TocStatus;
		}

		Toc.Initialize();

		IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
		ContainerFileAccessors.Reserve(TocResource.Header.PartitionCount);
		for (uint32 PartitionIndex = 0; PartitionIndex < TocResource.Header.PartitionCount; ++PartitionIndex)
		{
			TStringBuilder<256> ContainerFilePath;
			ContainerFilePath.Append(InContainerPath);
			if (PartitionIndex > 0)
			{
				ContainerFilePath.Append(FString::Printf(TEXT("_s%d"), PartitionIndex));
			}
			ContainerFilePath.Append(TEXT(".ucas"));

			ContainerFileAccessors.Emplace(new FContainerFileAccess(Ipf, *ContainerFilePath));
			if (ContainerFileAccessors.Last().IsValid() == false)
			{
				return FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Failed to open IoStore container file '") << *TocFilePath << TEXT("'");
			}
		}

		if (EnumHasAnyFlags(TocResource.Header.ContainerFlags, EIoContainerFlags::Encrypted))
		{
			const FAES::FAESKey* FindKey = InDecryptionKeys.Find(TocResource.Header.EncryptionKeyGuid);
			if (!FindKey)
			{
				return FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Missing decryption key for IoStore container file '") << *TocFilePath << TEXT("'");
			}
			DecryptionKey = *FindKey;
		}

		if (EnumHasAnyFlags(TocResource.Header.ContainerFlags, EIoContainerFlags::Indexed) &&
			TocResource.DirectoryIndexBuffer.Num() > 0)
		{
			FIoStatus DirectoryIndexStatus = DirectoryIndexReader.Initialize(TocResource.DirectoryIndexBuffer, DecryptionKey);
			if (!DirectoryIndexStatus.IsOk())
			{
				return DirectoryIndexStatus;
			}
			DirectoryIndexReader.IterateDirectoryIndex(
				FIoDirectoryIndexHandle::RootDirectory(),
				TEXT(""),
				[this](FStringView Filename, uint32 TocEntryIndex) -> bool
				{
					ChunkFileNamesMap.Add(TocEntryIndex, FString(Filename));
					return true;
				});
		}

		return FIoStatus::Ok;
	}

	FIoContainerId GetContainerId() const
	{
		return Toc.GetTocResource().Header.ContainerId;
	}

	uint32 GetVersion() const
	{
		return Toc.GetTocResource().Header.Version;
	}

	EIoContainerFlags GetContainerFlags() const
	{
		return Toc.GetTocResource().Header.ContainerFlags;
	}

	FGuid GetEncryptionKeyGuid() const
	{
		return Toc.GetTocResource().Header.EncryptionKeyGuid;
	}

	void EnumerateChunks(TFunction<bool(FIoStoreTocChunkInfo&&)>&& Callback) const
	{
		const FIoStoreTocResource& TocResource = Toc.GetTocResource();

		for (int32 ChunkIndex = 0; ChunkIndex < TocResource.ChunkIds.Num(); ++ChunkIndex)
		{
			FIoStoreTocChunkInfo ChunkInfo = Toc.GetTocChunkInfo(ChunkIndex, &ChunkFileNamesMap);
			if (!Callback(MoveTemp(ChunkInfo)))
			{
				break;
			}
		}
	}

	TIoStatusOr<FIoStoreTocChunkInfo> GetChunkInfo(const FIoChunkId& ChunkId) const
	{
		const int32* TocEntryIndex = Toc.GetTocEntryIndex(ChunkId);
		if (TocEntryIndex)
		{
			return Toc.GetTocChunkInfo(*TocEntryIndex, &ChunkFileNamesMap);
		}
		else
		{
			return FIoStatus(EIoErrorCode::NotFound, TEXT("Not found"));
		}
	}

	TIoStatusOr<FIoStoreTocChunkInfo> GetChunkInfo(const uint32 TocEntryIndex) const
	{
		const FIoStoreTocResource& TocResource = Toc.GetTocResource();

		if (TocEntryIndex < uint32(TocResource.ChunkIds.Num()))
		{
			return Toc.GetTocChunkInfo(TocEntryIndex, &ChunkFileNamesMap);
		}
		else
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid TocEntryIndex"));
		}
	}

	TIoStatusOr<FIoStoreCompressedChunkInfo> GetChunkCompressedInfo(const FIoChunkId& ChunkId) const
	{
		const int32* TocEntryIndex = Toc.GetTocEntryIndex(ChunkId);
		if (TocEntryIndex != nullptr)
		{
			// Find where in the virtual file the chunk exists.
			const FIoOffsetAndLength* OffsetAndLength = Toc.GetOffsetAndLength(ChunkId);
			if (!OffsetAndLength)
			{
				return FIoStatus(EIoErrorCode::NotFound, TEXT("Unknown chunk ID"));
			}

			const uint64 ResolvedOffset = OffsetAndLength->GetOffset();
			const uint64 ResolvedSize = OffsetAndLength->GetLength();

			// Find what compressed blocks this read straddles.
			const FIoStoreTocResource& TocResource = Toc.GetTocResource();
			const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;

			const int32 FirstBlockIndex = int32(ResolvedOffset / CompressionBlockSize);
			const int32 LastBlockIndex = int32((Align(ResolvedOffset + ResolvedSize, CompressionBlockSize) - 1) / CompressionBlockSize);

			// Determine size of the result and set up output buffers
			uint64 TotalCompressedSize = 0;
			for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
			{
				const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
				TotalCompressedSize += CompressionBlock.GetCompressedSize();
			}

			FIoStoreCompressedChunkInfo Info;
			if (!TocResource.OnDemandChunkMeta.IsEmpty())
			{
				Info.DiskHash = TocResource.OnDemandChunkMeta[*TocEntryIndex].DiskHash;
			}

			Info.Blocks.Reserve(LastBlockIndex + 1 - FirstBlockIndex);
			Info.UncompressedOffset = ResolvedOffset % CompressionBlockSize;
			Info.UncompressedSize = ResolvedSize;
			Info.TotalCompressedSize = TotalCompressedSize;

			// Set up the result blocks.
			uint64 CurrentOffset = 0;
			for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
			{
				const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
				FIoStoreCompressedBlockInfo& BlockInfo = Info.Blocks.AddDefaulted_GetRef();
				if (!TocResource.OnDemandCompressedBlockMeta.IsEmpty())
				{
					BlockInfo.DiskHash = TocResource.OnDemandCompressedBlockMeta[BlockIndex].DiskHash;
				}
				BlockInfo.CompressionMethod = TocResource.CompressionMethods[CompressionBlock.GetCompressionMethodIndex()];
				BlockInfo.CompressedSize = CompressionBlock.GetCompressedSize();
				BlockInfo.UncompressedSize = CompressionBlock.GetUncompressedSize();
				BlockInfo.OffsetInBuffer = CurrentOffset;
				BlockInfo.AlignedSize = Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);
				CurrentOffset += BlockInfo.AlignedSize;
			}

			return Info;
		}
		else
		{
			return FIoStatus(EIoErrorCode::NotFound, TEXT("Not found"));
		}
	}

	UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> ReadAsync(const FIoChunkId& ChunkId, const FIoReadOptions& Options) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ReadChunkAsync);

		struct FState
		{
			TArray64<uint8> CompressedBuffer;
			uint64 CompressedSize = 0;
			uint64 UncompressedSize = 0;
			TOptional<FIoBuffer> UncompressedBuffer;
			std::atomic_bool bReadSucceeded {false};
			std::atomic_bool bUncompressFailed { false };
		};

		const FIoOffsetAndLength* OffsetAndLength = Toc.GetOffsetAndLength(ChunkId);
		if (!OffsetAndLength )
		{
			// Currently there's no way to make a task with a valid result that just emplaces
			// without running.
			return UE::Tasks::Launch(TEXT("FIoStoreRead_Error"), 
				[] { return TIoStatusOr<FIoBuffer>(FIoStatus(EIoErrorCode::NotFound, TEXT("Unknown chunk ID"))); },
				UE::Tasks::ETaskPriority::Normal,
				UE::Tasks::EExtendedTaskPriority::Inline); // force execution on this thread
		}

		const uint64 RequestedOffset = Options.GetOffset();
		const uint64 ResolvedOffset = OffsetAndLength->GetOffset() + RequestedOffset;
		const uint64 ResolvedSize = RequestedOffset <= OffsetAndLength->GetLength() ? FMath::Min(Options.GetSize(), OffsetAndLength->GetLength() - RequestedOffset) : 0;
		const FIoStoreTocResource& TocResource = Toc.GetTocResource();
		const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;
		const int32 FirstBlockIndex = int32(ResolvedOffset / CompressionBlockSize);
		const int32 LastBlockIndex = int32((Align(ResolvedOffset + ResolvedSize, CompressionBlockSize) - 1) / CompressionBlockSize);
		const int32 BlockCount = LastBlockIndex - FirstBlockIndex + 1;
		if (!BlockCount)
		{
			// Currently there's no way to make a task with a valid result that just emplaces
			// without running.
			return UE::Tasks::Launch(TEXT("FIoStoreRead_Empty"),
				[] { return TIoStatusOr<FIoBuffer>(); },
				UE::Tasks::ETaskPriority::Normal,
				UE::Tasks::EExtendedTaskPriority::Inline); // force execution on this thread
		}
		const FIoStoreTocCompressedBlockEntry& FirstBlock = TocResource.CompressionBlocks[FirstBlockIndex];
		const FIoStoreTocCompressedBlockEntry& LastBlock = TocResource.CompressionBlocks[LastBlockIndex];
		const int32 PartitionIndex = static_cast<int32>(FirstBlock.GetOffset() / TocResource.Header.PartitionSize);
		check(static_cast<int32>(LastBlock.GetOffset() / TocResource.Header.PartitionSize) == PartitionIndex);
		const uint64 ReadStartOffset = FirstBlock.GetOffset() % TocResource.Header.PartitionSize;
		const uint64 ReadEndOffset = (LastBlock.GetOffset() + Align(LastBlock.GetCompressedSize(), FAES::AESBlockSize)) % TocResource.Header.PartitionSize;
		FState* State = new FState();
		State->CompressedSize = ReadEndOffset - ReadStartOffset;
		State->UncompressedSize = ResolvedSize;
		State->CompressedBuffer.AddUninitialized(State->CompressedSize);
		State->UncompressedBuffer.Emplace(State->UncompressedSize);

		UE::Tasks::FTask ReadJob = StartAsyncRead(PartitionIndex, ReadStartOffset, (int32)State->CompressedSize, State->CompressedBuffer.GetData(), &State->bReadSucceeded);

		UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> ReturnTask = UE::Tasks::Launch(TEXT("FIoStoreReader::AsyncRead"), [this, State, PartitionIndex, CompressionBlockSize, ResolvedOffset, FirstBlockIndex, LastBlockIndex, ResolvedSize, ReadStartOffset, &TocResource]()
		{			
			UE::Tasks::FTaskEvent DecompressionDoneEvent(TEXT("FIoStoreReader::DecompressionDone"));

			std::atomic_int32_t DecompressionJobsRemaining = LastBlockIndex - FirstBlockIndex + 1;

			uint64 CompressedSourceOffset = 0;
			uint64 UncompressedDestinationOffset = 0;
			uint64 OffsetInBlock = ResolvedOffset % CompressionBlockSize;
			uint64 RemainingSize = ResolvedSize;
			for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
			{
				UE::Tasks::FTask DecompressBlockTask = UE::Tasks::Launch(TEXT("FIoStoreReader::Decompress"), [this, State, BlockIndex, CompressedSourceOffset, UncompressedDestinationOffset, OffsetInBlock, RemainingSize, &DecompressionDoneEvent, &DecompressionJobsRemaining]()
				{
					if (State->bReadSucceeded)
					{
						uint8* CompressedSource = State->CompressedBuffer.GetData() + CompressedSourceOffset;
						uint8* UncompressedDestination = State->UncompressedBuffer->Data() + UncompressedDestinationOffset;
						const FIoStoreTocResource& TocResource = Toc.GetTocResource();
						const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
						const uint32 RawSize = Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);
						const uint32 UncompressedSize = CompressionBlock.GetUncompressedSize();
						FName CompressionMethod = TocResource.CompressionMethods[CompressionBlock.GetCompressionMethodIndex()];
						if (EnumHasAnyFlags(TocResource.Header.ContainerFlags, EIoContainerFlags::Encrypted))
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(Decrypt);
							check(CompressedSource + RawSize <= State->CompressedBuffer.GetData() + State->CompressedSize);
							FAES::DecryptData(CompressedSource, RawSize, DecryptionKey);
						}
						if (CompressionMethod.IsNone())
						{
							check(UncompressedDestination + UncompressedSize - OffsetInBlock <= State->UncompressedBuffer->Data() + State->UncompressedBuffer->DataSize());
							FMemory::Memcpy(UncompressedDestination, CompressedSource + OffsetInBlock, UncompressedSize - OffsetInBlock);
						}
						else
						{
							bool bUncompressed;
							if (OffsetInBlock || RemainingSize < UncompressedSize)
							{
								TArray<uint8> TempBuffer;
								TempBuffer.SetNumUninitialized(UncompressedSize);
								bUncompressed = FCompression::UncompressMemory(CompressionMethod, TempBuffer.GetData(), UncompressedSize, CompressedSource, CompressionBlock.GetCompressedSize());
								uint64 CopySize = FMath::Min<uint64>(UncompressedSize - OffsetInBlock, RemainingSize);
								FMemory::Memcpy(UncompressedDestination, TempBuffer.GetData() + OffsetInBlock, CopySize);
							}
							else
							{
								check(UncompressedDestination + UncompressedSize <= State->UncompressedBuffer->Data() + State->UncompressedBuffer->DataSize());
								bUncompressed = FCompression::UncompressMemory(CompressionMethod, UncompressedDestination, UncompressedSize, CompressedSource, CompressionBlock.GetCompressedSize());
							}
							if (!bUncompressed)
							{
								State->bUncompressFailed = true;
							}
						}
					} // end if read succeeded

					if (DecompressionJobsRemaining.fetch_add(-1) == 1)
					{
						DecompressionDoneEvent.Trigger();
					}
				}); // end decompression lambda

				const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
				const uint32 RawSize = Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);
				CompressedSourceOffset += RawSize;
				UncompressedDestinationOffset += CompressionBlock.GetUncompressedSize();
				RemainingSize -= CompressionBlock.GetUncompressedSize();
				OffsetInBlock = 0;
			} // end for each block

			// Wait for everything
			DecompressionDoneEvent.BusyWait();

			TIoStatusOr<FIoBuffer> Result;
			if (State->bReadSucceeded == false)
			{
				Result = FIoStatus(EIoErrorCode::ReadError, TEXT("Failed reading chunk from container file"));
			}
			else if (State->bUncompressFailed)
			{
				Result = FIoStatus(EIoErrorCode::ReadError, TEXT("Failed uncompressing chunk"));
			}
			else
			{
				Result = State->UncompressedBuffer.GetValue();
			}
			delete State;

			return Result;
		}, UE::Tasks::Prerequisites(ReadJob)); // end read and compress lambda launch

		return ReturnTask;
	} // end ReadAsync

	TIoStatusOr<FIoBuffer> Read(const FIoChunkId& ChunkId, const FIoReadOptions& Options) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ReadChunk);

		const FIoOffsetAndLength* OffsetAndLength = Toc.GetOffsetAndLength(ChunkId);
		if (!OffsetAndLength)
		{
			return FIoStatus(EIoErrorCode::NotFound, TEXT("Unknown chunk ID"));
		}

		uint64 RequestedOffset = Options.GetOffset();
		uint64 ResolvedOffset = OffsetAndLength->GetOffset() + RequestedOffset;
		uint64 ResolvedSize = 0;
		if (RequestedOffset <= OffsetAndLength->GetLength())
		{
			ResolvedSize = FMath::Min(Options.GetSize(), OffsetAndLength->GetLength() - RequestedOffset);
		}

		const FIoStoreTocResource& TocResource = Toc.GetTocResource();
		const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;
		FIoBuffer UncompressedBuffer(ResolvedSize);
		if (ResolvedSize == 0)
		{
			return UncompressedBuffer;
		}

		// From here on we are reading / decompressing at least one block.

		// We try to overlap the IO for the next block with the decrypt/decompress for the current
		// block, which requires two IO buffers:
		TArray<uint8> CompressedBuffers[2];
		std::atomic_bool AsyncReadSucceeded[2];

		int32 FirstBlockIndex = int32(ResolvedOffset / CompressionBlockSize);
		int32 LastBlockIndex = int32((Align(ResolvedOffset + ResolvedSize, CompressionBlockSize) - 1) / CompressionBlockSize);

		// Lambda to kick off a read with a sufficient output buffer.
		auto LaunchBlockRead = [&TocResource, this](int32 BlockIndex, TArray<uint8>& DestinationBuffer, std::atomic_bool* OutReadSucceeded)
		{
			const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;
			const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];

			// CompressionBlockSize is technically the _uncompresseed_ block size, however it's a good
			// size to use for reuse as block compression can vary wildly and we want to be able to
			// read blocks that happen to be uncompressed.
			uint32 SizeForDecrypt = Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);
			uint32 CompressedBufferSizeNeeded = FMath::Max(uint32(CompressionBlockSize), SizeForDecrypt);

			if (uint32(DestinationBuffer.Num()) < CompressedBufferSizeNeeded)
			{
				DestinationBuffer.SetNumUninitialized(CompressedBufferSizeNeeded);
			}

			int32 PartitionIndex = int32(CompressionBlock.GetOffset() / TocResource.Header.PartitionSize);
			int64 PartitionOffset = int64(CompressionBlock.GetOffset() % TocResource.Header.PartitionSize);
			return StartAsyncRead(PartitionIndex, PartitionOffset, SizeForDecrypt, DestinationBuffer.GetData(), OutReadSucceeded);
		};


		// Kick off the first async read
		UE::Tasks::FTask NextReadRequest;
		uint8 NextReadBufferIndex = 0;
		NextReadRequest = LaunchBlockRead(FirstBlockIndex, CompressedBuffers[NextReadBufferIndex], &AsyncReadSucceeded[NextReadBufferIndex]);

		uint64 UncompressedDestinationOffset = 0;
		uint64 OffsetInBlock = ResolvedOffset % CompressionBlockSize;
		uint64 RemainingSize = ResolvedSize;
		TArray<uint8> TempBuffer;
		for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
		{
			// Kick off the next block's IO if there is one
			UE::Tasks::FTask ReadRequest(MoveTemp(NextReadRequest));
			uint8 OurBufferIndex = NextReadBufferIndex;
			if (BlockIndex + 1 <= LastBlockIndex)
			{
				NextReadBufferIndex = NextReadBufferIndex ^ 1;
				NextReadRequest = LaunchBlockRead(BlockIndex + 1, CompressedBuffers[NextReadBufferIndex], &AsyncReadSucceeded[NextReadBufferIndex]);
			}

			// Now, wait for _our_ block's IO
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(WaitForIo);
				ReadRequest.BusyWait();
			}

			if (AsyncReadSucceeded[OurBufferIndex] == false)
			{
				return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed async read in FIoStoreReader::ReadCompressed"));
			}

			const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];

			// This also happened in the LaunchBlockRead call, so we know the buffer has the necessary size.
			uint32 RawSize = Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);
			if (EnumHasAnyFlags(TocResource.Header.ContainerFlags, EIoContainerFlags::Encrypted))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Decrypt);
				FAES::DecryptData(CompressedBuffers[OurBufferIndex].GetData(), RawSize, DecryptionKey);
			}

			FName CompressionMethod = TocResource.CompressionMethods[CompressionBlock.GetCompressionMethodIndex()];
			uint8* UncompressedDestination = UncompressedBuffer.Data() + UncompressedDestinationOffset;
			const uint32 UncompressedSize = CompressionBlock.GetUncompressedSize();
			if (CompressionMethod.IsNone())
			{
				uint64 CopySize = FMath::Min<uint64>(UncompressedSize - OffsetInBlock, RemainingSize);
				check(UncompressedDestination + CopySize <= UncompressedBuffer.Data() + UncompressedBuffer.DataSize());
				FMemory::Memcpy(UncompressedDestination, CompressedBuffers[OurBufferIndex].GetData() + OffsetInBlock, CopySize);
				UncompressedDestinationOffset += CopySize;
				RemainingSize -= CopySize;
			}
			else
			{
				bool bUncompressed;
				if (OffsetInBlock || RemainingSize < UncompressedSize)
				{
					// If this block is larger than the amount of data actually requested, decompress to a temp
					// buffer and then copy out. Should never happen when reading the entire chunk.
					TempBuffer.SetNumUninitialized(UncompressedSize);
					bUncompressed = FCompression::UncompressMemory(CompressionMethod, TempBuffer.GetData(), UncompressedSize, CompressedBuffers[OurBufferIndex].GetData(), CompressionBlock.GetCompressedSize());
					uint64 CopySize = FMath::Min<uint64>(UncompressedSize - OffsetInBlock, RemainingSize);
					check(UncompressedDestination + CopySize <= UncompressedBuffer.Data() + UncompressedBuffer.DataSize());
					FMemory::Memcpy(UncompressedDestination, TempBuffer.GetData() + OffsetInBlock, CopySize);
					UncompressedDestinationOffset += CopySize;
					RemainingSize -= CopySize;
				}
				else
				{
					check(UncompressedDestination + UncompressedSize <= UncompressedBuffer.Data() + UncompressedBuffer.DataSize());
					bUncompressed = FCompression::UncompressMemory(CompressionMethod, UncompressedDestination, UncompressedSize, CompressedBuffers[OurBufferIndex].GetData(), CompressionBlock.GetCompressedSize());
					UncompressedDestinationOffset += UncompressedSize;
					RemainingSize -= UncompressedSize;
				}
				if (!bUncompressed)
				{
					return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed uncompressing chunk"));
				}
			}
			OffsetInBlock = 0;
		}
		return UncompressedBuffer;
	}

	TIoStatusOr<FIoStoreCompressedReadResult> ReadCompressed(const FIoChunkId& ChunkId, const FIoReadOptions& Options, bool bDecrypt) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ReadChunkCompressed);

		// Find where in the virtual file the chunk exists.
		const FIoOffsetAndLength* OffsetAndLength = Toc.GetOffsetAndLength(ChunkId);
		if (!OffsetAndLength)
		{
			return FIoStatus(EIoErrorCode::NotFound, TEXT("Unknown chunk ID"));
		}

		// Combine with offset/size requested by the reader.
		uint64 RequestedOffset = Options.GetOffset();
		uint64 ResolvedOffset = OffsetAndLength->GetOffset() + RequestedOffset;
		uint64 ResolvedSize = 0;
		if (RequestedOffset <= OffsetAndLength->GetLength())
		{
			ResolvedSize = FMath::Min(Options.GetSize(), OffsetAndLength->GetLength() - RequestedOffset);
		}

		// Find what compressed blocks this read straddles.
		const FIoStoreTocResource& TocResource = Toc.GetTocResource();
		const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;
		int32 FirstBlockIndex = int32(ResolvedOffset / CompressionBlockSize);
		int32 LastBlockIndex = int32((Align(ResolvedOffset + ResolvedSize, CompressionBlockSize) - 1) / CompressionBlockSize);

		// Determine size of the result and set up output buffers
		uint64 TotalCompressedSize = 0;
		uint64 TotalAlignedSize = 0;
		for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
		{
			const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
			TotalCompressedSize += CompressionBlock.GetCompressedSize();
			TotalAlignedSize += Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);
		}

		FIoStoreCompressedReadResult Result;
		Result.IoBuffer = FIoBuffer(TotalAlignedSize);
		Result.Blocks.Reserve(LastBlockIndex + 1 - FirstBlockIndex);
		Result.UncompressedOffset = ResolvedOffset % CompressionBlockSize;
		Result.UncompressedSize = ResolvedSize;
		Result.TotalCompressedSize = TotalCompressedSize;

		// Set up the result blocks.
		uint64 CurrentOffset = 0;
		for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
		{
			const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
			FIoStoreCompressedBlockInfo& BlockInfo = Result.Blocks.AddDefaulted_GetRef();
			if (!TocResource.OnDemandCompressedBlockMeta.IsEmpty())
			{
				BlockInfo.DiskHash = TocResource.OnDemandCompressedBlockMeta[BlockIndex].DiskHash;
			}
			BlockInfo.CompressionMethod = TocResource.CompressionMethods[CompressionBlock.GetCompressionMethodIndex()];
			BlockInfo.CompressedSize = CompressionBlock.GetCompressedSize();
			BlockInfo.UncompressedSize = CompressionBlock.GetUncompressedSize();
			BlockInfo.OffsetInBuffer = CurrentOffset;
			BlockInfo.AlignedSize = Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);
			CurrentOffset += BlockInfo.AlignedSize;
		}

		uint8* OutputBuffer = Result.IoBuffer.Data();

		// We can read the entire thing at once since we obligate the caller to skip the alignment padding.
		{
			const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[FirstBlockIndex];
			int32 PartitionIndex = int32(CompressionBlock.GetOffset() / TocResource.Header.PartitionSize);
			int64 PartitionOffset = int64(CompressionBlock.GetOffset() % TocResource.Header.PartitionSize);

			std::atomic_bool bReadSucceeded;
			UE::Tasks::FTask ReadTask = StartAsyncRead(PartitionIndex, PartitionOffset, TotalAlignedSize, OutputBuffer, &bReadSucceeded);

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(WaitForIo);
				ReadTask.BusyWait();
			}

			if (bReadSucceeded == false)
			{
				UE_LOG(LogIoStore, Error, TEXT("Read from container %s failed (partition %d, offset %lld, size %d)"), *ContainerPath, PartitionIndex, PartitionOffset, TotalAlignedSize);
				return FIoStoreCompressedReadResult();
			}
		}

		if (bDecrypt && EnumHasAnyFlags(TocResource.Header.ContainerFlags, EIoContainerFlags::Encrypted) )
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Decrypt);

			for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
			{
				FIoStoreCompressedBlockInfo& OutputBlock = Result.Blocks[BlockIndex - FirstBlockIndex];
				uint8* Buffer = OutputBuffer + OutputBlock.OffsetInBuffer;
				FAES::DecryptData(Buffer, OutputBlock.AlignedSize, DecryptionKey);
			}
		}
		return Result;
	}

	const FIoDirectoryIndexReader& GetDirectoryIndexReader() const
	{
		return DirectoryIndexReader;
	}

	bool TocChunkContainsBlockIndex(const int32 TocEntryIndex, const int32 BlockIndex) const
	{
		const FIoStoreTocResource& TocResource = Toc.GetTocResource();
		const FIoOffsetAndLength& OffsetLength = TocResource.ChunkOffsetLengths[TocEntryIndex];

		const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;
		int32 FirstBlockIndex = int32(OffsetLength.GetOffset() / CompressionBlockSize);
		int32 LastBlockIndex = int32((Align(OffsetLength.GetOffset() + OffsetLength.GetLength(), CompressionBlockSize) - 1) / CompressionBlockSize);

		return BlockIndex >= FirstBlockIndex && BlockIndex <= LastBlockIndex;
	}

	uint32 GetCompressionBlockSize() const
	{
		return Toc.GetTocResource().Header.CompressionBlockSize;
	}
	
	const TArray<FName>& GetCompressionMethods() const
	{
		return Toc.GetTocResource().CompressionMethods;
	}

	bool EnumerateCompressedBlocksForChunk(const FIoChunkId& ChunkId, TFunction<bool(const FIoStoreTocCompressedBlockInfo&)>&& Callback) const
	{
		const FIoOffsetAndLength* OffsetAndLength = Toc.GetOffsetAndLength(ChunkId);
		if (!OffsetAndLength)
		{
			return false;
		}

		// Find what compressed blocks this chunk straddles.
		const FIoStoreTocResource& TocResource = Toc.GetTocResource();
		const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;
		int32 FirstBlockIndex = int32(OffsetAndLength->GetOffset() / CompressionBlockSize);
		int32 LastBlockIndex = int32((Align(OffsetAndLength->GetOffset() + OffsetAndLength->GetLength(), CompressionBlockSize) - 1) / CompressionBlockSize);

		for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
		{
			const FIoStoreTocCompressedBlockEntry& Entry = TocResource.CompressionBlocks[BlockIndex];
			FIoStoreTocCompressedBlockInfo Info
			{
				Entry.GetOffset(),
				Entry.GetCompressedSize(),
				Entry.GetUncompressedSize(),
				Entry.GetCompressionMethodIndex()
			};
			if (!Callback(Info))
			{
				break;
			}
		}
		return true;
	}

	void EnumerateCompressedBlocks(TFunction<bool(const FIoStoreTocCompressedBlockInfo&)>&& Callback) const
	{
		const FIoStoreTocResource& TocResource = Toc.GetTocResource();

		for (int32 BlockIndex = 0; BlockIndex < TocResource.CompressionBlocks.Num(); ++BlockIndex)
		{
			const FIoStoreTocCompressedBlockEntry& Entry = TocResource.CompressionBlocks[BlockIndex];
			FIoStoreTocCompressedBlockInfo Info
			{
				Entry.GetOffset(),
				Entry.GetCompressedSize(),
				Entry.GetUncompressedSize(),
				Entry.GetCompressionMethodIndex()
			};
			if (!Callback(Info))
			{
				break;
			}
		}
	}

	void GetContainerFilePaths(TArray<FString>& OutPaths)
	{
		TStringBuilder<256> Sb;

		for (uint32 PartitionIndex = 0; PartitionIndex < Toc.GetTocResource().Header.PartitionCount; ++PartitionIndex)
		{
			Sb.Reset();
			Sb.Append(ContainerPath);
			if (PartitionIndex > 0)
			{
				Sb.Append(FString::Printf(TEXT("_s%d"), PartitionIndex));
			}
			Sb.Append(TEXT(".ucas"));
			OutPaths.Add(Sb.ToString());
		}
	}

private:


	FIoStoreToc Toc;
	FAES::FAESKey DecryptionKey;
	TArray<TUniquePtr<FContainerFileAccess>> ContainerFileAccessors;
	FString ContainerPath;
	FIoDirectoryIndexReader DirectoryIndexReader;
	TMap<int32, FString> ChunkFileNamesMap;
};

FIoStoreReader::FIoStoreReader()
	: Impl(new FIoStoreReaderImpl())
{
}

FIoStoreReader::~FIoStoreReader()
{
	delete Impl;
}

FIoStatus FIoStoreReader::Initialize(FStringView InContainerPath, const TMap<FGuid, FAES::FAESKey>& InDecryptionKeys)
{
	return Impl->Initialize(InContainerPath, InDecryptionKeys);
}

FIoContainerId FIoStoreReader::GetContainerId() const
{
	return Impl->GetContainerId();
}

uint32 FIoStoreReader::GetVersion() const
{
	return Impl->GetVersion();
}

EIoContainerFlags FIoStoreReader::GetContainerFlags() const
{
	return Impl->GetContainerFlags();
}

FGuid FIoStoreReader::GetEncryptionKeyGuid() const
{
	return Impl->GetEncryptionKeyGuid();
}

void FIoStoreReader::EnumerateChunks(TFunction<bool(FIoStoreTocChunkInfo&&)>&& Callback) const
{
	Impl->EnumerateChunks(MoveTemp(Callback));
}

TIoStatusOr<FIoStoreTocChunkInfo> FIoStoreReader::GetChunkInfo(const FIoChunkId& Chunk) const
{
	return Impl->GetChunkInfo(Chunk);
}

TIoStatusOr<FIoStoreTocChunkInfo> FIoStoreReader::GetChunkInfo(const uint32 TocEntryIndex) const
{
	return Impl->GetChunkInfo(TocEntryIndex);
}

TIoStatusOr<FIoStoreCompressedChunkInfo> FIoStoreReader::GetChunkCompressedInfo(const FIoChunkId& Chunk) const
{
	return Impl->GetChunkCompressedInfo(Chunk);
}

TIoStatusOr<FIoBuffer> FIoStoreReader::Read(const FIoChunkId& Chunk, const FIoReadOptions& Options) const
{
	return Impl->Read(Chunk, Options);
}

TIoStatusOr<FIoStoreCompressedReadResult> FIoStoreReader::ReadCompressed(const FIoChunkId& Chunk, const FIoReadOptions& Options, bool bDecrypt) const
{
	return Impl->ReadCompressed(Chunk, Options, bDecrypt);
}

UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> FIoStoreReader::ReadAsync(const FIoChunkId& Chunk, const FIoReadOptions& Options) const
{
	return Impl->ReadAsync(Chunk, Options);
}

const FIoDirectoryIndexReader& FIoStoreReader::GetDirectoryIndexReader() const
{
	return Impl->GetDirectoryIndexReader();
}

uint32 FIoStoreReader::GetCompressionBlockSize() const
{
	return Impl->GetCompressionBlockSize();
}

const TArray<FName>& FIoStoreReader::GetCompressionMethods() const
{
	return Impl->GetCompressionMethods();
}

void FIoStoreReader::EnumerateCompressedBlocks(TFunction<bool(const FIoStoreTocCompressedBlockInfo&)>&& Callback) const
{
	Impl->EnumerateCompressedBlocks(MoveTemp(Callback));
}

void FIoStoreReader::EnumerateCompressedBlocksForChunk(const FIoChunkId& Chunk, TFunction<bool(const FIoStoreTocCompressedBlockInfo&)>&& Callback) const
{
	Impl->EnumerateCompressedBlocksForChunk(Chunk, MoveTemp(Callback));
}

void FIoStoreReader::GetContainerFilePaths(TArray<FString>& OutPaths)
{
	Impl->GetContainerFilePaths(OutPaths);
}

FIoStatus FIoStoreTocResource::Read(const TCHAR* TocFilePath, EIoStoreTocReadOptions ReadOptions, FIoStoreTocResource& OutTocResource)
{
	check(TocFilePath != nullptr);

	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	TUniquePtr<IFileHandle>	TocFileHandle(Ipf.OpenRead(TocFilePath, /* allowwrite */ false));

	if (!TocFileHandle)
	{
		return FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Failed to open IoStore TOC file '") << TocFilePath << TEXT("'");
	}

	// Header
	FIoStoreTocHeader& Header = OutTocResource.Header;
	if (!TocFileHandle->Read(reinterpret_cast<uint8*>(&Header), sizeof(FIoStoreTocHeader)))
	{
		return FIoStatusBuilder(EIoErrorCode::CorruptToc) << TEXT("Failed to read IoStore TOC file '") << TocFilePath << TEXT("'");
	}

	if (!Header.CheckMagic())
	{
		return FIoStatusBuilder(EIoErrorCode::CorruptToc) << TEXT("TOC header magic mismatch while reading '") << TocFilePath << TEXT("'");
	}

	if (Header.TocHeaderSize != sizeof(FIoStoreTocHeader))
	{
		return FIoStatusBuilder(EIoErrorCode::CorruptToc) << TEXT("TOC header size mismatch while reading '") << TocFilePath << TEXT("'");
	}

	if (Header.TocCompressedBlockEntrySize != sizeof(FIoStoreTocCompressedBlockEntry))
	{
		return FIoStatusBuilder(EIoErrorCode::CorruptToc) << TEXT("TOC compressed block entry size mismatch while reading '") << TocFilePath << TEXT("'");
	}

	if (Header.Version < static_cast<uint8>(EIoStoreTocVersion::DirectoryIndex))
	{
		return FIoStatusBuilder(EIoErrorCode::CorruptToc) << TEXT("Outdated TOC header version while reading '") << TocFilePath << TEXT("'");
	}

	if (Header.Version > static_cast<uint8>(EIoStoreTocVersion::Latest))
	{
		return FIoStatusBuilder(EIoErrorCode::CorruptToc) << TEXT("Too new TOC header version while reading '") << TocFilePath << TEXT("'");
	}

	const bool bHasOnDemandMetaData = Header.Version >= static_cast<uint8>(EIoStoreTocVersion::OnDemandMetaData) && EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::OnDemand);

	const uint64 TotalTocSize = TocFileHandle->Size() - sizeof(FIoStoreTocHeader);
	const uint64 TocMetaSize = Header.TocEntryCount * sizeof(FIoStoreTocEntryMeta);
	const uint64 TocOnDemandMetaSize = !bHasOnDemandMetaData ? 0 :	(Header.TocEntryCount * sizeof(FIoStoreTocOnDemandChunkMeta)) +
																	(Header.TocCompressedBlockEntryCount * sizeof(FIoStoreTocOnDemandCompressedBlockMeta));

	const uint64 DefaultTocSize = TotalTocSize - (Header.DirectoryIndexSize + TocMetaSize + TocOnDemandMetaSize);
	uint64 TocSize = DefaultTocSize;

	if (EnumHasAnyFlags(ReadOptions, EIoStoreTocReadOptions::ReadTocMeta))
	{
		TocSize = TotalTocSize; // Meta data is at the end of the TOC file
	}
	else if (EnumHasAnyFlags(ReadOptions, EIoStoreTocReadOptions::ReadDirectoryIndex))
	{
		TocSize = DefaultTocSize + Header.DirectoryIndexSize;
	}

	TUniquePtr<uint8[]> TocBuffer = MakeUnique<uint8[]>(TocSize);

	if (!TocFileHandle->Read(TocBuffer.Get(), TocSize))
	{
		return FIoStatusBuilder(EIoErrorCode::CorruptToc) << TEXT("Failed to read IoStore TOC file '") << TocFilePath << TEXT("'");
	}

	// Chunk IDs
	const uint8* DataPtr = TocBuffer.Get();
	const FIoChunkId* ChunkIds = reinterpret_cast<const FIoChunkId*>(DataPtr);
	OutTocResource.ChunkIds = MakeArrayView<FIoChunkId const>(ChunkIds, Header.TocEntryCount);
	DataPtr += Header.TocEntryCount * sizeof(FIoChunkId);

	// Chunk offsets
	const FIoOffsetAndLength* ChunkOffsetLengths = reinterpret_cast<const FIoOffsetAndLength*>(DataPtr);
	OutTocResource.ChunkOffsetLengths = MakeArrayView<FIoOffsetAndLength const>(ChunkOffsetLengths, Header.TocEntryCount);
	DataPtr += Header.TocEntryCount * sizeof(FIoOffsetAndLength);

	// Chunk perfect hash map
	uint32 PerfectHashSeedsCount = 0;
	uint32 ChunksWithoutPerfectHashCount = 0;
	if (Header.Version >= static_cast<uint8>(EIoStoreTocVersion::PerfectHashWithOverflow))
	{
		PerfectHashSeedsCount = Header.TocChunkPerfectHashSeedsCount;
		ChunksWithoutPerfectHashCount = Header.TocChunksWithoutPerfectHashCount;
	}
	else if (Header.Version >= static_cast<uint8>(EIoStoreTocVersion::PerfectHash))
	{
		PerfectHashSeedsCount = Header.TocChunkPerfectHashSeedsCount;
	}
	if (PerfectHashSeedsCount)
	{
		const int32* ChunkPerfectHashSeeds = reinterpret_cast<const int32*>(DataPtr);
		OutTocResource.ChunkPerfectHashSeeds = MakeArrayView<int32 const>(ChunkPerfectHashSeeds, PerfectHashSeedsCount);
		DataPtr += PerfectHashSeedsCount * sizeof(int32);
	}
	if (ChunksWithoutPerfectHashCount)
	{
		const int32* ChunkIndicesWithoutPerfectHash = reinterpret_cast<const int32*>(DataPtr);
		OutTocResource.ChunkIndicesWithoutPerfectHash = MakeArrayView<int32 const>(ChunkIndicesWithoutPerfectHash, ChunksWithoutPerfectHashCount);
		DataPtr += ChunksWithoutPerfectHashCount * sizeof(int32);
	}

	// Compression blocks
	const FIoStoreTocCompressedBlockEntry* CompressionBlocks = reinterpret_cast<const FIoStoreTocCompressedBlockEntry*>(DataPtr);
	OutTocResource.CompressionBlocks = MakeArrayView<FIoStoreTocCompressedBlockEntry const>(CompressionBlocks, Header.TocCompressedBlockEntryCount);
	DataPtr += Header.TocCompressedBlockEntryCount * sizeof(FIoStoreTocCompressedBlockEntry);

	// Compression methods
	OutTocResource.CompressionMethods.Reserve(Header.CompressionMethodNameCount + 1);
	OutTocResource.CompressionMethods.Add(NAME_None);

	const ANSICHAR* AnsiCompressionMethodNames = reinterpret_cast<const ANSICHAR*>(DataPtr);
	for (uint32 CompressonNameIndex = 0; CompressonNameIndex < Header.CompressionMethodNameCount; CompressonNameIndex++)
	{
		const ANSICHAR* AnsiCompressionMethodName = AnsiCompressionMethodNames + CompressonNameIndex * Header.CompressionMethodNameLength;
		OutTocResource.CompressionMethods.Add(FName(AnsiCompressionMethodName));
	}
	DataPtr += Header.CompressionMethodNameCount * Header.CompressionMethodNameLength;

	// Chunk block signatures
	const uint8* SignatureBuffer = reinterpret_cast<const uint8*>(DataPtr);
	const uint8* DirectoryIndexBuffer = SignatureBuffer;

	const bool bIsSigned = EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::Signed);
	if (IsSigningEnabled() || bIsSigned)
	{
		if (!bIsSigned)
		{
			return FIoStatus(EIoErrorCode::SignatureError, TEXT("Missing signature"));
		}

		const int32* HashSize = reinterpret_cast<const int32*>(SignatureBuffer);
		TArrayView<const uint8> TocSignature = MakeArrayView<const uint8>(reinterpret_cast<const uint8*>(HashSize + 1), *HashSize);
		TArrayView<const uint8> BlockSignature = MakeArrayView<const uint8>(TocSignature.GetData() + *HashSize, *HashSize);

		TArrayView<const uint8> BothSignatures = MakeArrayView<const uint8>(TocSignature.GetData(), *HashSize * 2);
		FSHA1::HashBuffer(BothSignatures.GetData(), BothSignatures.Num(), OutTocResource.SignatureHash.Hash);

		TArrayView<const FSHAHash> ChunkBlockSignatures = MakeArrayView<const FSHAHash>(reinterpret_cast<const FSHAHash*>(BlockSignature.GetData() + *HashSize), Header.TocCompressedBlockEntryCount);

		// Adjust address to meta data
		DirectoryIndexBuffer = reinterpret_cast<const uint8*>(ChunkBlockSignatures.GetData() + ChunkBlockSignatures.Num());

		OutTocResource.ChunkBlockSignatures = ChunkBlockSignatures;

		if (IsSigningEnabled())
		{
			FIoStatus SignatureStatus = ValidateContainerSignature(GetPublicSigningKey(), Header, OutTocResource.ChunkBlockSignatures, TocSignature, BlockSignature);
			if (!SignatureStatus.IsOk())
			{
				return SignatureStatus;
			}
		}
	}

	// Directory index
	if (EnumHasAnyFlags(ReadOptions, EIoStoreTocReadOptions::ReadDirectoryIndex) &&
		EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::Indexed) &&
		Header.DirectoryIndexSize > 0)
	{
		OutTocResource.DirectoryIndexBuffer = MakeArrayView<const uint8>(DirectoryIndexBuffer, Header.DirectoryIndexSize);
	}

	// Meta
	if (EnumHasAnyFlags(ReadOptions, EIoStoreTocReadOptions::ReadTocMeta))
	{
		const uint8* TocMeta = (uint8*)DirectoryIndexBuffer + Header.DirectoryIndexSize;

		const FIoStoreTocEntryMeta* ChunkMetas = reinterpret_cast<const FIoStoreTocEntryMeta*>(TocMeta);
		OutTocResource.ChunkMetas = MakeArrayView<FIoStoreTocEntryMeta const>(ChunkMetas, Header.TocEntryCount);

		// OnDemand 
		if (bHasOnDemandMetaData)
		{
			uint8 const* OnDemandPtr = TocMeta + TocMetaSize;

			const FIoStoreTocOnDemandChunkMeta* OnDemandChunkMeta = reinterpret_cast<const FIoStoreTocOnDemandChunkMeta*>(OnDemandPtr);
			OutTocResource.OnDemandChunkMeta = MakeArrayView<FIoStoreTocOnDemandChunkMeta const>(OnDemandChunkMeta, Header.TocEntryCount);

			OnDemandPtr += sizeof(FIoStoreTocOnDemandChunkMeta) * Header.TocEntryCount;

			const FIoStoreTocOnDemandCompressedBlockMeta* OnDemandCompressedBlockMeta = reinterpret_cast<const FIoStoreTocOnDemandCompressedBlockMeta*>(OnDemandPtr);
			OutTocResource.OnDemandCompressedBlockMeta = MakeArrayView<FIoStoreTocOnDemandCompressedBlockMeta const>(OnDemandCompressedBlockMeta, Header.TocCompressedBlockEntryCount);
		}
	}

	if (Header.Version < static_cast<uint8>(EIoStoreTocVersion::PartitionSize))
	{
		Header.PartitionCount = 1;
		Header.PartitionSize = MAX_uint64;
	}

	return FIoStatus::Ok;
}

TIoStatusOr<uint64> FIoStoreTocResource::Write(
	const TCHAR* TocFilePath,
	FIoStoreTocResource& TocResource,
	uint32 CompressionBlockSize,
	uint64 MaxPartitionSize,
	const FIoContainerSettings& ContainerSettings)
{
	check(TocFilePath != nullptr);

	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	TUniquePtr<IFileHandle> TocFileHandle(Ipf.OpenWrite(TocFilePath, /* append */ false, /* allowread */ true));

	if (!TocFileHandle)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Failed to open IoStore TOC file '") << TocFilePath << TEXT("'");
		return Status;
	}

	if (TocResource.ChunkIds.Num() != TocResource.ChunkOffsetLengths.Num())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Number of TOC chunk IDs doesn't match the number of offsets"));
	}

	if (TocResource.ChunkIds.Num() != TocResource.ChunkMetas.Num())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Number of TOC chunk IDs doesn't match the number of chunk meta data"));
	}

	bool bHasExplicitCompressionMethodNone = false;
	for (int32 CompressionMethodIndex = 0; CompressionMethodIndex < TocResource.CompressionMethods.Num(); ++CompressionMethodIndex)
	{
		if (TocResource.CompressionMethods[CompressionMethodIndex].IsNone())
		{
			if (CompressionMethodIndex != 0)
			{
				return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Compression method None must be the first compression method"));
			}
			bHasExplicitCompressionMethodNone = true;
		}
	}

	FMemory::Memzero(&TocResource.Header, sizeof(FIoStoreTocHeader));

	FIoStoreTocHeader& TocHeader = TocResource.Header;
	TocHeader.MakeMagic();
	TocHeader.Version = static_cast<uint8>(EIoStoreTocVersion::Latest);
	TocHeader.TocHeaderSize = sizeof(TocHeader);
	TocHeader.TocEntryCount = TocResource.ChunkIds.Num();
	TocHeader.TocChunkPerfectHashSeedsCount = TocResource.ChunkPerfectHashSeeds.Num();
	TocHeader.TocChunksWithoutPerfectHashCount = TocResource.ChunkIndicesWithoutPerfectHash.Num();
	TocHeader.TocCompressedBlockEntryCount = TocResource.CompressionBlocks.Num();
	TocHeader.TocCompressedBlockEntrySize = sizeof(FIoStoreTocCompressedBlockEntry);
	TocHeader.CompressionBlockSize = CompressionBlockSize;
	TocHeader.CompressionMethodNameCount = TocResource.CompressionMethods.Num() - (bHasExplicitCompressionMethodNone ? 1 : 0);
	TocHeader.CompressionMethodNameLength = FIoStoreTocResource::CompressionMethodNameLen;
	TocHeader.DirectoryIndexSize = TocResource.DirectoryIndexBuffer.Num();
	TocHeader.ContainerId = ContainerSettings.ContainerId;
	TocHeader.EncryptionKeyGuid = ContainerSettings.EncryptionKeyGuid;
	TocHeader.ContainerFlags = ContainerSettings.ContainerFlags;
	if (TocHeader.TocEntryCount == 0)
	{
		TocHeader.PartitionCount = 0;
		TocHeader.PartitionSize = MAX_uint64;
	}
	else if (MaxPartitionSize)
	{
		const FIoStoreTocCompressedBlockEntry& LastBlock = TocResource.CompressionBlocks.Last();
		uint64 LastBlockEnd = LastBlock.GetOffset() + LastBlock.GetCompressedSize() - 1;
		TocHeader.PartitionCount = IntCastChecked<uint32>(LastBlockEnd / MaxPartitionSize + 1);
		check(TocHeader.PartitionCount > 0);
		TocHeader.PartitionSize = MaxPartitionSize;
	}
	else
	{
		TocHeader.PartitionCount = 1;
		TocHeader.PartitionSize = MAX_uint64;
	}

	TocFileHandle->Seek(0);

	// Header
	if (!TocFileHandle->Write(reinterpret_cast<const uint8*>(&TocResource.Header), sizeof(FIoStoreTocHeader)))
	{
		return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write TOC header"));
	}

	// Chunk IDs
	if (!WriteArray(TocFileHandle.Get(), TocResource.ChunkIds))
	{
		return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write chunk ids"));
	}

	// Chunk offsets
	if (!WriteArray(TocFileHandle.Get(), TocResource.ChunkOffsetLengths))
	{
		return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write chunk offsets"));
	}

	// Chunk perfect hash map
	if (!WriteArray(TocFileHandle.Get(), TocResource.ChunkPerfectHashSeeds))
	{
		return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write chunk hash seeds"));
	}
	if (!WriteArray(TocFileHandle.Get(), TocResource.ChunkIndicesWithoutPerfectHash))
	{
		return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write chunk indices without perfect hash"));
	}

	// Compression blocks
	if (!WriteArray(TocFileHandle.Get(), TocResource.CompressionBlocks))
	{
		return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write chunk block entries"));
	}

	// Compression methods
	ANSICHAR AnsiMethodName[FIoStoreTocResource::CompressionMethodNameLen];

	for (FName MethodName : TocResource.CompressionMethods)
	{
		if (MethodName.IsNone())
		{
			continue;
		}
		FMemory::Memzero(AnsiMethodName, FIoStoreTocResource::CompressionMethodNameLen);
		FCStringAnsi::Strcpy(AnsiMethodName, FIoStoreTocResource::CompressionMethodNameLen, TCHAR_TO_ANSI(*MethodName.ToString()));

		if (!TocFileHandle->Write(reinterpret_cast<const uint8*>(AnsiMethodName), FIoStoreTocResource::CompressionMethodNameLen))
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write compression method TOC entry"));
		}
	}

	// Chunk block signatures
	if (EnumHasAnyFlags(TocHeader.ContainerFlags, EIoContainerFlags::Signed))
	{
		TArray<uint8> TocSignature, BlockSignature;
		check(TocResource.ChunkBlockSignatures.Num() == TocResource.CompressionBlocks.Num());

		FIoStatus SignatureStatus = CreateContainerSignature(
			ContainerSettings.SigningKey,
			TocHeader,
			TocResource.ChunkBlockSignatures,
			TocSignature,
			BlockSignature);

		if (!SignatureStatus .IsOk())
		{
			return SignatureStatus;
		}

		check(TocSignature.Num() == BlockSignature.Num());

		const int32 HashSize = TocSignature.Num();
		TocFileHandle->Write(reinterpret_cast<const uint8*>(&HashSize), sizeof(int32));
		TocFileHandle->Write(TocSignature.GetData(), HashSize);
		TocFileHandle->Write(BlockSignature.GetData(), HashSize);

		if (!WriteArray(TocFileHandle.Get(), TocResource.ChunkBlockSignatures))
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write chunk block signatures"));
		}
	}

	// Directory index (EIoStoreTocReadOptions::ReadDirectoryIndex)
	if (EnumHasAnyFlags(TocHeader.ContainerFlags, EIoContainerFlags::Indexed))
	{
		if (!TocFileHandle->Write(TocResource.DirectoryIndexBuffer.GetData(), TocResource.DirectoryIndexBuffer.Num()))
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write directory index buffer"));
		}
	}

	// Meta data (EIoStoreTocReadOptions::ReadTocMeta)
	{
		// First write out the standard meta info
		if (!WriteArray(TocFileHandle.Get(), TocResource.ChunkMetas))
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write chunk meta data"));
		}

		// Write out the metadata for the 'OnDemand' feature if the container supports this.
		if (EnumHasAnyFlags(TocHeader.ContainerFlags, EIoContainerFlags::OnDemand))
		{
			if (!WriteArray(TocFileHandle.Get(), TocResource.OnDemandChunkMeta))
			{
				return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write on demand chunk meta data"));
			}

			if (!WriteArray(TocFileHandle.Get(), TocResource.OnDemandCompressedBlockMeta))
			{
				return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write on demand compressed block meta data"));
			}
		}
	}

	TocFileHandle->Flush(true);

	return TocFileHandle->Tell();
}

uint64 FIoStoreTocResource::HashChunkIdWithSeed(int32 Seed, const FIoChunkId& ChunkId)
{
	const uint8* Data = ChunkId.GetData();
	const uint32 DataSize = ChunkId.GetSize();
	uint64 Hash = Seed ? static_cast<uint64>(Seed) : 0xcbf29ce484222325;
	for (uint32 Index = 0; Index < DataSize; ++Index)
	{
		Hash = (Hash * 0x00000100000001B3) ^ Data[Index];
	}
	return Hash;
}

void FIoStoreReader::GetFilenames(TArray<FString>& OutFileList) const
{
	const FIoDirectoryIndexReader& DirectoryIndex = GetDirectoryIndexReader();

	DirectoryIndex.IterateDirectoryIndex(
		FIoDirectoryIndexHandle::RootDirectory(),
		TEXT(""),
		[&OutFileList](FStringView Filename, uint32 TocEntryIndex) -> bool
		{
			OutFileList.AddUnique(FString(Filename));
			return true;
		});
}

void FIoStoreReader::GetFilenamesByBlockIndex(const TArray<int32>& InBlockIndexList, TArray<FString>& OutFileList) const
{
	const FIoDirectoryIndexReader& DirectoryIndex = GetDirectoryIndexReader();

	DirectoryIndex.IterateDirectoryIndex(FIoDirectoryIndexHandle::RootDirectory(), TEXT(""),
		[this, &InBlockIndexList, &OutFileList](FStringView Filename, uint32 TocEntryIndex) -> bool
		{
			for (int32 BlockIndex : InBlockIndexList)
			{
				if (Impl->TocChunkContainsBlockIndex(TocEntryIndex, BlockIndex))
				{
					OutFileList.AddUnique(FString(Filename));
					break;
				}
			}

			return true;
		});
}
