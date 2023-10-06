// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILauncherWorker.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"

class FLauncherTask;
class ITargetDeviceProxyManager;
class ITargetDeviceProxy;

struct FCommandDesc
{
	FString Name;
	FString Desc;
	FString EndText;
};

/**
 * Implements the launcher's worker thread.
 */
class FLauncherWorker
	: public FRunnable
	, public ILauncherWorker
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InProfile The profile to process.
	 * @param InDeviceProxyManager The target device proxy manager to use.
	 */
	FLauncherWorker(const TSharedRef<ITargetDeviceProxyManager>& InDeviceProxyManager, const ILauncherProfileRef& InProfile);

	~FLauncherWorker()
	{
		TaskChain.Reset();
		DisableDeviceDiscoveryListener();
	}

public:

	virtual bool Init( ) override;

	virtual uint32 Run( ) override;

	virtual void Stop( ) override;

	virtual void Exit( ) override { }

public:

	virtual void Cancel( ) override;

	virtual void CancelAndWait( ) override;

	virtual ELauncherWorkerStatus::Type GetStatus( ) const  override
	{
		return Status;
	}

	virtual int32 GetTasks( TArray<ILauncherTaskPtr>& OutTasks ) const override;

	virtual FOutputMessageReceivedDelegate& OnOutputReceived() override
	{
		return OutputMessageReceived;
	}

	virtual FOnStageStartedDelegate& OnStageStarted() override
	{
		return StageStarted;
	}

	virtual FOnStageCompletedDelegate& OnStageCompleted() override
	{
		return StageCompleted;
	}

	virtual FOnLaunchCompletedDelegate& OnCompleted() override
	{
		return LaunchCompleted;
	}

	virtual FOnLaunchCanceledDelegate& OnCanceled() override
	{
		return LaunchCanceled;
	}

	virtual ILauncherProfilePtr GetLauncherProfile() const override
	{
		return Profile;
	}

	/** Callback for when a device proxy has been removed from the device proxy manager. */
	void HandleDeviceProxyManagerProxyRemoved(const TSharedRef<ITargetDeviceProxy>& RemovedProxy);

	/**
	 * Set the app id running on the device
	 */
	virtual void AddDevicePackagePair(const FString& Device, const FString& Package) override
	{
		CachedDevicePackagePair.Add(Device, Package);
	}

protected:

	/**
	 * Creates the tasks for the specified profile.
	 *
	 * @param InProfile - The profile to create tasks for.
	 */
	void CreateAndExecuteTasks( const ILauncherProfileRef& InProfile );

	void OnTaskStarted(const FString& TaskName);
	void OnTaskCompleted(const FString& TaskName);

	FString CreateUATCommand( const ILauncherProfileRef& InProfile, const TArray<FString>& InPlatforms, TArray<FCommandDesc>& OutCommands, FString& CommandStart );

	//start listening to the device discovery routine
	void EnableDeviceDiscoveryListener();

	//stop listening to the device discovery routine
	void DisableDeviceDiscoveryListener();

	//Cancel the currently running application on all devices
	bool TerminateLaunchedProcess();

private:

	// Holds a critical section to lock access to selected properties.
	FCriticalSection CriticalSection;

	// Holds a pointer  to the device proxy manager.
	TSharedPtr<ITargetDeviceProxyManager> DeviceProxyManager;

	// Holds a pointer to the launcher profile.
	ILauncherProfilePtr Profile;

	// Holds the worker's current status.
	volatile ELauncherWorkerStatus::Type Status = ELauncherWorkerStatus::Busy;

	// Holds the first task in the task chain.
	TSharedPtr<FLauncherTask> TaskChain;

	// holds the read and write pipes and the running UAT process
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FProcHandle ProcHandle;

	double StageStartTime = 0.0;
	double LaunchStartTime = 0.0;

	// message delegate
	FOutputMessageReceivedDelegate OutputMessageReceived;
	FOnStageStartedDelegate StageStarted;
	FOnStageCompletedDelegate StageCompleted;
	FOnLaunchCompletedDelegate LaunchCompleted;
	FOnLaunchCanceledDelegate LaunchCanceled;

	// delegate for device discovery (device removed)
	FDelegateHandle OnProxyRemovedDelegateHandle;

	// Dictionary holding the appid@device pairs from the last launch
	TMap<FString, FString> CachedDevicePackagePair;
};
