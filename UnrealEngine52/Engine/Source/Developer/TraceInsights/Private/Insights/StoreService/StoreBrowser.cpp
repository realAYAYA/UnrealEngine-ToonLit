// Copyright Epic Games, Inc. All Rights Reserved.

#include "StoreBrowser.h"

#include "Async/ParallelFor.h"
#include "Trace/Analysis.h"
#include "Trace/StoreClient.h"

// Insights
#include "Insights/Common/Stopwatch.h"
#include "Insights/InsightsManager.h"
#include "Insights/Log.h"
#include "Insights/StoreService/DiagnosticsSessionAnalyzer.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

FStoreBrowser::FStoreBrowser()
	: bRunning(true)
	, Thread(nullptr)
	, TracesCriticalSection()
	, bTracesLocked(false)
	, StoreChangeSerial(0)
	, TracesChangeSerial(0)
	, Traces()
	, TraceMap()
	, LiveTraceMap()
{
	Thread = FRunnableThread::Create(this, TEXT("StoreBrowser"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FStoreBrowser::~FStoreBrowser()
{
	Stop();

	if (Thread)
	{
		Thread->Kill(true);
		Thread = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FStoreBrowser::Init()
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FStoreBrowser::Run()
{
	while (bRunning)
	{
		FPlatformProcess::SleepNoStats(0.5f);
		UpdateTraces();
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FStoreBrowser::Stop()
{
	bRunning = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FStoreBrowser::Exit()
{
	ResetTraces();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FStoreBrowser::Refresh()
{
	if (!IsLocked())
	{
		FScopeLock Lock(&TracesCriticalSection);

		StoreChangeSerial = 0;
		TracesChangeSerial = 0;
		Traces.Reset();
		TraceMap.Reset();
		LiveTraceMap.Reset();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

UE::Trace::FStoreClient* FStoreBrowser::GetStoreClient() const
{
	return FInsightsManager::Get()->GetStoreClient();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FCriticalSection& FStoreBrowser::GetStoreClientCriticalSection() const
{
	return FInsightsManager::Get()->GetStoreClientCriticalSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FStoreBrowser::UpdateTraces()
{
	UE::Trace::FStoreClient* StoreClient = GetStoreClient();
	if (StoreClient == nullptr)
	{
		ResetTraces();
		return;
	}

	FStopwatch StopwatchTotal;
	StopwatchTotal.Start();

	FScopeLock StoreClientLock(&GetStoreClientCriticalSection());

	// Check if the list of trace sessions has changed.
	{
		const UE::Trace::FStoreClient::FStatus* Status = StoreClient->GetStatus();
		if (Status != nullptr && StoreChangeSerial != Status->GetChangeSerial())
		{
			StoreChangeSerial = Status->GetChangeSerial();

			// Check for removed traces.
			{
				for (int32 TraceIndex = Traces.Num() - 1; TraceIndex >= 0; --TraceIndex)
				{
					const uint32 TraceId = Traces[TraceIndex]->TraceId;
					const UE::Trace::FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(TraceId);
					if (TraceInfo == nullptr)
					{
						FScopeLock Lock(&TracesCriticalSection);
						TracesChangeSerial++;
						Traces.RemoveAtSwap(TraceIndex);
						TraceMap.Remove(TraceId);
					}
				}
			}

			// Check for added traces.
			{
				const int32 TraceCount = StoreClient->GetTraceCount();
				for (int32 TraceIndex = 0; TraceIndex < TraceCount; ++TraceIndex)
				{
					const UE::Trace::FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfo(TraceIndex);
					if (TraceInfo != nullptr)
					{
						const uint32 TraceId = TraceInfo->GetId();
						TSharedPtr<FStoreBrowserTraceInfo>* TracePtrPtr = TraceMap.Find(TraceId);
						if (TracePtrPtr == nullptr)
						{
							TSharedPtr<FStoreBrowserTraceInfo> TracePtr = MakeShared<FStoreBrowserTraceInfo>();
							FStoreBrowserTraceInfo& Trace = *TracePtr;

							Trace.TraceId = TraceId;
							Trace.TraceIndex = TraceIndex;

							const FUtf8StringView Utf8NameView = TraceInfo->GetName();
							Trace.Name = FString(Utf8NameView);

							Trace.Timestamp = FStoreBrowserTraceInfo::ConvertTimestamp(TraceInfo->GetTimestamp());
							Trace.Size = TraceInfo->GetSize();

							FScopeLock Lock(&TracesCriticalSection);
							TracesChangeSerial++;
							Traces.Add(TracePtr);
							TraceMap.Add(TraceId, TracePtr);
						}
					}
				}
			}
		}
	}

	StopwatchTotal.Update();
	const uint64 Step1 = StopwatchTotal.GetAccumulatedTimeMs();

	// Update the live trace sessions.
	{
		TArray<uint32> NotLiveTraces;

		for (const auto& KV : LiveTraceMap)
		{
			const uint32 TraceId = KV.Key;
			FStoreBrowserTraceInfo& Trace = *KV.Value;

			ensure(Trace.bIsLive);

			FDateTime Timestamp(0);
			uint64 Size = 0;
			const UE::Trace::FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(TraceId);
			if (TraceInfo != nullptr)
			{
				Timestamp = FStoreBrowserTraceInfo::ConvertTimestamp(TraceInfo->GetTimestamp());
				Size = TraceInfo->GetSize();
			}

			const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfoByTraceId(TraceId);
			if (SessionInfo != nullptr)
			{
				// The trace is still live.
				const uint32 IpAddress = SessionInfo->GetIpAddress();
				if (IpAddress != Trace.IpAddress || Timestamp != Trace.Timestamp || Size != Trace.Size)
				{
					FScopeLock Lock(&TracesCriticalSection);
					TracesChangeSerial++;
					Trace.ChangeSerial++;
					Trace.Timestamp = Timestamp;
					Trace.Size = Size;
					Trace.IpAddress = IpAddress;
				}
			}
			else
			{
				NotLiveTraces.Add(TraceId);

				// The trace is not live anymore.
				FScopeLock Lock(&TracesCriticalSection);
				TracesChangeSerial++;
				Trace.ChangeSerial++;
				Trace.Timestamp = Timestamp;
				Trace.Size = Size;
				Trace.bIsLive = false;
				Trace.IpAddress = 0;
			}
		}

		for (const uint32 TraceId : NotLiveTraces)
		{
			LiveTraceMap.Remove(TraceId);
		}
	}

	StopwatchTotal.Update();
	const uint64 Step2 = StopwatchTotal.GetAccumulatedTimeMs();

	// Check if we have new live sessions.
	{
		const uint32 SessionCount = StoreClient->GetSessionCount();
		for (uint32 SessionIndex = 0; SessionIndex < SessionCount; ++SessionIndex)
		{
			const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfo(SessionIndex);
			if (SessionInfo != nullptr)
			{
				const uint32 TraceId = SessionInfo->GetTraceId();
				if (!LiveTraceMap.Find(TraceId))
				{
					TSharedPtr<FStoreBrowserTraceInfo>* TracePtrPtr = TraceMap.Find(TraceId);
					if (TracePtrPtr)
					{
						// This trace is a new live session.
						LiveTraceMap.Add(TraceId, *TracePtrPtr);

						const uint32 IpAddress = SessionInfo->GetIpAddress();

						FDateTime Timestamp(0);
						uint64 Size = 0;
						const UE::Trace::FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(TraceId);
						if (TraceInfo != nullptr)
						{
							Timestamp = FStoreBrowserTraceInfo::ConvertTimestamp(TraceInfo->GetTimestamp());
							Size = TraceInfo->GetSize();
						}

						FStoreBrowserTraceInfo& Trace = **TracePtrPtr;

						FScopeLock Lock(&TracesCriticalSection);
						TracesChangeSerial++;
						Trace.ChangeSerial++;
						Trace.Timestamp = Timestamp;
						Trace.Size = Size;
						Trace.bIsLive = true;
						Trace.IpAddress = IpAddress;
					}
				}
			}
		}
	}

	StoreClientLock.Unlock();

	StopwatchTotal.Update();
	const uint64 Step3 = StopwatchTotal.GetAccumulatedTimeMs();

	// Check to see if we need to update metadata.
	{
		FStopwatch Stopwatch;
		Stopwatch.Start();

		struct FStoreBrowserTraceInfoBox
		{
			FStoreBrowserTraceInfo* Ptr;
		};
		TArray<FStoreBrowserTraceInfoBox> TracesToUpdate;

		for (TSharedPtr<FStoreBrowserTraceInfo>& TracePtr : Traces)
		{
			if (TracePtr->MetadataUpdateCount > 0)
			{
				TracesToUpdate.Add({ TracePtr.Get() });
			}
		}

		// Sort descending by timestamp (i.e. to update the newer traces first).
		TracesToUpdate.Sort([](const FStoreBrowserTraceInfoBox& A, const FStoreBrowserTraceInfoBox& B)
			{
				return A.Ptr->Timestamp.GetTicks() > B.Ptr->Timestamp.GetTicks();
			});

#if 0
		for (const FStoreBrowserTraceInfoBox& Trace : TracesToUpdate)
		{
			UpdateMetadata(*Trace.Ptr);
		}
#else
		if (TracesToUpdate.Num() > 0)
		{
			ParallelFor(TracesToUpdate.Num(), [this, &TracesToUpdate](uint32 Index)
				{
					UpdateMetadata(*TracesToUpdate[Index].Ptr);
				},
				EParallelForFlags::BackgroundPriority);
		}
#endif

		Stopwatch.Stop();
		if (TracesToUpdate.Num() > 0)
		{
			UE_LOG(TraceInsights, Log, TEXT("[StoreBrowser] Metadata updated in %llu ms for %d trace(s) (~%llu ms/trace)."),
				Stopwatch.GetAccumulatedTimeMs(),
				TracesToUpdate.Num(),
				Stopwatch.GetAccumulatedTimeMs() / TracesToUpdate.Num());
		}
	}

	StopwatchTotal.Stop();
	const uint64 TotalTime = StopwatchTotal.GetAccumulatedTimeMs();
	if (TotalTime > 5)
	{
		UE_LOG(TraceInsights, Log, TEXT("[StoreBrowser] Updated in %llu ms (%llu + %llu + %llu + %llu)."),
			TotalTime, Step1, Step2 - Step1, Step3 - Step2, TotalTime - Step3);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FStoreBrowser::ResetTraces()
{
	if (Traces.Num() > 0)
	{
		FScopeLock Lock(&TracesCriticalSection);
		TracesChangeSerial++;
		Traces.Reset();
		TraceMap.Reset();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FStoreBrowser::UpdateMetadata(FStoreBrowserTraceInfo& Trace)
{
	UE::Trace::FStoreClient* StoreClient = GetStoreClient();
	if (StoreClient == nullptr)
	{
		return;
	}

	FPlatformProcess::SleepNoStats(0.0f);

	FScopeLock StoreClientLock(&GetStoreClientCriticalSection());
	UE::Trace::FStoreClient::FTraceData TraceData = StoreClient->ReadTrace(Trace.TraceId);
	StoreClientLock.Unlock();
	if (!TraceData)
	{
		return;
	}

	struct FDataStream : public UE::Trace::IInDataStream
	{
		enum class EReadStatus
		{
			Ready = 0,
			StoppedByReadSizeLimit,
			StoppedByTimeLimit,
		};

		virtual int32 Read(void* Data, uint32 Size) override
		{
			if (BytesRead >= 1024 * 1024)
			{
				Status = EReadStatus::StoppedByReadSizeLimit;
				return 0;
			}

			Stopwatch.Update();
			if (Stopwatch.GetAccumulatedTime() > TimeLimit)
			{
				Status = EReadStatus::StoppedByTimeLimit;
				return 0;
			}

			const int32 InnerBytesRead = Inner->Read(Data, Size);
			BytesRead += InnerBytesRead;

			return InnerBytesRead;
		}

		virtual void Close() override
		{
			Inner->Close();
		}

		UE::Trace::IInDataStream* Inner;
		double TimeLimit = 1.0;
		FStopwatch Stopwatch;
		int32 BytesRead = 0;
		EReadStatus Status = EReadStatus::Ready;
	};

	FDataStream DataStream;
	DataStream.Inner = TraceData.Get();
	DataStream.TimeLimit = 1.0 + (double)(Trace.MetadataUpdateCount - 1) * 2.0; // 1s, 3s, 5s, ...
	DataStream.Stopwatch.Start();

	FDiagnosticsSessionAnalyzer Analyzer;
	UE::Trace::FAnalysisContext Context;
	Context.AddAnalyzer(Analyzer);
	Context.Process(DataStream).Wait();

	// Update the FStoreBrowserTraceInfo object.
	{
		FScopeLock Lock(&TracesCriticalSection);

		if (Analyzer.Platform.Len() != 0)
		{
			TracesChangeSerial++;
			Trace.ChangeSerial++;
			Trace.Platform = Analyzer.Platform;
			Trace.AppName = Analyzer.AppName;
			Trace.CommandLine = Analyzer.CommandLine;
			Trace.Branch = Analyzer.Branch;
			Trace.BuildVersion = Analyzer.BuildVersion;
			Trace.Changelist = Analyzer.Changelist;
			Trace.ConfigurationType = static_cast<EBuildConfiguration>(Analyzer.ConfigurationType);
			Trace.TargetType = static_cast<EBuildTargetType>(Analyzer.TargetType);
		}

		if (DataStream.Status == FDataStream::EReadStatus::StoppedByTimeLimit)
		{
			// Try again later.
			++Trace.MetadataUpdateCount;
			if (Trace.MetadataUpdateCount > 5)
			{
				Trace.MetadataUpdateCount = 0; // no more updates
			}
		}
		else
		{
			Trace.MetadataUpdateCount = 0; // no more updates
		}
	}

	DataStream.Stopwatch.Stop();
	UE_LOG(TraceInsights, Log, TEXT("[StoreBrowser] Metadata updated in %llu ms for trace \"%s\""),
		DataStream.Stopwatch.GetAccumulatedTimeMs(), *Trace.Name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
