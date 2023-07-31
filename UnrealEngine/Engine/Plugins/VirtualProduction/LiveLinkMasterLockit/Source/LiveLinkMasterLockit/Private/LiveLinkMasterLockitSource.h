// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkSource.h"

#include "LiveLinkMasterLockitSourceSettings.h"
#include "LiveLinkMasterLockitConnectionSettings.h"

#include "MasterLockitMessageThread.h"

#include <atomic>

static constexpr int MasterLockitPortNumber = 42000; // From the MasterLockit API

class ISocketSubsystem;

class LIVELINKMASTERLOCKIT_API FLiveLinkMasterLockitSource : public ILiveLinkSource
{
public:

	FLiveLinkMasterLockitSource(FLiveLinkMasterLockitConnectionSettings ConnectionSettings);
	~FLiveLinkMasterLockitSource();

	// Begin ILiveLinkSource Implementation
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;

	virtual bool IsSourceStillValid() const override;

	virtual bool RequestSourceShutdown() override;

	virtual FText GetSourceType() const override;
	virtual FText GetSourceMachineName() const override { return SourceMachineName; }
	virtual FText GetSourceStatus() const override;

	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const { return ULiveLinkMasterLockitSourceSettings::StaticClass(); }
	// End ILiveLinkSourceImplementation

private:
	void OpenConnection();
	void OnHandshakeEstablished_AnyThread();
	void OnFrameDataReady_AnyThread(FLensPacket InData);

	ILiveLinkClient* Client = nullptr;
	FSocket* Socket = nullptr;
	ISocketSubsystem* SocketSubsystem = nullptr;

	FLiveLinkMasterLockitConnectionSettings ConnectionSettings;
	FLiveLinkSubjectKey SubjectKey;
	FText SourceMachineName;

	TUniquePtr<FMasterLockitMessageThread> MessageThread;

	std::atomic<double> LastTimeDataReceived;
	std::atomic<bool> bReceivedData;

private:
	const float DataReceivedTimeout = 20.0f;
};