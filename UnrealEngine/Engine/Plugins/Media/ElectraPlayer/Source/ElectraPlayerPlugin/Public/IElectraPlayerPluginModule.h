// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"
#include "Misc/ScopeLock.h"
#include "Misc/Timespan.h"
#include "Containers/Array.h"
#include "IMediaOptions.h"

class IAnalyticsProviderET;
class IMediaEventSink;
class IMediaPlayer;


/**
 * This class is used to get safe access to an IMediaOptions interface.
 * When passing IMediaOptions through media framework as a POD pointer there is the risk
 * that it is actually from a derived class like UMediaSource which is subject to GC.
 * Thus it is not safe to keep the POD IMediaOptions around.
 * This class is intended to be derived and instantiated from and stored as a TSharedPtr
 * in a derived UMediaSource class.
 * Then, as the media player is opened and the POD IMediaOptions is passed along, a
 * GetMediaOption("GetSafeMediaOptions") query will be made on it to get this instance.
 * If future access to the IMediaOptions is needed it will be made through this class
 * by first locking, getting and using the IMediaOptions pointer returned here if it
 * not null and unlocking afterwards.
 */
class IElectraSafeMediaOptionInterface : public IMediaOptions::FDataContainer
{
public:
	virtual ~IElectraSafeMediaOptionInterface() = default;
	virtual void Lock() = 0;
	virtual void Unlock() = 0;
	virtual IMediaOptions* GetMediaOptionInterface() = 0;

	class FScopedLock
	{
	public:
		FScopedLock(const TSharedPtr<IElectraSafeMediaOptionInterface, ESPMode::ThreadSafe>& InSafeMediaOptionInterface)
			: SafeMediaOptionInterface(InSafeMediaOptionInterface)
		{
			if (SafeMediaOptionInterface.IsValid())
			{
				SafeMediaOptionInterface->Lock();
			}
		}
		~FScopedLock()
		{
			if (SafeMediaOptionInterface.IsValid())
			{
				SafeMediaOptionInterface->Unlock();
			}
		}
	
	private:
		TSharedPtr<IElectraSafeMediaOptionInterface, ESPMode::ThreadSafe> SafeMediaOptionInterface;
	};
};


/**
 * A ready to use implementation of the IElectraSafeMediaOptionInterface
 */
class FElectraSafeMediaOptionInterface : public IElectraSafeMediaOptionInterface
{
public:
	FElectraSafeMediaOptionInterface(IMediaOptions* InOwner)
		: Owner(InOwner)
	{ }
	virtual ~FElectraSafeMediaOptionInterface()
	{
		ClearOwner();
	}
	void ClearOwner()
	{
		FScopeLock lock(&OwnerLock);
		Owner = nullptr;
	}
	virtual void Lock() override
	{
		OwnerLock.Lock();
	}
	virtual void Unlock() override
	{
		OwnerLock.Unlock();
	}
	virtual IMediaOptions* GetMediaOptionInterface() override
	{
		return Owner;
	}
private:
	FCriticalSection OwnerLock;
	IMediaOptions* Owner = nullptr;
};



//! Data type for use with media options interface
class FElectraSeekablePositions : public IMediaOptions::FDataContainer
{
public:
	FElectraSeekablePositions(const TArray<FTimespan>& InData) : Data(InData) {}
	virtual ~FElectraSeekablePositions() {}

	TArray<FTimespan> Data;
};


/**
 * Interface for the ElectraPlayerPlugin module.
 */
class IElectraPlayerPluginModule
	: public IModuleInterface
{
public:

	/**
	 * Is the ElectraPlayerPlugin module initialized?
	 * @return True if the module is initialized.
	 */
	virtual bool IsInitialized() const = 0;

	/**
	 * Creates a media player
	 *
	 * @param EventSink The object that receives media events from the player.
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) = 0;

	virtual void SendAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider, const FGuid& PlayerGuid) = 0;
	virtual void SendAnalyticMetricsPerMinute(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider) = 0;

	virtual void ReportVideoStreamingError(const FGuid& PlayerGuid, const FString& LastError) = 0;
	virtual void ReportSubtitlesMetrics(const FGuid& PlayerGuid, const FString& URL, double ResponseTime, const FString& LastError) = 0;


public:
	virtual ~IElectraPlayerPluginModule() { }
};
