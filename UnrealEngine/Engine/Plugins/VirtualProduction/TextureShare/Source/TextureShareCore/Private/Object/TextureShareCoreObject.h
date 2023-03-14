// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareCoreObject.h"
#include "Core/TextureShareCore.h"

#include "IPC/Containers/TextureShareCoreInterprocessObject.h"
#include "IPC/Containers/TextureShareCoreInterprocessEnums.h"

/**
 * TextureShareCore object api implementation
 */
class FTextureShareCoreObject
	: public ITextureShareCoreObject
{
public:
	FTextureShareCoreObject(FTextureShareCore& InOwner, const FString& InTextureShareName);
	virtual ~FTextureShareCoreObject();

public:
	///////////////////// State /////////////////////

	virtual const FString& GetName() const override;
	virtual const FTextureShareCoreObjectDesc& GetObjectDesc() const override;

	virtual bool IsActive() const override;
	virtual bool IsFrameSyncActive() const override;

private:
	FTextureShareCoreObjectDesc ObjectDesc;

public:
	///////////////////// Settings /////////////////////

	virtual bool SetProcessId(const FString& InProcessId) override;
	virtual bool SetDeviceType(const ETextureShareDeviceType InDeviceType) override;

	virtual bool SetSyncSetting(const FTextureShareCoreSyncSettings& InSyncSetting) override;
	virtual const FTextureShareCoreSyncSettings& GetSyncSetting() const override;

	virtual FTextureShareCoreFrameSyncSettings GetFrameSyncSettings(const ETextureShareFrameSyncTemplate InType) const override;

private:
	// Delayed process id changes
	FString NextProcessId;

	// Delayed sync settings. Changes are made from BeginFrameSync()
	TSharedPtr<FTextureShareCoreSyncSettings, ESPMode::ThreadSafe> NewSyncSetting;

	// This settings used for a frame
	FTextureShareCoreSyncSettings SyncSettings;

protected:
	void UpdateProcessId(FTextureShareCoreInterprocessObject& InOutObject);
	void UpdateFrameSyncSetting();

public:
	///////////////////// Session /////////////////////

	virtual bool BeginSession() override;
	virtual bool EndSession() override;
	virtual bool IsSessionActive() const override;

private:
	bool bSessionActive = false;

public:
	///////////////////// Thread sync support /////////////////////

	virtual bool LockThreadMutex(const ETextureShareThreadMutex InThreadMutex) override;
	virtual bool UnlockThreadMutex(const ETextureShareThreadMutex InThreadMutex) override;

	virtual bool IsBeginFrameSyncActive() const override;
	virtual bool IsBeginFrameSyncActive_RenderThread() const override;

protected:
	TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe> GetThreadMutex(const ETextureShareThreadMutex InThreadMutex);

	void ResetThreadMutexes();
	void ReleaseThreadMutexes();

private:
	TMap<ETextureShareThreadMutex, TSharedPtr<FTextureShareCoreInterprocessMutex, ESPMode::ThreadSafe>> ThreadMutexMap;

public:
	///////////////////// Interprocess Synchronization /////////////////////

	virtual bool BeginFrameSync() override;
	virtual bool FrameSync(const ETextureShareSyncStep InSyncStep) override;
	virtual bool EndFrameSync() override;

	virtual bool BeginFrameSync_RenderThread() override;
	virtual bool FrameSync_RenderThread(const ETextureShareSyncStep InSyncStep) override;
	virtual bool EndFrameSync_RenderThread() override;

	virtual const TArraySerializable<FTextureShareCoreObjectDesc>& GetConnectedInterprocessObjects() const override;

protected:
	void ReleaseSyncData();

private:
	// When the frame starts, a list of processes connected to the frame is created.
	// New processes connected later inside this framelock will be ignored.
	TArraySerializable<FTextureShareCoreObjectDesc> FrameConnections;

	// Prevend double check for begion connection timeout
	bool bIsFrameConnectionTimeoutReached = false;

	ETextureShareCoreInterprocessObjectFrameSyncState FrameSyncState = ETextureShareCoreInterprocessObjectFrameSyncState::Undefined;
	ETextureShareSyncStep CurrentSyncStep = ETextureShareSyncStep::Undefined;

	// Local process notification event
	TSharedPtr<FEvent, ESPMode::ThreadSafe> NotificationEvent;

	// Opened events for remote processes
	TMap<FGuid, TSharedPtr<FEvent, ESPMode::ThreadSafe>> CachedNotificationEvents;

protected:
	bool TryFrameProcessesConnection(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject);
	bool ConnectFrameProcesses();
	bool DisconnectFrameProcesses();

	bool DoFrameSync(const ETextureShareSyncStep InSyncStep);
	bool DoFrameSync_RenderThread(const ETextureShareSyncStep InSyncStep);

	// Barrier
	bool BeginSyncBarrier(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject, const ETextureShareSyncStep InSyncStep, const ETextureShareSyncPass InSyncPass);
	bool AcceptSyncBarrier(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject, const ETextureShareSyncStep InSyncStep, const ETextureShareSyncPass InSyncPass);
	bool PrepareSyncBarrierPass(const ETextureShareSyncStep InSyncStep);

	bool TryEnterSyncBarrier(const ETextureShareSyncStep InSyncStep) const;
	bool TryFrameProcessesBarrier(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject, const ETextureShareSyncStep InSyncStep, const ETextureShareSyncPass InSyncPass);

	// Wait until all connected processes entered to desired sync step
	bool SyncBarrierPass(const ETextureShareSyncStep InSyncStep, const ETextureShareSyncPass InSyncPass);

	void SetCurrentSyncStep(const ETextureShareSyncStep InSyncStep);
	void SetFrameSyncState(const ETextureShareCoreInterprocessObjectFrameSyncState InFrameSyncState);

	ETextureShareSyncStep FindNextSyncStep(const ETextureShareSyncStep InSyncStep) const;

	void UpdateLastAccessTime() const;

	bool TryWaitFrameProcesses(const uint32 InRemainMaxMillisecondsToWait);
	void SendNotificationEvents(const bool bInterprocessMemoryLockRequired);

	void HandleResetSync(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject);
	void HandleFrameLost(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject);
	void HandleFrameSkip(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject);

public:
	///////////////////// Data Containers /////////////////////

	virtual FTextureShareCoreData&      GetData() override;
	virtual FTextureShareCoreProxyData& GetProxyData_RenderThread() override;

	virtual const TArraySerializable<FTextureShareCoreObjectData>&      GetReceivedData() const override;
	virtual const TArraySerializable<FTextureShareCoreObjectProxyData>& GetReceivedProxyData_RenderThread() const override;

protected:
	void ReleaseData();

private:
	// Local process data
	FTextureShareCoreData      Data;
	FTextureShareCoreProxyData ProxyData;

	// Local copy of transfered resources and data
	TArraySerializable<FTextureShareCoreObjectData>      ReceivedObjectsData;
	TArraySerializable<FTextureShareCoreObjectProxyData> ReceivedObjectsProxyData;

public:
	///////////////////// Destructor /////////////////////

	virtual bool RemoveObject() override;

protected:
	// Data
	bool SendFrameData();
	bool ReceiveFrameData();

	bool SendFrameProxyData_RenderThread();
	bool ReceiveFrameProxyData_RenderThread();

private:
	FTextureShareCore& Owner;
};
