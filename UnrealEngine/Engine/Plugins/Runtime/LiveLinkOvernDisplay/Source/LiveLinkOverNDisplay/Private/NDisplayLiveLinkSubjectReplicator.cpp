// Copyright Epic Games, Inc. All Rights Reserved.

#include "NDisplayLiveLinkSubjectReplicator.h"


#include "Cluster/IDisplayClusterClusterManager.h"
#include "Features/IModularFeatures.h"
#include "IDisplayCluster.h"
#include "ILiveLinkClient.h"
#include "LiveLinkSubjectSettings.h"
#include "Misc/CoreDelegates.h"
#include "NDisplaySlaveVirtualSubject.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"


DEFINE_LOG_CATEGORY_STATIC(LogNDisplayLiveLinkSubjectReplicator, Log, All);



FNDisplayLiveLinkSubjectReplicator::~FNDisplayLiveLinkSubjectReplicator()
{
	Deactivate();
	
	if (LiveLinkClient)
	{
		LiveLinkClient->OnLiveLinkTicked().RemoveAll(this);
		LiveLinkClient->OnLiveLinkSourceRemoved().RemoveAll(this);
		LiveLinkClient->RemoveSource(LiveLinkSourceGuid);
	}

	//Cleanup our virtual source from LiveLink
	TrackedSubjects.Empty();

	IModularFeatures::Get().OnModularFeatureUnregistered().RemoveAll(this);
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	FCoreDelegates::OnBeginFrame.RemoveAll(this);
}

bool FNDisplayLiveLinkSubjectReplicator::IsActive() const
{
	return LiveLinkClient != nullptr;
}

FString FNDisplayLiveLinkSubjectReplicator::GetSyncId() const
{
	static const FString SyncId = TEXT("NDisplayLiveLinkSyncObject");
	return SyncId;
}

FString FNDisplayLiveLinkSubjectReplicator::SerializeToString() const
{
	//Serialization is done on the master. Payload was created on LiveLinkTicked callback
	FScopeLock Lock(&PayloadCriticalSection);
	const FString ThisFramePayload = LiveLinkPayload;
	return ThisFramePayload;
}

bool FNDisplayLiveLinkSubjectReplicator::DeserializeFromString(const FString& Str)
{
	//Deserialization is done on each slave
	TArray<uint8> TempBytes;
	TempBytes.AddUninitialized(Str.Len());
	StringToBytes(Str, TempBytes.GetData(), Str.Len());

	FMemoryReader Ar(TempBytes);
	OnDataSynchronization(Ar);

	return true;
}

void FNDisplayLiveLinkSubjectReplicator::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (const UNDisplaySlaveVirtualSubject* Subject : TrackedSubjects)
	{
		Collector.AddReferencedObject(Subject);
	}
}

void FNDisplayLiveLinkSubjectReplicator::Initialize()
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		//Cache LiveLink client if it was available and listen for it being unregistered.
		LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		IModularFeatures::Get().OnModularFeatureUnregistered().AddRaw(this, &FNDisplayLiveLinkSubjectReplicator::OnModularFeatureRemoved);

		if (IDisplayCluster::IsAvailable())
		{
			if (IDisplayCluster::Get().GetClusterMgr()->IsPrimary())
			{
				//If we're a master, listen to LiveLinkTicked callback to bundle up the SyncObject. Subjects frames will have been processed at that point.
				LiveLinkClient->OnLiveLinkTicked().AddRaw(this, &FNDisplayLiveLinkSubjectReplicator::OnLiveLinkTicked);
			}
			else if (IDisplayCluster::Get().GetClusterMgr()->IsSecondary())
			{
				//If we're a slave, listen for new subject and disables them if we're tracking that subject
				FCoreDelegates::OnBeginFrame.AddRaw(this, &FNDisplayLiveLinkSubjectReplicator::OnEngineBeginFrame);

				//Used to reinitialize ourself if we are removed from livelink
				LiveLinkClient->OnLiveLinkSourceRemoved().AddRaw(this, &FNDisplayLiveLinkSubjectReplicator::OnLiveLinkSourceRemoved);

				ReInitializeVirtualSource();
			}
		}
		else
		{
			UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Error, TEXT("Can't initialize LiveLink Subject Replicator for nDisplay because nDisplay is not available."));
		}
	}
	else
	{
		UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Error, TEXT("Can't initialize LiveLink Subject Replicator for nDisplay because LiveLink is not available."));
	}
}

void FNDisplayLiveLinkSubjectReplicator::Activate()
{
	UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Log, TEXT("Registering sync object."));

	//Register SyncObject on PreTick to have this behavior

	//Order of operation will be
	//1. When LiveLink creates snapshots for the frame, Master will create the sync object payload
	//2. On PreTick, the SyncObject will be synchronized between cluster machines
	//3. On object processing, Slaves will push received data for each subject to have the same data then Master
	if (IDisplayCluster::IsAvailable())
	{
		IDisplayClusterClusterManager* ClusterManager = IDisplayCluster::Get().GetClusterMgr();
		ClusterManager->RegisterSyncObject(this, EDisplayClusterSyncGroup::PreTick);
	}
	else
	{
		UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Warning, TEXT("Can't activate LiveLink Subject Replicator for nDisplay because nDisplay is not available."));
	}
}

void FNDisplayLiveLinkSubjectReplicator::Deactivate()
{
	UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Log, TEXT("Unregistering sync object."));
	
	if (IDisplayCluster::IsAvailable())
	{
		if (IDisplayClusterClusterManager* ClusterManager = IDisplayCluster::Get().GetClusterMgr())
		{
			ClusterManager->UnregisterSyncObject(this);
		}
	}
}

void FNDisplayLiveLinkSubjectReplicator::OnLiveLinkTicked()
{
	TArray<uint8> TempBytes;
	FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);
	OnDataSynchronization(Ar);

	FScopeLock Lock(&PayloadCriticalSection);
	LiveLinkPayload = BytesToString(TempBytes.GetData(), TempBytes.Num());
}

void FNDisplayLiveLinkSubjectReplicator::OnEngineBeginFrame()
{
	//Each frame, disable all subjects. When we are enabled, we are replicating ALL subjects from the master to each slave.
	//Only our replicated VirtualSubjects should be enabled
	if (LiveLinkClient)
	{
		check(IDisplayCluster::Get().GetClusterMgr()->IsSecondary());

		const bool bIncludeDisabled = false;
		const bool bIncludeVirtual = true;
		const TArray<FLiveLinkSubjectKey> Subjects = LiveLinkClient->GetSubjects(bIncludeDisabled, bIncludeVirtual);
		for (const FLiveLinkSubjectKey& Subject : Subjects)
		{
			if (Subject.Source != LiveLinkSourceGuid)
			{
				LiveLinkClient->SetSubjectEnabled(Subject, false);
			}
		}
		
		//Make sure all our replicated subjects are enabled
		for (const UNDisplaySlaveVirtualSubject* OurSubject : TrackedSubjects)
		{
			LiveLinkClient->SetSubjectEnabled(OurSubject->GetSubjectKey(), true);
		}
	}
}

void FNDisplayLiveLinkSubjectReplicator::OnDataSynchronization(FArchive& Ar)
{
	//Master is saving, slaves are loading
	const bool bIsSaving = Ar.IsSaving();
	bool bContinue = bIsSaving;

	EFrameType FrameType = EFrameType::DataOnly;

	//Fill in current tracked subject list. If a subject is not processed this frame, we remove it
	TArray<FLiveLinkSubjectKey, TInlineAllocator<16>> LastFrameSubjectKeys;
	for (const UNDisplaySlaveVirtualSubject* Subject : TrackedSubjects)
	{
		LastFrameSubjectKeys.Add(Subject->GetAssociatedSubjectKey());
	}

	if (bIsSaving)
	{
		if (LiveLinkClient)
		{
			/**
			 * We're the master, and serializing data out to clients:
			 *	- Iterate over all of the snapshots.
			 *		- Determine the type of frame this is (new static data, new subject, data only).
			 *		- Serialize a True continuation byte.
			 *		- Serialize the frame.
			 *	- Once all snapshots have been added, serialize a False continuation byte.
			 */
			{
				//Get all subjects except the ones that are disabled
				//Since we are replicating using a VirtualSubject, slaves will have to remove any VirtualSubject being replicated
				const bool bIncludeDisabled = false;
				const bool bIncludeVirtuals = true;
				TArray<FLiveLinkSubjectKey> ThisFramesSubjects = LiveLinkClient->GetSubjects(bIncludeDisabled, bIncludeVirtuals);
				for (FLiveLinkSubjectKey& SubjectKey : ThisFramesSubjects)
				{
					const FName SubjectName = SubjectKey.SubjectName;
					TSubclassOf<ULiveLinkRole> SubjectRole = LiveLinkClient->GetSubjectRole_AnyThread(SubjectKey);

					// Check if the subject's source is pending kill. If it is, the subject itself will soon be killed and we can skip evaluating it.

					TArray<FGuid> ValidSources = LiveLinkClient->GetSources(false);

					if (bIncludeVirtuals)
					{
						ValidSources += LiveLinkClient->GetVirtualSources(false);
					}

					const bool bIsSourceValid = ValidSources.ContainsByPredicate([SubjectKey](const FGuid& Other) { return Other == SubjectKey.Source; });

					if (!bIsSourceValid)
					{
						UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Verbose, TEXT("Master could not evaluate Subject '%s'. Its source is marked as pending kill."), *SubjectName.ToString());
						continue;
					}

					FLiveLinkSubjectFrameData SubjectFrameData;
					if (LiveLinkClient->EvaluateFrame_AnyThread(SubjectName, SubjectRole, SubjectFrameData))
					{
						if (UNDisplaySlaveVirtualSubject** FoundSujectDataPtr = TrackedSubjects.FindByPredicate(
							[SubjectKey](const UNDisplaySlaveVirtualSubject* Other) { return Other->GetAssociatedSubjectKey().SubjectName == SubjectKey.SubjectName; }))
						{
							UNDisplaySlaveVirtualSubject* FoundSubjectData = *FoundSujectDataPtr;
							if (SubjectFrameData.StaticData != FoundSubjectData->GetStaticData() 
								|| FoundSubjectData->GetAssociatedSubjectKey() != SubjectKey 
								|| FoundSubjectData->GetRole() != SubjectRole)
							{
								FrameType = EFrameType::UpdatedSubject;
							}
							else
							{
								FrameType = EFrameType::DataOnly;
							}
						}
						else
						{
							FrameType = EFrameType::NewSubject;
						}

						//This subject was processed
						LastFrameSubjectKeys.Remove(SubjectKey);

						Ar << bContinue;

						HandleFrame(Ar, FrameType, SubjectKey, SubjectRole, SubjectFrameData);
					}
					else
					{
						UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Verbose, TEXT("Master could not evaluate Subject '%s'. It could have starved."), *SubjectName.ToString());
					}
				}
			}
		}

		//We're done processing subjects, mark the frame as done 
		bContinue = false;
		Ar << bContinue;
	}
	else
	{
		/**
		 * We're a slave, and serializing data from the master:
		 *	- Serialize the continuation byte
		 *		- If False, we're done.
		 *		- If True:
		 *			- Serialize the frame.
		 *			- Based on frame type, do required process.
		 *			- Repeat.
		 */

		TSubclassOf<ULiveLinkRole> SubjectRole;
		FLiveLinkSubjectKey SubjectKey;
		FLiveLinkSubjectFrameData SubjectFrameData;

		while (true)
		{
			Ar << bContinue;
			if (bContinue)
			{
				//Process frame data and fill this subject's data
				HandleFrame(Ar, FrameType, SubjectKey, SubjectRole, SubjectFrameData);
				if (!SubjectKey.SubjectName.IsNone())
				{
					ProcessLiveLinkData_Slave(FrameType, SubjectKey, SubjectFrameData.FrameData);

					LastFrameSubjectKeys.RemoveSingleSwap(SubjectKey);
				}
			}
			else
			{
				break;
			}
		}
	}

	// Cleanup missing subjects. Slaves will need to remove VirtualSubjects that were created
	if (LiveLinkClient)
	{
		for (const FLiveLinkSubjectKey& ItemSubjectKey : LastFrameSubjectKeys)
		{
			UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Verbose, TEXT("Subject '%s' was not processed. Removing it from our list."), *ItemSubjectKey.SubjectName.ToString());

			if (!bIsSaving)
			{
				const FLiveLinkSubjectKey ReplicatedSubjectKey(LiveLinkSourceGuid, ItemSubjectKey.SubjectName);
				LiveLinkClient->RemoveVirtualSubject(ReplicatedSubjectKey);
			}

			const int32 FoundIndex = TrackedSubjects.IndexOfByPredicate([ItemSubjectKey](const UNDisplaySlaveVirtualSubject* Other) { return Other->GetAssociatedSubjectKey() == ItemSubjectKey; });
			if (TrackedSubjects.IsValidIndex(FoundIndex))
			{
				TrackedSubjects.RemoveAtSwap(FoundIndex);
			}
		}
	}
	else
	{
		TrackedSubjects.Reset();
	}
}

void FNDisplayLiveLinkSubjectReplicator::HandleFrame(FArchive& Ar, EFrameType& FrameType, FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole>& SubjectRole, FLiveLinkSubjectFrameData& SubjectFrame)
{
	//Archive always start with the type and subject identifier
	Ar << FrameType;
	Ar << SubjectKey;

	//If it's a new subject / updated one, process static data and role
	if (FrameType == EFrameType::NewSubject || FrameType == EFrameType::UpdatedSubject)
	{
		HandleNewSubject(Ar, SubjectKey, SubjectRole, SubjectFrame);
	}

	//End archive with Frame data
	Ar << SubjectFrame.FrameData;
	
	UE_LOG(LogNDisplayLiveLinkSubjectReplicator, VeryVerbose, TEXT("HandleFrame SubjectName=%s FrameType=%d"), *SubjectKey.SubjectName.ToString(), static_cast<uint8>(FrameType));
}

void FNDisplayLiveLinkSubjectReplicator::HandleNewSubject(FArchive& Ar, FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole>& SubjectRole, FLiveLinkSubjectFrameData& SubjectFrame)
{
	//New or updated subjects will have static data 
	Ar << SubjectFrame.StaticData;

	UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Verbose, TEXT("HandleNewSubject SubjectName=%s"), *SubjectKey.SubjectName.ToString());

	//Verify if we're already tracking that subject
	UNDisplaySlaveVirtualSubject* TrackedSubject = nullptr;
	if (UNDisplaySlaveVirtualSubject** FoundSujectDataPtr = TrackedSubjects.FindByPredicate(
		[SubjectKey](const UNDisplaySlaveVirtualSubject* Other) { return Other->GetAssociatedSubjectKey().SubjectName == SubjectKey.SubjectName; }))
	{
		TrackedSubject = *FoundSujectDataPtr;
		UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Verbose, TEXT("HandleNewSubject SubjectName=%s but was already tracked. Updating its info."), *SubjectKey.SubjectName.ToString());
	}

	if (Ar.IsSaving())
	{
		FString RoleClassName = SubjectRole->GetPathName();
		Ar << RoleClassName;

		//If we're not tracking that subject yet, create it
		if (TrackedSubject == nullptr)
		{
			TrackedSubject = NewObject<UNDisplaySlaveVirtualSubject>(GetTransientPackage());
			TrackedSubjects.Add(TrackedSubject);
		}
		
		//When handling new subject, always setup role and static data
		TrackedSubject->SetTrackedSubjectInfo(SubjectKey, SubjectRole);
		TrackedSubject->GetStaticData().InitializeWith(SubjectFrame.StaticData);
	}
	else
	{
		FString RoleClassName;
		Ar << RoleClassName;
		SubjectRole = StaticLoadClass(UObject::StaticClass(), NULL, *RoleClassName, NULL, LOAD_None, NULL);
		check(SubjectRole);

		//On Slaves, create the virtual subject directly in LiveLink so it's part of the system
		ULiveLinkVirtualSubject* ReplicatedSubject = Cast<ULiveLinkVirtualSubject>(LiveLinkClient->GetSubjectSettings(SubjectKey));

		//Always disable the subject we're replicating
		LiveLinkClient->SetSubjectEnabled(SubjectKey, false);

		//If we're not tracking that subject yet, create it
		if (TrackedSubject == nullptr)
		{
			const FLiveLinkSubjectKey ReplicatedSubjectKey(LiveLinkSourceGuid, SubjectKey.SubjectName);
			if (LiveLinkClient->AddVirtualSubject(ReplicatedSubjectKey, UNDisplaySlaveVirtualSubject::StaticClass()))
			{
				//Retrieve the newly created subject to add it to our tracking array
				const bool bIncludeDisabled = false;
				const bool bIncludeVirtual = true;
				for (const FLiveLinkSubjectKey& FoundSubjectKey : LiveLinkClient->GetSubjects(bIncludeDisabled, bIncludeVirtual))
				{
					if (FoundSubjectKey.SubjectName == SubjectKey.SubjectName)
					{
						//If this subject has the same subject name, verify if it's the VirtualOne. Settings object will return the VirtualSubject directly
						if (UNDisplaySlaveVirtualSubject* NewlyCreatedSubject = Cast<UNDisplaySlaveVirtualSubject>(LiveLinkClient->GetSubjectSettings(FoundSubjectKey)))
						{
							TrackedSubject = NewlyCreatedSubject;
							TrackedSubjects.Add(NewlyCreatedSubject);
						}
					}
				}

				checkf(TrackedSubject, TEXT("TrackedSubject '%s' was added but could not be found afterwards."), *SubjectKey.SubjectName.ToString());
			}
			else
			{
				UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Warning, TEXT("Could not create new subject '%s'. It won't be tracked correctly on this node."), *SubjectKey.SubjectName.ToString());
			}
		}

		if (TrackedSubject)
		{
			//VirtualSubjects need translators to be able to be evaluated in different output format. Special treatment required for virtual subjects since they won't/can't exist in the client
			if (ReplicatedSubject)
			{
				const TArray<ULiveLinkFrameTranslator*>& Translators = ReplicatedSubject->GetTranslators();
				TrackedSubject->UpdateTranslators(Translators);
			}
			else
			{
				if (ULiveLinkSubjectSettings* SubjectSetting = Cast<ULiveLinkSubjectSettings>(LiveLinkClient->GetSubjectSettings(SubjectKey)))
				{
					TrackedSubject->UpdateTranslators(SubjectSetting->Translators);
				}
				else
				{
					UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Warning, TEXT("Replicating subject '%s' but could not find its settings. Translators won't work."), *SubjectKey.SubjectName.ToString());
				}
			}

			//When handling new subject, always setup role and static data
			TrackedSubject->SetTrackedSubjectInfo(SubjectKey, SubjectRole);
			TrackedSubject->GetStaticData().InitializeWith(SubjectFrame.StaticData);
		}
	}
}

void FNDisplayLiveLinkSubjectReplicator::ProcessLiveLinkData_Slave(EFrameType FrameType, const FLiveLinkSubjectKey& SubjectKey, FLiveLinkFrameDataStruct& FrameData)
{
	//Find associated tracked subject data to broadcast role and static data. Should always be found
	UNDisplaySlaveVirtualSubject** FoundTrackedSubjectPtr = TrackedSubjects.FindByPredicate([SubjectKey](const UNDisplaySlaveVirtualSubject* Other) { return Other->GetAssociatedSubjectKey() == SubjectKey; });
	check(FoundTrackedSubjectPtr);
	UNDisplaySlaveVirtualSubject* FoundTrackedSubject = *FoundTrackedSubjectPtr;
	
	FrameData.GetBaseData()->WorldTime = FPlatformTime::Seconds();
	
	UE_LOG(LogNDisplayLiveLinkSubjectReplicator, VeryVerbose, TEXT("ProcessLiveLinkData SubjectUpdated=%s UpdateType=%d Timecode=%s")
		, *(SubjectKey.SubjectName.ToString()), static_cast<uint8>(FrameType)
		, *(FTimecode::FromFrameNumber(FrameData.GetBaseData()->MetaData.SceneTime.Time.FrameNumber, FrameData.GetBaseData()->MetaData.SceneTime.Rate, false).ToString()));
	
	FoundTrackedSubject->UpdateFrameData(MoveTemp(FrameData));
}

void FNDisplayLiveLinkSubjectReplicator::ReInitializeVirtualSource()
{
	//if we are rebuilding our source, let go of any subject we are currently tracking
	for (const UNDisplaySlaveVirtualSubject* Subject : TrackedSubjects)
	{
		LiveLinkClient->RemoveVirtualSubject(Subject->GetSubjectKey());
	}

	//Start source with a clean slate
	TrackedSubjects.Empty();

	//Create our Source to hold all VirtualSubject we'll create
	LiveLinkSourceGuid = LiveLinkClient->AddVirtualSubjectSource(TEXT("nDisplaySubjectReplicator"));
	if (!LiveLinkSourceGuid.IsValid())
	{
		UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Error, TEXT("Could not add VirtualSubject Source for nDisplay replication."));
	}
}

void FNDisplayLiveLinkSubjectReplicator::OnLiveLinkSourceRemoved(FGuid SourceGuid)
{
	if (LiveLinkSourceGuid.IsValid() && LiveLinkSourceGuid == SourceGuid)
	{
		UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Verbose, TEXT("nDisplay LiveLink Source was removed. Reinitializing ourself."));
		ReInitializeVirtualSource();
	}
}

void FNDisplayLiveLinkSubjectReplicator::OnModularFeatureRemoved(const FName& Type, IModularFeature* ModularFeature)
{
	//If LiveLink gets unregistered, invalidate our cached client which make the replicator do nothing
	//To support hot reload, we would need to register for the feature to be loaded again
	if (Type == ILiveLinkClient::ModularFeatureName)
	{
		UE_LOG(LogNDisplayLiveLinkSubjectReplicator, Verbose, TEXT("LiveLink feature was removed."));
		LiveLinkClient = nullptr;
	}
}
