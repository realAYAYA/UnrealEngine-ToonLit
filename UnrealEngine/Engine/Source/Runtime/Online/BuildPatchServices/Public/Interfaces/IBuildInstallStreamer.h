// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchState.h"
#include "BuildPatchMessage.h"
#include "Interfaces/IBuildInstaller.h"

namespace BuildPatchServices
{
	struct FBuildInstallStreamerConfiguration;
	struct FBuildInstallStreamerStats;
}

typedef TSharedPtr<class IBuildInstallStreamer, ESPMode::ThreadSafe> IBuildInstallStreamerPtr;
typedef TSharedRef<class IBuildInstallStreamer, ESPMode::ThreadSafe> IBuildInstallStreamerRef;
typedef TWeakPtr<class IBuildInstallStreamer, ESPMode::ThreadSafe> IBuildInstallStreamerWeakPtr;

struct FBuildPatchStreamResult
{
public:
	TSet<FString> Request;
	EBuildPatchInstallError ErrorType;
	FString ErrorCode;
	int64 TotalDownloaded;
};

DECLARE_DELEGATE_OneParam(FBuildPatchStreamCompleteDelegate, FBuildPatchStreamResult /*Result*/);

class IBuildInstallStreamer
{
public:
	/**
	 * Virtual destructor.
	 */
	virtual ~IBuildInstallStreamer() { }
	
	virtual void QueueFilesByTag(TSet<FString> Tags, FBuildPatchStreamCompleteDelegate OnComplete) = 0;
	virtual void QueueFilesByName(TSet<FString> Files, FBuildPatchStreamCompleteDelegate OnComplete) = 0;
	virtual void CancelAllRequests() = 0;

	/**
	 * Registers a message handler with the installer.
	 * @param MessageHandler    Ptr to the message handler to add. Must not be null.
	 */
	virtual void RegisterMessageHandler(BuildPatchServices::FMessageHandler* MessageHandler) = 0;

	/**
	 * Unregisters a message handler, will no longer receive HandleMessage calls.
	 * @param MessageHandler    Ptr to the message handler to remove.
	 */
	virtual void UnregisterMessageHandler(BuildPatchServices::FMessageHandler* MessageHandler) = 0;

	/**
	 * Get the configuration object.
	 * 
	 * @returns a const reference to the configuration.
	 */
	virtual const BuildPatchServices::FBuildInstallStreamerConfiguration& GetConfiguration() const = 0;

	/**
	 * Get the stats for this install streamer.
	 * 
	 * @returns a const reference to the statistics.
	 */
	virtual const BuildPatchServices::FBuildInstallStreamerStats& GetInstallStreamerStatistics() const = 0;
	
	/**
	 * Get the per-session stats for this install streamer.
	 * 
	 * @returns a const reference to the statistics.
	 */
	virtual const BuildPatchServices::FBuildInstallStreamerStats& GetInstallStreamerSessionStatistics() const = 0;
	
	/**
	 * Resets tracking on the per-session stats for this install streamer.
	 */
	virtual void ResetSessionStatistics() = 0;
};