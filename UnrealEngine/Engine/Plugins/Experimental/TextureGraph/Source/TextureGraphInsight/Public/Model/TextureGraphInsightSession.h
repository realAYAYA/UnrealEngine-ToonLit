// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TextureGraphInsightRecord.h"
#include "TextureGraphInsightObserver.h"

class UMixInterface;
using MixPtr = UMixInterface*;
using MixPtrW = UMixInterface*;
//using MixPtr = std::shared_ptr<UMixInterface>;
//using MixPtrW = std::weak_ptr<UMixInterface>;

class TEXTUREGRAPHINSIGHT_API TextureGraphInsightSession
{
public:
	/// Keep all the pointers to "real" TextureGraph Engine objects in the live runtime cache
	class LiveCache
	{
	public:
		LiveCache(TextureGraphInsightSession* InSession) : Session(InSession) {}
		TextureGraphInsightSession*		Session = nullptr;

		// TODO: Find a better way to keep the Batch pointer alive, for now, only the last one will still be valid
		std::vector<JobBatchPtrW>	Batches; 
		using						BlobEntry= HashType;
		std::vector<BlobEntry>		Blobs;
		std::vector<MixPtrW>		Mixes;

		void						AddBlob(RecordID RecId, BlobPtr BlobObj);
		BlobPtr						GetBlob(RecordID RecId) const;

		void						AddBatch(RecordID RecId, JobBatchPtr Batch);
		JobBatchPtr					GetBatch(RecordID RecId) const;

		uint64						DeviceCacheVersions[(uint32)DeviceType::Count] = { 0, 0, 0, 0, 0, 0 , 0};

		DeviceBufferPtr				GetDeviceBuffer(RecordID RecId) const;

		void						AddMix(RecordID RecId, MixPtr MixObj);
		MixPtr						GetMix(RecordID RecId) const;
	};

private:
protected:
	friend class TextureGraphInsightEngineObserver;
	/// Protected interface of emitters called by the engine observer
	void							EngineCreated();
	void							EngineDestroyed();
	void							EmitOnEngineReset(bool);

	/// manage DeviceBuffer update
	friend class TextureGraphInsightDeviceObserver;
	uint32							ParseDeviceBuffersCache(DeviceType type, RecordIDArray& addedBufferIds);
	void							DeviceBuffersUpdated(DeviceType DevType, TextureGraphInsightDeviceObserver::HashNDescArray&& AddedBuffers, TextureGraphInsightDeviceObserver::HashArray&& RemovedBuffers);

	/// Manage Blobber update
	friend class TextureGraphInsightBlobberObserver;
	void							BlobberHashesUpdated(TextureGraphInsightBlobberObserver::HashArray&& AddedHashes, TextureGraphInsightBlobberObserver::HashArray&& RemappedHashes);

	friend class TextureGraphInsightSchedulerObserver;
	/// Protected interface of emitters called by the scheduler observer
	void							UpdateIdle();
	void							BatchAdded(JobBatchPtr Batch);
	void							BatchDone(JobBatchPtr Batch);
	void							BatchJobsDone(JobBatchPtr Batch);

	/// manage creation and update of the Batch job records
	BatchRecord						MakeBatchRecord(RecordID RecId, JobBatchPtr Batch, bool isDone = true);
	void							UpdateBatchRecord(JobBatchPtr Batch, BatchRecord& record);
	JobRecord						MakeJobRecord(JobPtrW job, uint32 idx, int32 phaseIdx = 0);
	void							UpdateJobRecord(JobPtrW job, JobRecord& record, double batchBeginTime_ms, double batchEndTime_ms);


	/// manage creation and update of the BlobObj records
	RecordID						FindOrCreateBlobRecordFromHash(HashType hash);
	RecordID						RemapBlobHash(HashType oldHash, HashType newHash);
	RecordID						FindOrCreateBlobRecord(BlobPtr BlobObj, RecordID sourceID = RecordID());
	BlobRecord						MakeBlobRecord(RecordID RecId, BlobPtr BlobObj, RecordID sourceID = RecordID());
	void							EditTiledBlobRecord(BlobRecord& rd, const RecordIDTiles& subTiles);
	void							UpdateBlobRecord(RecordID RecId, BlobPtr BlobObj);
	RecordIDArray					NewBlobsToNotify;
	RecordIDArray					MappedBlobsToNotify;
	void							EmitOnNewBlob();
	void							EmitOnMappedBlob();

	/// manage creation and update of the MixObj records
	RecordID						AssociateBatchToMix(RecordID Batch, MixPtr MixObj);
	RecordID						FindOrCreateMixRecord(MixPtr MixObj);

	/// manage creation and update of the buffer records
	RecordIDArray					NewBuffersToNotify;

	/// The session record, collecting all the VALUES
	/// A session record is renewed for each engine
	SessionRecordPtr				Record;
	std::mutex						RecordMutex;

	/// Keep all the pointers to "real" TextureGraph Engine objects in the live cache
	/// A session live cache is renewed for each engine
	std::shared_ptr<LiveCache>		Cache;

	int32							EngineID = -1;
public:

	TextureGraphInsightSession();
    ~TextureGraphInsightSession();

	FORCEINLINE SessionRecord&		GetRecord() { std::lock_guard<std::mutex> Lock(RecordMutex); return (*Record); }
	FORCEINLINE LiveCache&			GetCache() { std::lock_guard<std::mutex> Lock(RecordMutex); return (*Cache); }

	/// Send the specified record to be inspected
	void							SendToInspector(RecordID RecId) const;

	/// Notifier subscription
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEngineReset, int32);
	FOnEngineReset& OnEngineReset()		{ return OnEngineResetEvent; }

	DECLARE_MULTICAST_DELEGATE(FOnUpdateIdle);
	FOnUpdateIdle& OnUpdateIdle()		{ return OnUpdateIdleEvent; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRecordIDArray, const RecordIDArray&);

	FOnRecordIDArray& OnDeviceBufferAdded()		{ return OnDeviceBufferAddedEvent; }
	FOnRecordIDArray& OnDeviceBufferRemoved()	{ return OnDeviceBufferRemovedEvent; }
	FOnRecordIDArray& OnBlobAdded()				{ return OnBlobAddedEvent; }
	FOnRecordIDArray& OnBlobMapped()			{ return OnBlobMappedEvent; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRecordID, RecordID);

	FOnRecordID& OnBatchAdded()			{ return OnBatchAddedEvent; }
	FOnRecordID& OnBatchDone()			{ return OnBatchDoneEvent; }
	FOnRecordID& OnBatchJobsDone()		{ return OnBatchJobsDoneEvent; }

	FOnRecordID& OnMixAdded()			{ return OnMixAddedEvent; }
	FOnRecordID& OnMixUpdated()			{ return OnMixUpdatedEvent; }
	
	FOnRecordID& OnBatchInspected()		{ return OnBatchInspectedEvent; }
	FOnRecordID& OnJobInspected()		{ return OnJobInspectedEvent; }
	FOnRecordID& OnBlobInspected()		{ return OnBlobInspectedEvent; }
	FOnRecordID& OnBufferInspected()	{ return OnBufferInspectedEvent; }


	// Trigger Replay of Batch and jobs
	// this call is async and returns if the scheduling of the replay is successful or not
	bool							ReplayBatch(RecordID batchRecordID, bool captureRenderDoc);
	bool							ReplayJob(RecordID jobRecordID, bool captureRenderDoc);

protected:
	FOnEngineReset		OnEngineResetEvent;
	FOnUpdateIdle		OnUpdateIdleEvent;

	FOnRecordIDArray	OnDeviceBufferAddedEvent;
	FOnRecordIDArray	OnDeviceBufferRemovedEvent;

	FOnRecordIDArray	OnBlobAddedEvent;
	FOnRecordIDArray	OnBlobMappedEvent;

	FOnRecordID			OnBatchAddedEvent;
	FOnRecordID			OnBatchDoneEvent;
	FOnRecordID			OnBatchJobsDoneEvent;

	FOnRecordID			OnMixAddedEvent;
	FOnRecordID			OnMixUpdatedEvent;

	FOnRecordID			OnBatchInspectedEvent;
	FOnRecordID			OnJobInspectedEvent;
	FOnRecordID			OnBlobInspectedEvent;
	FOnRecordID			OnBufferInspectedEvent;
};

