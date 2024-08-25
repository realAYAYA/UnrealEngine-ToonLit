// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TextureGraphEngine.h"
#include "Device/Device.h"
#include "Device/DeviceObserverSource.h"
#include "Data/Blobber.h"
#include "Job/Scheduler.h"

using HashArray = DeviceObserverSource::HashArray;

DECLARE_LOG_CATEGORY_EXTERN(LogTextureGraphInsightObserver, Log, All);

/// Concrete DeviceObserver interface
class TEXTUREGRAPHINSIGHT_API TextureGraphInsightDeviceObserver : public DeviceObserverSource
{
protected:
	DeviceType DevType;

	/// Protected interface of emitters called by the device to notify the observers
	virtual void DeviceBuffersUpdated(HashNDescArray&& AddedBuffers, HashArray&& RemovedBuffers) override;
public:
	TextureGraphInsightDeviceObserver();
	explicit TextureGraphInsightDeviceObserver(DeviceType);
	virtual ~TextureGraphInsightDeviceObserver() override;
};

/// Concrete BlobberObserver interface
class TEXTUREGRAPHINSIGHT_API TextureGraphInsightBlobberObserver : public BlobberObserverSource
{
private:
protected:
	/// Protected interface of emitters called by the device to notify the observers
	virtual void BlobberUpdated(HashArray&& AddedHashes, HashArray&& RemappedHashes) override;

public:
	TextureGraphInsightBlobberObserver();
	virtual ~TextureGraphInsightBlobberObserver() override;
};

/// Concrete SchedulerObserver interface
class TEXTUREGRAPHINSIGHT_API TextureGraphInsightSchedulerObserver : public SchedulerObserverSource
{
private:
protected:
	/// Protected interface of emitters called by the scheduler to notify the observers
	virtual void Start() override;
	virtual void UpdateIdle() override;
	virtual void Stop() override;
	virtual void BatchAdded(JobBatchPtr Batch) override;
	virtual void BatchDone(JobBatchPtr Batch) override;
	virtual void BatchJobsDone(JobBatchPtr Batch) override;

public:

	TextureGraphInsightSchedulerObserver();
	virtual ~TextureGraphInsightSchedulerObserver() override;
};

/// Concrete EngineObserver interface
/// Responsible for:
///	  1/ watching the engine life cycle
///   2/ owning the other system observers, and installing them appropirately when an Engine is active
///	  3/ notifying Insight
class TEXTUREGRAPHINSIGHT_API TextureGraphInsightEngineObserver : public EngineObserverSource
{
protected:
	/// Protected interface of emitters called by the engine to notify the observers
	virtual void Created() override;
	virtual void Destroyed() override;

public:
	TextureGraphInsightEngineObserver();
	virtual ~TextureGraphInsightEngineObserver() override;

	std::shared_ptr<TextureGraphInsightDeviceObserver>			_deviceObservers[(uint32)DeviceType::Count];
	std::shared_ptr<TextureGraphInsightBlobberObserver>		BlobberObserver;
	std::shared_ptr<TextureGraphInsightSchedulerObserver>		SchedulerObserver;
};
