// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/IDisplayClusterClusterSyncObject.h"
#include "UObject/GCObject.h"

#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"


class ILiveLinkClient;
class IModularFeature;
class UNDisplaySlaveVirtualSubject;


/**
 * Classes used to replicates data to be used for each frame for each enabled subjects on the Master machines.
 * Slaves will use that replicated data to use the same thing on each machines
 */
class LIVELINKOVERNDISPLAY_API FNDisplayLiveLinkSubjectReplicator : public IDisplayClusterClusterSyncObject, public FGCObject
{
private:
	enum class EFrameType : uint8
	{
		DataOnly,
		NewSubject,			//A new subject was sent this frame.
		UpdatedSubject,		//The subject's static data or role was updated this frame.
	};

public:

	FNDisplayLiveLinkSubjectReplicator() = default;
	virtual ~FNDisplayLiveLinkSubjectReplicator();

	/** Move only type since we're owning LiveLink base structs */
	FNDisplayLiveLinkSubjectReplicator(const FNDisplayLiveLinkSubjectReplicator&) = delete;
	FNDisplayLiveLinkSubjectReplicator& operator=(const FNDisplayLiveLinkSubjectReplicator&) = delete;

	//~ Begin IDisplayClusterClusterSyncObject interface
	virtual bool IsActive() const override;
	virtual FString GetSyncId() const override;
	virtual bool IsDirty() const override { return true; };
	virtual void ClearDirty() override {};
	virtual FString SerializeToString() const override;
	virtual bool DeserializeFromString(const FString& Ar) override;
	//~ End IDisplayClusterClusterSyncObject interface

	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FNDisplayLiveLinkSubjectReplicator");
	}
	//~ End FGCObject interface

	/** Initializes all required callbacks and LiveLink source */
	void Initialize();
	
	/** Registers ourself as sync object in nDisplay */
	void Activate();
	
	/** Unregisters ourself as sync object from nDisplay */
	void Deactivate();

protected:

	/**
     * Callback to be notified when LiveLinkClient was ticked so SyncObject can be serialized
     * @note Only called on Master
     */
	void OnLiveLinkTicked();

	/**
	 * Hook to beginning of frames to make sure our replicated virtual subjects are the ones enabled
	 * @note Only called on Slave
	 */
	void OnEngineBeginFrame();

	/** Synchronization point when object is going to be synchronized across nDisplay cluster */
	void OnDataSynchronization(FArchive& Ar);

	/** Process new subject data when serializing SyncObject */
	void HandleNewSubject(FArchive& Ar, FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole>& SubjectRole, FLiveLinkSubjectFrameData& SubjectFrame);

	/** Process a frame for a specific subject */
	void HandleFrame(FArchive& Ar, EFrameType& FrameType, FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole>& SubjectRole, FLiveLinkSubjectFrameData& SubjectFrame);

	/** Only used on slaves, handles this frame data for a subject. If a new subject is handled, the associated VirtualSubject will be created */
	void ProcessLiveLinkData_Slave(EFrameType FrameType, const FLiveLinkSubjectKey& SubjectKey, FLiveLinkFrameDataStruct& FrameData);

	/** For slave only, remove tracked subjets, add ourself as a source  */
	void ReInitializeVirtualSource();

	/** If our source is removed (could happen if a new preset is applied), reinitialize ourself to stay awake */
	void OnLiveLinkSourceRemoved(FGuid SourceGuid);

	/** Listen for modular feature removed in case LiveLink gets unloaded */
	void OnModularFeatureRemoved(const FName& Type , IModularFeature* ModularFeature);

private:

	/** Cached LiveLinkClient when modular feature is registered */
	ILiveLinkClient* LiveLinkClient = nullptr;

	//CriticalSection to protect the payload. SerializeToString can be called from other threads than Main Thread
	mutable FCriticalSection PayloadCriticalSection;

	/** SyncObject uses string to pass around data. This represents binary LiveLinkSubject information converted to string */
	FString LiveLinkPayload;

	/** List of Subjects that we are replicating across cluster. On Slaves, it will actually be added to the LiveLink subject's list */
	TArray<UNDisplaySlaveVirtualSubject*> TrackedSubjects;

	/** Guid associated to our Virtual Subject Source */
	FGuid LiveLinkSourceGuid;
};
