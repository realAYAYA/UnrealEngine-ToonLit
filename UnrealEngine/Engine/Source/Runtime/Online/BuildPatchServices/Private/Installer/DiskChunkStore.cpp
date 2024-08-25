// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/DiskChunkStore.h"
#include "Installer/InstallerSharedContext.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Union.h"
#include "Containers/Queue.h"
#include "Misc/ScopeLock.h"
#include "Async/Async.h"
#include "Math/UnitConversion.h"
#include "Core/AsyncHelpers.h"
#include "Data/ChunkData.h"
#include "Common/FileSystem.h"

namespace BuildPatchServices
{
	typedef TTuple<EChunkLoadResult, IChunkDataAccess*> FLoadResult;
	typedef TFuture<FLoadResult> FLoadFuture;
	typedef TPromise<FLoadResult> FLoadPromise;
	typedef TTuple<FGuid, FLoadPromise*> FQueuedChunkLoad;
	typedef TTuple<FGuid, IChunkDataAccess*> FQueuedChunkSave;
	typedef TUnion<FQueuedChunkLoad, FQueuedChunkSave> FQueuedChunk;
	typedef TQueue<FQueuedChunk, EQueueMode::Mpsc> FChunkQueue;

	class FDiskChunkStore
		: public IDiskChunkStore
	{
	public:
		FDiskChunkStore(IFileSystem* FileSystem, IChunkDataSerialization* InSerializer, IDiskChunkStoreStat* InDiskChunkStoreStat, FDiskChunkStoreConfig InConfiguration);
		~FDiskChunkStore();

		// IChunkStore interface begin.
		virtual void Put(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData) override;
		virtual IChunkDataAccess* Get(const FGuid& DataId) override;
		virtual TUniquePtr<IChunkDataAccess> Remove(const FGuid& DataId) override;
		virtual int32 GetSize() const override;
		virtual void SetLostChunkCallback(TFunction<void(const FGuid&)> Callback) override;
		// IChunkStore interface end.

	private:
		void IoThread();
		FLoadFuture QueueLoadRequest(const FGuid& DataId);
		void QueueSaveRequest(const FGuid& DataId, TUniquePtr<IChunkDataAccess>&& ChunkData);
		void ExecLostChunkCallback(const FGuid& LostChunk);
		bool IsExpectedChunk(IChunkDataAccess* ChunkDataAccess, const FGuid& ChunkId);

	private:
		IFileSystem* const FileSystem = nullptr;
		IChunkDataSerialization* const Serializer = nullptr;
		IDiskChunkStoreStat* const DiskChunkStoreStat = nullptr;
		const FDiskChunkStoreConfig Configuration;
		FCriticalSection LostChunkCallbackCs;
		TFunction<void(const FGuid&)> LostChunkCallback;
		FCriticalSection ThreadLockCs;
		FGuid LastGetId;
		TUniquePtr<IChunkDataAccess> LastGetData;
		TSet<FGuid> PlacedInStore;
		std::atomic<bool> bShouldRun = true;
		FChunkQueue Queue;
		std::atomic<int32> QueueNum = 0;
		FEvent* QueueTrigger = nullptr;
		TPromise<void> IoThreadPromise;
		TFuture<void> IoThreadFuture;
		IBuildInstallerThread* Thread = nullptr;
		FEvent* IoThreadTrigger = nullptr;
	};

	FDiskChunkStore::FDiskChunkStore(IFileSystem* InFileSystem, IChunkDataSerialization* InSerializer, IDiskChunkStoreStat* InDiskChunkStoreStat, FDiskChunkStoreConfig InConfiguration)
		: FileSystem(InFileSystem)
		, Serializer(InSerializer)
		, DiskChunkStoreStat(InDiskChunkStoreStat)
		, Configuration(MoveTemp(InConfiguration))
		, QueueTrigger(FPlatformProcess::GetSynchEventFromPool(true))
		, IoThreadTrigger(FPlatformProcess::GetSynchEventFromPool(true))
	{
		IoThreadFuture = IoThreadPromise.GetFuture();
		Thread = Configuration.SharedContext->CreateThread();
		Thread->RunTask([this]() { IoThread(); });
	}

	FDiskChunkStore::~FDiskChunkStore()
	{
		// Signal running loops to exit.
		bShouldRun = false;
		QueueTrigger->Trigger();
		IoThreadTrigger->Trigger();
		IoThreadFuture.Wait();

		// Return events.
		FPlatformProcess::ReturnSynchEventToPool(IoThreadTrigger);
		FPlatformProcess::ReturnSynchEventToPool(QueueTrigger);

		// Return thread.
		Configuration.SharedContext->ReleaseThread(Thread);
		Thread = nullptr;

		// Clean up allocations left in the queue.
		FQueuedChunk QueuedChunk;
		while (Queue.Dequeue(QueuedChunk))
		{
			if (QueuedChunk.HasSubtype<FQueuedChunkLoad>())
			{
				TUniquePtr<FLoadPromise> LoadPromisePtr(QueuedChunk.GetSubtype<FQueuedChunkLoad>().Get<1>());
				LoadPromisePtr->SetValue(FLoadResult(EChunkLoadResult::Aborted, nullptr));
			}
			else if (QueuedChunk.HasSubtype<FQueuedChunkSave>())
			{
				TUniquePtr<IChunkDataAccess>(QueuedChunk.GetSubtype<FQueuedChunkSave>().Get<1>());
			}
		}
	}

	void FDiskChunkStore::Put(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData)
	{
		// Thread lock to protect access to PlacedInStore.
		FScopeLock ThreadLock(&ThreadLockCs);
		QueueSaveRequest(DataId, MoveTemp(ChunkData));
		PlacedInStore.Add(DataId);
		DiskChunkStoreStat->OnCacheUseUpdated(PlacedInStore.Num());
	}

	IChunkDataAccess* FDiskChunkStore::Get(const FGuid& DataId)
	{
		// Thread lock to protect access to PlacedInStore, LastGetId, and LastGetData.
		FScopeLock ThreadLock(&ThreadLockCs);
		// Load different chunk if necessary.
		IChunkDataAccess* ChunkData = nullptr;
		if (LastGetId != DataId)
		{
			if (PlacedInStore.Contains(DataId))
			{
				FLoadFuture ChunkLoadFuture = QueueLoadRequest(DataId);
				FLoadResult LoadResult = ChunkLoadFuture.Get();
				if (LoadResult.Get<0>() == EChunkLoadResult::Success)
				{
					LastGetId = DataId;
					LastGetData.Reset(LoadResult.Get<1>());
					ChunkData = LastGetData.Get();
				}
				else
				{
					PlacedInStore.Remove(DataId);
					DiskChunkStoreStat->OnCacheUseUpdated(PlacedInStore.Num());
				}
			}
		}
		else
		{
			ChunkData = LastGetData.Get();
		}
		return ChunkData;
	}

	TUniquePtr<IChunkDataAccess> FDiskChunkStore::Remove(const FGuid& DataId)
	{
		// Thread lock to protect access to LastGetId and LastGetData.
		FScopeLock ThreadLock(&ThreadLockCs);
		TUniquePtr<IChunkDataAccess> ReturnValue;
		if (LastGetId != DataId)
		{
			if (PlacedInStore.Contains(DataId))
			{
				FLoadFuture ChunkLoadFuture = QueueLoadRequest(DataId);
				FLoadResult LoadResult = ChunkLoadFuture.Get();
				ReturnValue.Reset(LoadResult.Get<1>());
				if (LoadResult.Get<0>() != EChunkLoadResult::Success)
				{
					PlacedInStore.Remove(DataId);
					DiskChunkStoreStat->OnCacheUseUpdated(PlacedInStore.Num());
				}
			}
		}
		else
		{
			LastGetId.Invalidate();
			ReturnValue = MoveTemp(LastGetData);
		}
		return ReturnValue;
	}

	int32 FDiskChunkStore::GetSize() const
	{
		// We are not configured with a max, so as per API spec, return max int32
		return MAX_int32;
	}

	void FDiskChunkStore::SetLostChunkCallback(TFunction<void(const FGuid&)> Callback)
	{
		// Thread lock to protect access to LostChunkCallback.
		FScopeLock ThreadLock(&LostChunkCallbackCs);
		LostChunkCallback = MoveTemp(Callback);
	}

	void FDiskChunkStore::IoThread()
	{
		FString DumpFilename = Configuration.StoreRootPath / TEXT("chunkdump");
		TMap<FGuid, int64> ChunkIdToDumpLocation;

		TUniquePtr<FArchive> DumpWriter = FileSystem->CreateFileWriter(*DumpFilename, EWriteFlags::AllowRead);
		TUniquePtr<FArchive> DumpReader = FileSystem->CreateFileReader(*DumpFilename, EReadFlags::AllowWrite);
		bool bValidFileHandle = DumpReader.IsValid() && DumpWriter.IsValid();
		if (bValidFileHandle)
		{
			DumpWriter->Seek(0);
		}

		FQueuedChunk QueuedChunk;
		while (bShouldRun)
		{
			if (Queue.Dequeue(QueuedChunk))
			{
				QueueNum--;
				QueueTrigger->Trigger();
				if (QueuedChunk.HasSubtype<FQueuedChunkLoad>())
				{
					FQueuedChunkLoad& QueuedChunkLoad = QueuedChunk.GetSubtype<FQueuedChunkLoad>();
					FGuid& DataId = QueuedChunkLoad.Get<0>();
					TUniquePtr<FLoadPromise> LoadPromisePtr(QueuedChunkLoad.Get<1>());
					DiskChunkStoreStat->OnBeforeChunkLoad(DataId);
					FLoadResult LoadResult(EChunkLoadResult::BadArchive, nullptr);
					EChunkLoadResult& ChunkLoadResult = LoadResult.Get<0>();
					IChunkDataAccess*& ChunkDataAccess = LoadResult.Get<1>();
					if (ChunkIdToDumpLocation.Contains(DataId))
					{
						const int64& ChunkStartPos = ChunkIdToDumpLocation[DataId];
						if (bValidFileHandle && DumpReader->TotalSize() != DumpWriter->TotalSize())
						{
							// We have to re-open the reader when the size has not been refreshed.
							DumpReader = FileSystem->CreateFileReader(*DumpFilename, EReadFlags::AllowWrite);
							bValidFileHandle = DumpReader.IsValid();
						}
						if (bValidFileHandle && ChunkStartPos < DumpReader->TotalSize())
						{
							DumpWriter->Flush();
							DumpReader->Seek(ChunkStartPos);
							ChunkDataAccess = Serializer->LoadFromArchive(*DumpReader, ChunkLoadResult);
							bValidFileHandle = !DumpReader->IsError() && !DumpWriter->IsError();
							if (!bValidFileHandle || !IsExpectedChunk(ChunkDataAccess, DataId))
							{
								ChunkLoadResult = EChunkLoadResult::SerializationError;
							}
						}
						if (ChunkLoadResult != EChunkLoadResult::Success)
						{
							ChunkIdToDumpLocation.Remove(DataId);
							ExecLostChunkCallback(DataId);
							int64 ChunkDumpFileSize = 0;
							FileSystem->GetFileSize(*DumpFilename, ChunkDumpFileSize);
							for (auto ChunkIdToDumpLocationIt = ChunkIdToDumpLocation.CreateIterator(); ChunkIdToDumpLocationIt; ++ChunkIdToDumpLocationIt)
							{
								const TPair<FGuid, int64>& LocationPair = *ChunkIdToDumpLocationIt;
								if (LocationPair.Value >= ChunkDumpFileSize)
								{
									ExecLostChunkCallback(LocationPair.Key);
									ChunkIdToDumpLocationIt.RemoveCurrent();
								}
							}
						}
					}
					LoadPromisePtr->SetValue(LoadResult);
					DiskChunkStoreStat->OnChunkLoaded(DataId, DumpFilename, ChunkLoadResult);
				}
				else if (QueuedChunk.HasSubtype<FQueuedChunkSave>())
				{
					FQueuedChunkSave& QueuedChunkSave = QueuedChunk.GetSubtype<FQueuedChunkSave>();
					FGuid& DataId = QueuedChunkSave.Get<0>();
					TUniquePtr<IChunkDataAccess> ChunkDataPtr(QueuedChunkSave.Get<1>());
					if (!ChunkIdToDumpLocation.Contains(DataId))
					{
						bool bSuccess = false;
						EChunkSaveResult SaveResult = EChunkSaveResult::FileCreateFail;
						if (bValidFileHandle)
						{
							DumpWriter->Seek(DumpWriter->TotalSize());
							const int64 ChunkStartPos = DumpWriter->Tell();
							SaveResult = Serializer->SaveToArchive(*DumpWriter, ChunkDataPtr.Get());
							const int64 ChunkEndPos = DumpWriter->Tell();
							bValidFileHandle = !DumpWriter->IsError();
							if (SaveResult == EChunkSaveResult::Success && bValidFileHandle)
							{
								ChunkIdToDumpLocation.Add(DataId, ChunkStartPos);
								bSuccess = true;
							}
						}
						if (bSuccess == false)
						{
							ExecLostChunkCallback(DataId);
						}
						DiskChunkStoreStat->OnChunkStored(DataId, DumpFilename, SaveResult);
					}
				}
				else
				{
					checkf(0, TEXT("Union has no recognized type"));
				}
			}
			else if (bValidFileHandle && DumpReader->TotalSize() != DumpWriter->TotalSize())
			{
				// We have to re-open the reader when the size has not been refreshed.
				DumpReader = FileSystem->CreateFileReader(*DumpFilename, EReadFlags::AllowWrite);
				bValidFileHandle = DumpReader.IsValid();
			}
			else
			{
				// Flush when waiting for jobs.
				if (bValidFileHandle)
				{
					DumpWriter->Flush();
					bValidFileHandle = !DumpWriter->IsError();
				}
				// Or try to fix our file access issue if we have one.
				else
				{
					DumpWriter = FileSystem->CreateFileWriter(*DumpFilename, EWriteFlags::AllowRead);
					DumpReader = FileSystem->CreateFileReader(*DumpFilename, EReadFlags::AllowWrite);
					bValidFileHandle = DumpReader.IsValid() && DumpWriter.IsValid();
				}
				// Max wait time allows us to periodically retry inaccessible file.
				static const uint32 WaitTime = Configuration.MaxRetryTime * 1000.0;
				IoThreadTrigger->Wait(WaitTime);
				IoThreadTrigger->Reset();
			}
		}

		DumpWriter.Reset();
		DumpReader.Reset();
		FileSystem->DeleteFile(*DumpFilename);

		IoThreadPromise.SetValue();
	}

	BuildPatchServices::FLoadFuture FDiskChunkStore::QueueLoadRequest(const FGuid& DataId)
	{
		while (QueueNum > Configuration.QueueSize && bShouldRun)
		{
			// Wait 1 second max in case of abort.
			static const uint32 WaitTime = 1000;
			QueueTrigger->Wait(WaitTime);
			QueueTrigger->Reset();
		}
		QueueNum++;
		TUniquePtr<FLoadPromise> PromisePtr(new FLoadPromise());
		FLoadFuture Future = PromisePtr->GetFuture();
		if (bShouldRun)
		{
			Queue.Enqueue(FQueuedChunk(FQueuedChunkLoad(DataId, PromisePtr.Release())));
			IoThreadTrigger->Trigger();
		}
		else
		{
			PromisePtr->SetValue(FLoadResult(EChunkLoadResult::Aborted, nullptr));
		}
		return Future;
	}

	void FDiskChunkStore::QueueSaveRequest(const FGuid& DataId, TUniquePtr<IChunkDataAccess>&& ChunkData)
	{
		while (QueueNum > Configuration.QueueSize && bShouldRun)
		{
			// Wait 1 second max in case of abort.
			static const uint32 WaitTime = 1000;
			QueueTrigger->Wait(WaitTime);
			QueueTrigger->Reset();
		}
		QueueNum++;
		Queue.Enqueue(FQueuedChunk(FQueuedChunkSave(DataId, ChunkData.Release())));
		IoThreadTrigger->Trigger();
	}

	void FDiskChunkStore::ExecLostChunkCallback(const FGuid& LostChunk)
	{
		// Thread lock to protect access to LostChunkCallback.
		FScopeLock ThreadLock(&LostChunkCallbackCs);
		if (LostChunkCallback)
		{
			LostChunkCallback(LostChunk);
		}
	}

	bool FDiskChunkStore::IsExpectedChunk(IChunkDataAccess* ChunkDataAccess, const FGuid& ChunkId)
	{
		if (ChunkDataAccess != nullptr)
		{
			FScopeLockedChunkData LockedChunkData(ChunkDataAccess);
			return LockedChunkData.GetHeader()->Guid == ChunkId;
		}
		return false;
	}

	IDiskChunkStore* FDiskChunkStoreFactory::Create(IFileSystem* FileSystem, IChunkDataSerialization* Serializer, IDiskChunkStoreStat* DiskChunkStoreStat, FDiskChunkStoreConfig Configuration)
	{
		check(FileSystem != nullptr);
		check(Serializer != nullptr);
		check(DiskChunkStoreStat != nullptr);
		check(Configuration.StoreRootPath.IsEmpty() == false);
		return new FDiskChunkStore(FileSystem, Serializer, DiskChunkStoreStat, MoveTemp(Configuration));
	}
}