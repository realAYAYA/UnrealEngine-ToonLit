// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generation/ChunkWriter.h"

#include "Logging/LogMacros.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Async/Async.h"

#include "Common/FileSystem.h"
#include "Common/StatsCollector.h"
#include "Core/AsyncHelpers.h"
#include "Data/ChunkData.h"
#include "BuildPatchUtil.h"

DECLARE_LOG_CATEGORY_CLASS(LogChunkWriter, Log, All);

namespace BuildPatchServices
{
	typedef TTuple<TArray<uint8>, FGuid, uint64, FSHAHash> FChunkDataJob;
	typedef TTuple <FGuid, int64> FChunkOutputSize;
	typedef TTuple <FGuid, uint64> FChunkOutputHash;
	typedef TTuple <FGuid, FSHAHash> FChunkOutputSha;

	class FWriterChunkDataAccess
		: public IChunkDataAccess
	{
	public:
		FWriterChunkDataAccess(TArray<uint8>& InDataRef, FChunkHeader& InHeaderRef)
			: DataRef(InDataRef)
			, HeaderRef(InHeaderRef)
		{
		}
		// IChunkDataAccess interface begin.
		virtual void GetDataLock(const uint8** OutChunkData, const FChunkHeader** OutChunkHeader) const override
		{
			(*OutChunkData) = DataRef.GetData();
			(*OutChunkHeader) = &HeaderRef;
		}
		virtual void GetDataLock(uint8** OutChunkData, FChunkHeader** OutChunkHeader) override
		{
			(*OutChunkData) = DataRef.GetData();
			(*OutChunkHeader) = &HeaderRef;
		}
		virtual void ReleaseDataLock() const
		{
		}
		// IChunkDataAccess interface end.

	private:
		TArray<uint8>& DataRef;
		FChunkHeader& HeaderRef;
	};

	class FParallelChunkWriter
		: public IParallelChunkWriter
	{
	public:
		FParallelChunkWriter(FParallelChunkWriterConfig InConfig, IFileSystem* InFileSystem, IChunkDataSerialization* InChunkDataSerialization, FStatsCollector* InStatsCollector)
			: Config(MoveTemp(InConfig))
			, FileSystem(InFileSystem)
			, ChunkDataSerialization(InChunkDataSerialization)
			, StatsCollector(InStatsCollector)
			, bMoreDataIsExpected(true)
			, bShouldAbort(false)
		{
			StatUncompressesData = 0;
			StatSerlialiseTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Serialize Time"), EStatFormat::Timer);
			StatChunksSaved = StatsCollector->CreateStat(TEXT("Chunk Writer: Num Saved"), EStatFormat::Value);
			StatDataWritten = StatsCollector->CreateStat(TEXT("Chunk Writer: Data Size Written"), EStatFormat::DataSize);
			StatCompressionRatio = StatsCollector->CreateStat(TEXT("Chunk Writer: Compression Ratio"), EStatFormat::Percentage);
			StatDataWriteSpeed = StatsCollector->CreateStat(TEXT("Chunk Writer: Data Write Speed"), EStatFormat::DataSpeed);
			FileSystem->MakeDirectory(*Config.ChunkDirectory);
			if (!FileSystem->DirectoryExists(*Config.ChunkDirectory))
			{
				UE_LOG(LogChunkWriter, Fatal, TEXT("Could not create cloud directory (%s)."), *Config.ChunkDirectory);
			}
			for (int32 ThreadIdx = 0; ThreadIdx < Config.NumberOfThreads; ++ThreadIdx)
			{
				WriterThreads.Add(Async(EAsyncExecution::Thread, [this]()
				{
					WriterThread();
				}));
			}
		}

		~FParallelChunkWriter()
		{
			bShouldAbort = true;
			for (const TFuture<void>& Thread : WriterThreads)
			{
				Thread.Wait();
			}
			WriterThreads.Empty();
		}

		// IParallelChunkWriter interface begin.
		virtual void AddChunkData(TArray<uint8> ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash, const FSHAHash& ChunkSha) override
		{
			DebugCheckSingleProducer();
			// Check for violations of feature level.
			check(Config.FeatureLevel >= EFeatureLevel::VariableSizeChunksWithoutWindowSizeChunkInfo || ChunkData.Num() == LegacyFixedChunkWindow);
			// Wait for queue space.
			while (ChunkDataJobQueueCount.GetValue() >= Config.MaxQueueSize)
			{
				FPlatformProcess::Sleep(0);
			}
			ChunkDataJobQueueCount.Increment();
			ChunkDataJobQueue.Enqueue(FChunkDataJob(MoveTemp(ChunkData), ChunkGuid, ChunkHash, ChunkSha));
		}
		virtual FParallelChunkWriterSummaries OnProcessComplete() override
		{
			bMoreDataIsExpected = false;
			for (const TFuture<void>& Thread : WriterThreads)
			{
				Thread.Wait();
			}
			WriterThreads.Empty();
			ParallelChunkWriterSummaries.FeatureLevel = Config.FeatureLevel;
			FChunkOutputSize ChunkOutputSize;
			while (ChunkOutputSizeQueue.Dequeue(ChunkOutputSize))
			{
				ParallelChunkWriterSummaries.ChunkOutputSizes.Add(ChunkOutputSize.Get<0>(), ChunkOutputSize.Get<1>());
			}
			FChunkOutputHash ChunkOutputHash;
			while (ChunkOutputHashQueue.Dequeue(ChunkOutputHash))
			{
				ParallelChunkWriterSummaries.ChunkOutputHashes.Add(ChunkOutputHash.Get<0>(), ChunkOutputHash.Get<1>());
			}
			FChunkOutputSha ChunkOutputSha;
			while (ChunkOutputShaQueue.Dequeue(ChunkOutputSha))
			{
				ParallelChunkWriterSummaries.ChunkOutputShas.Add(ChunkOutputSha.Get<0>(), ChunkOutputSha.Get<1>());
			}
			return ParallelChunkWriterSummaries;
		}
		// IParallelChunkWriter interface end.

	private:
		void WriterThread()
		{
			uint64 ChunkWriteTime;
			bool bReceivedJob;
			FChunkDataJob ChunkDataJob;
			while (!bShouldAbort && (bMoreDataIsExpected || ChunkDataJobQueueCount.GetValue() > 0))
			{
				ChunkDataJobQueueConsumerCS.Lock();
				bReceivedJob = ChunkDataJobQueue.Dequeue(ChunkDataJob);
				ChunkDataJobQueueConsumerCS.Unlock();
				if (bReceivedJob)
				{
					ChunkDataJobQueueCount.Decrement();
					TArray<uint8>& ChunkData = ChunkDataJob.Get<0>();
					FGuid& ChunkGuid = ChunkDataJob.Get<1>();
					uint64& ChunkHash = ChunkDataJob.Get<2>();
					FSHAHash& ChunkSha = ChunkDataJob.Get<3>();
					FChunkHeader ChunkHeader;
					ChunkHeader.Guid = ChunkGuid;
					ChunkHeader.DataSizeCompressed = ChunkData.Num();
					ChunkHeader.DataSizeUncompressed = ChunkData.Num();
					ChunkHeader.StoredAs = EChunkStorageFlags::None;
					ChunkHeader.HashType = EChunkHashFlags::RollingPoly64 | EChunkHashFlags::Sha1;
					ChunkHeader.RollingHash = ChunkHash;
					ChunkHeader.SHAHash = ChunkSha;
					TUniquePtr<FWriterChunkDataAccess> ChunkDataAccess(new FWriterChunkDataAccess(ChunkData, ChunkHeader));
					const FString NewChunkFilename = Config.ChunkDirectory / FBuildPatchUtils::GetChunkNewFilename(Config.FeatureLevel, ChunkGuid, ChunkHash);
					int64 ExistingChunkFilesize = INDEX_NONE;
					const bool bFileExists = FileSystem->GetFileSize(*NewChunkFilename, ExistingChunkFilesize);
					if (!bFileExists)
					{
						bool bSaveSuccess = false;
						int32 RetryCount = Config.SaveRetryCount;
						while (!bShouldAbort && !bSaveSuccess && RetryCount-- >= 0)
						{
							FStatsCollector::AccumulateTimeBegin(ChunkWriteTime);
							FileSystem->MakeDirectory(*FPaths::GetPath(NewChunkFilename));
							TUniquePtr<FArchive> ChunkFileOut = FileSystem->CreateFileWriter(*NewChunkFilename);
							if (!ChunkFileOut.IsValid())
							{
								UE_LOG(LogChunkWriter, Log, TEXT("Could not create chunk (%s)."), *NewChunkFilename);
								FPlatformProcess::Sleep(Config.SaveRetryTime);
								continue;
							}
							EChunkSaveResult ChunkSaveResult = ChunkDataSerialization->SaveToArchive(*ChunkFileOut.Get(), ChunkDataAccess.Get());
							FStatsCollector::AccumulateTimeEnd(StatSerlialiseTime, ChunkWriteTime);
							const int64 ChunkFileSize = ChunkFileOut->TotalSize();
							const bool bArchiveSuccess = ChunkFileOut->Close();
							const bool bNewFileExists = FileSystem->FileExists(*NewChunkFilename);
							if (!bArchiveSuccess || !bNewFileExists || ChunkSaveResult != EChunkSaveResult::Success)
							{
								UE_LOG(LogChunkWriter, Log, TEXT("Could not save chunk [%d %d %s] (%s)."), bArchiveSuccess, bNewFileExists, ToString(ChunkSaveResult), *NewChunkFilename);
								FPlatformProcess::Sleep(Config.SaveRetryTime);
								continue;
							}
							bSaveSuccess = true;
							ChunkOutputSizeQueue.Enqueue(FChunkOutputSize(ChunkGuid, ChunkFileSize));
							ChunkOutputHashQueue.Enqueue(FChunkOutputHash(ChunkGuid, ChunkHash));
							ChunkOutputShaQueue.Enqueue(FChunkOutputSha(ChunkGuid, ChunkSha));
							FStatsCollector::Accumulate(StatChunksSaved, 1);
							FStatsCollector::Accumulate(StatDataWritten, ChunkFileSize);
							FStatsCollector::Accumulate(&StatUncompressesData, ChunkHeader.HeaderSize + ChunkHeader.DataSizeUncompressed);
							const double DataWrittenD = *StatDataWritten;
							const double UncompressesDataD = StatUncompressesData;
							if (UncompressesDataD > 0)
							{
								FStatsCollector::SetAsPercentage(StatCompressionRatio, DataWrittenD / UncompressesDataD);
							}
							const double SerlialiseTime = FStatsCollector::CyclesToSeconds(*StatSerlialiseTime);
							if (SerlialiseTime > 0)
							{
								FStatsCollector::Set(StatDataWriteSpeed, *StatDataWritten / SerlialiseTime);
							}
						}
						if (!bSaveSuccess)
						{
							UE_LOG(LogChunkWriter, Fatal, TEXT("Chunk save failure (%s)."), *NewChunkFilename);
						}
					}
					else
					{
						ChunkOutputSizeQueue.Enqueue(FChunkOutputSize(ChunkGuid, ExistingChunkFilesize));
						ChunkOutputHashQueue.Enqueue(FChunkOutputHash(ChunkGuid, ChunkHash));
						ChunkOutputShaQueue.Enqueue(FChunkOutputSha(ChunkGuid, ChunkSha));
					}
				}
				else
				{
					FPlatformProcess::Sleep(1.0/10.0);
				}
			}
		}

		void DebugCheckSingleProducer()
		{
#if !UE_BUILD_SHIPPING
			const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
			if (ProducerThreadId.IsSet())
			{
				check(CurrentThreadId == ProducerThreadId.GetValue());
			}
			else
			{
				ProducerThreadId = CurrentThreadId;
			}
#endif
		}

	private:
		const FParallelChunkWriterConfig Config;
		IFileSystem* const FileSystem;
		IChunkDataSerialization* const ChunkDataSerialization;
		FStatsCollector* const StatsCollector;

		volatile int64 StatUncompressesData;
		volatile int64* StatSerlialiseTime;
		volatile int64* StatChunksSaved;
		volatile int64* StatDataWritten;
		volatile int64* StatDataWriteSpeed;
		volatile int64* StatCompressionRatio;

		TArray<TFuture<void>> WriterThreads;
		FThreadSafeBool bMoreDataIsExpected;
		FThreadSafeBool bShouldAbort;

		FCriticalSection ChunkDataJobQueueConsumerCS;
		TQueue<FChunkDataJob, EQueueMode::Spsc> ChunkDataJobQueue;
		FThreadSafeInt32 ChunkDataJobQueueCount;

		TQueue<FChunkOutputSize, EQueueMode::Mpsc> ChunkOutputSizeQueue;
		TQueue<FChunkOutputHash, EQueueMode::Mpsc> ChunkOutputHashQueue;
		TQueue<FChunkOutputSha, EQueueMode::Mpsc> ChunkOutputShaQueue;
		FParallelChunkWriterSummaries ParallelChunkWriterSummaries;

#if !UE_BUILD_SHIPPING
		TOptional<uint32> ProducerThreadId;
#endif
	};

	IParallelChunkWriter* FParallelChunkWriterFactory::Create(FParallelChunkWriterConfig Config, IFileSystem* FileSystem, IChunkDataSerialization* ChunkDataSerialization, FStatsCollector* StatsCollector)
	{
		return new FParallelChunkWriter(MoveTemp(Config), FileSystem, ChunkDataSerialization, StatsCollector);
	}
}
