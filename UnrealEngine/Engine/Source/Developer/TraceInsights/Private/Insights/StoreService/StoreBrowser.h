// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/AsyncWork.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/CriticalSection.h"
#include "Templates/Atomic.h"

namespace UE
{
namespace Trace
{
	class FStoreClient;
}
}

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FStoreBrowserTraceInfo
{
	uint32 TraceId = 0;
	int32 TraceIndex = -1;

	uint64 ChangeSerial = 0;

	FString Name;
	FString Uri;

	FDateTime Timestamp = 0;
	uint64 Size = 0;

	FString Platform;
	FString AppName;
	FString ProjectName;
	FString CommandLine;
	FString Branch;
	FString BuildVersion;
	uint32 Changelist;
	EBuildConfiguration ConfigurationType = EBuildConfiguration::Unknown;
	EBuildTargetType TargetType = EBuildTargetType::Unknown;

	bool bIsLive = false;
	uint8 MetadataUpdateCount = 1;

	uint32 IpAddress = 0;

	FStoreBrowserTraceInfo() = default;

	static FDateTime ConvertTimestamp(uint64 InTimestamp)
	{
		return FDateTime(static_cast<int64>(InTimestamp));
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FStoreBrowser : public FRunnable
{
	friend class FStoreBrowserUpdaterAsyncTask;

public:
	FStoreBrowser();
	virtual ~FStoreBrowser();

	//////////////////////////////////////////////////
	// FRunnable interface

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	//////////////////////////////////////////////////

	enum class EConnectionStatus : uint8
	{
		// Attempting connection
		Connecting = 0,
		// Values between (start,end) is interpreted as
		// number of seconds until next reconnection attemp
		SecondsToReconnectStart,
		SecondsToReconnectEnd = 0xfd,
		// No connection could be made, no more reconnection
		// attempts are made.
		NoConnection = 0xfe,
		// Connection is active
		Connected = 0xff
	};

	EConnectionStatus GetConnectionStatus() const { return ConnectionStatus; }
	bool IsRunning() const { return bRunning; }

	bool IsLocked() const { return bTracesLocked; }
	void Lock() { check(!bTracesLocked); bTracesLocked = true; TracesCriticalSection.Lock(); }
	uint64 GetLockedTracesChangeSerial() const { check(bTracesLocked); return TracesChangeSerial; }
	uint32 GetLockedSettingsSerial() const { check(bTracesLocked); return SettingsChangeSerial; };
	const TArray<TSharedPtr<FStoreBrowserTraceInfo>>& GetLockedTraces() const { check(bTracesLocked); return Traces; }
	const TMap<uint32, TSharedPtr<FStoreBrowserTraceInfo>>& GetLockedTraceMap() const { check(bTracesLocked); return TraceMap; }
	const TArray<FString>& GetLockedWatchDirectories() const { check(bTracesLocked); return WatchDirectories; }
	const FString& GetLockedStoreDirectory() const { check(bTracesLocked); return StoreDirectory; }
	const FString& GetLockedHost() const { check(bTracesLocked); return Host; }
	void Unlock() { check(bTracesLocked); bTracesLocked = false; TracesCriticalSection.Unlock(); }
	void Refresh();

private:
	UE::Trace::FStoreClient* GetStoreClient() const;
	FCriticalSection& GetStoreClientCriticalSection() const;

	void UpdateTraces();
	void ResetTraces();

	void UpdateMetadata(FStoreBrowserTraceInfo& TraceSession);

private:
	// Thread safe bool for stopping the thread
	FThreadSafeBool bRunning;

	// Thread for continously updating and caching info about trace store.
	FRunnableThread* Thread;

	mutable FCriticalSection TracesCriticalSection;
	FThreadSafeBool bTracesLocked; // for debugging, to ensure GetLocked*() methods are only called between Lock() - Unlock() calls.
	std::atomic<EConnectionStatus> ConnectionStatus;
	uint32 StoreChangeSerial;
	uint64 TracesChangeSerial;
	uint32 SettingsChangeSerial;
	TArray<TSharedPtr<FStoreBrowserTraceInfo>> Traces;
	TMap<uint32, TSharedPtr<FStoreBrowserTraceInfo>> TraceMap;
	TMap<uint32, TSharedPtr<FStoreBrowserTraceInfo>> LiveTraceMap;
	TArray<FString> WatchDirectories;
	FString StoreDirectory;
	FString Host;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
