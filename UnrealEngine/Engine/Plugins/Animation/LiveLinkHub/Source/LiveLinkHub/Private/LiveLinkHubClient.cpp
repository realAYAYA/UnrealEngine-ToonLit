// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubClient.h"

#include "Async/TaskGraphInterfaces.h"
#include "Engine/Engine.h"
#include "LiveLinkClient.h"
#include "LiveLinkHub.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkRoleTrait.h"
#include "LiveLinkSourceCollection.h"
#include "LiveLinkSettings.h"
#include "LiveLinkSubject.h"
#include "LiveLinkSubjectSettings.h"
#include "LiveLinkTimedDataInput.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Recording/LiveLinkPlaybackSource.h"
#include "Recording/LiveLinkPlaybackSubject.h"
#include "Stats/Stats2.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DECLARE_STATS_GROUP(TEXT("Live Link Hub"), STATGROUP_LiveLinkHub, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("LiveLinkHub - Push StaticData"), STAT_LiveLinkHub_PushStaticData, STATGROUP_LiveLinkHub);
DECLARE_CYCLE_STAT(TEXT("LiveLinkHub - Push FrameData"), STAT_LiveLinkHub_PushFrameData, STATGROUP_LiveLinkHub);

#define LOCTEXT_NAMESPACE "LiveLinkHub.LiveLinkHubClient"

FLiveLinkHubClient::FLiveLinkHubClient(TSharedPtr<ILiveLinkHub> InLiveLinkHub)
	: FLiveLinkClient()
	, LiveLinkHub(MoveTemp(InLiveLinkHub))
{
}

FLiveLinkHubClient::~FLiveLinkHubClient()
{
}

const FLiveLinkStaticDataStruct* FLiveLinkHubClient::GetSubjectStaticData(const FLiveLinkSubjectKey& InSubjectKey)
{
	FScopeLock Lock(&CollectionAccessCriticalSection);
	if (const FLiveLinkCollectionSubjectItem* SubjectItem = Collection->FindSubject(InSubjectKey))
	{
		if (FLiveLinkSubject* LiveLinkSubject = SubjectItem->GetLiveSubject())
		{
			return &LiveLinkSubject->GetStaticData();
		}
	}

	return nullptr;
}

bool FLiveLinkHubClient::CreateSource(const FLiveLinkSourcePreset& InSourcePreset)
{
	// Create fake sources if we're in playback mode so we don't process livelink data while we're playing a recording.
	if (LiveLinkHub.Pin()->IsInPlayback())
	{
		return CreatePlaybackSource(InSourcePreset);
	}
	else
	{
		return FLiveLinkClient::CreateSource(InSourcePreset);
	}
}

bool FLiveLinkHubClient::CreatePlaybackSource(const FLiveLinkSourcePreset& InSourcePreset)
{
	check(Collection);

	if (InSourcePreset.Settings == nullptr)
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Source Failure: The settings are not defined."));
		return false;
	}

	if (InSourcePreset.Guid == FLiveLinkSourceCollection::DefaultVirtualSubjectGuid)
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Source Failure: Can't create default subject source. It will be created automatically."));
		return false;
	}

	if (!InSourcePreset.Guid.IsValid())
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Source Failure: The guid is invalid."));
		return false;
	}

	{
		FScopeLock Lock(&CollectionAccessCriticalSection);
		if (Collection->FindSource(InSourcePreset.Guid) != nullptr)
		{
			UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Source Failure: The guid already exist."));
			return false;
		}
	}

	ULiveLinkSourceSettings* Setting = nullptr;
	TSharedPtr<ILiveLinkSource> Source;
	FLiveLinkCollectionSourceItem Data;
	Data.Guid = InSourcePreset.Guid;

	// subject source have a special settings class. We can differentiate them using this
	if (InSourcePreset.Settings->GetClass()->GetName() == TEXT("LiveLinkVirtualSubjectSourceSettings"))
	{
		Source = MakeShared<FLiveLinkPlaybackSource>();
		Data.bIsVirtualSource = true;
	}
	else
	{
		Source = MakeShared<FLiveLinkPlaybackSource>();
		Data.TimedData = MakeShared<FLiveLinkTimedDataInput>(this, InSourcePreset.Guid);
	}

	Data.Source = Source;

	// In case a source has changed its source settings class, instead of duplicating, create the right one and copy previous properties
	UClass* SourceSettingsClass = Source->GetSettingsClass().Get();
	if (SourceSettingsClass && SourceSettingsClass != InSourcePreset.Settings->GetClass())
	{
		Setting = NewObject<ULiveLinkSourceSettings>(GetTransientPackage(), SourceSettingsClass);
		UEngine::CopyPropertiesForUnrelatedObjects(InSourcePreset.Settings, Setting);
		Data.Setting = Setting;
	}
	else
	{
		Setting = Data.Setting = DuplicateObject<ULiveLinkSourceSettings>(InSourcePreset.Settings, GetTransientPackage());
	}

	{
		FScopeLock Lock(&CollectionAccessCriticalSection);
		Collection->AddSource(MoveTemp(Data));
	}
	Source->ReceiveClient(this, InSourcePreset.Guid);
	Source->InitializeSettings(Setting);

	return true;
}

bool FLiveLinkHubClient::CreateSubject(const FLiveLinkSubjectPreset& InSubjectPreset)
{
	// Create fake subjects if we're in playback mode
	if (LiveLinkHub.Pin()->IsInPlayback())
	{
		return CreatePlaybackSubject(InSubjectPreset);
	}
	else
	{
		return FLiveLinkClient::CreateSubject(InSubjectPreset);
	}
}

bool FLiveLinkHubClient::CreatePlaybackSubject(const FLiveLinkSubjectPreset& InSubjectPreset)
{
	check(Collection);

	if (InSubjectPreset.Role.Get() == nullptr || InSubjectPreset.Role.Get() == ULiveLinkRole::StaticClass())
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Subject Failure: The role is not defined."));
		return false;
	}

	if (InSubjectPreset.Key.Source == FLiveLinkSourceCollection::DefaultVirtualSubjectGuid && InSubjectPreset.VirtualSubject == nullptr)
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Source Failure: Can't create an empty subject."));
		return false;
	}

	if (InSubjectPreset.Key.SubjectName.IsNone())
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Subject Failure: The subject name is invalid."));
		return false;
	}

	FLiveLinkCollectionSourceItem* SourceItem = Collection->FindSource(InSubjectPreset.Key.Source);
	if (SourceItem == nullptr || SourceItem->bPendingKill)
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Subject Failure: The source doesn't exist."));
		return false;
	}

	FLiveLinkCollectionSubjectItem* SubjectItem = Collection->FindSubject(InSubjectPreset.Key);
	if (SubjectItem != nullptr)
	{
		if (SubjectItem->bPendingKill)
		{
			FScopeLock Lock(&CollectionAccessCriticalSection);
			Collection->RemoveSubject(InSubjectPreset.Key);
		}
		else
		{
			UE_LOG(LogLiveLinkHub, Warning, TEXT("Create Subject Failure: The subject already exist."));
			return false;
		}
	}

	if (InSubjectPreset.VirtualSubject)
	{
		bool bEnabled = false;
		ULiveLinkVirtualSubject* VSubject = DuplicateObject<ULiveLinkVirtualSubject>(InSubjectPreset.VirtualSubject, GetTransientPackage());
		FLiveLinkCollectionSubjectItem VSubjectData(InSubjectPreset.Key, VSubject, bEnabled);
		VSubject->Initialize(VSubjectData.Key, VSubject->GetRole(), this);

		FScopeLock Lock(&CollectionAccessCriticalSection);
		Collection->AddSubject(MoveTemp(VSubjectData));
		Collection->SetSubjectEnabled(InSubjectPreset.Key, InSubjectPreset.bEnabled);
	}
	else
	{
		ULiveLinkSubjectSettings* SubjectSettings = nullptr;
		if (InSubjectPreset.Settings)
		{
			SubjectSettings = DuplicateObject<ULiveLinkSubjectSettings>(InSubjectPreset.Settings, GetTransientPackage());
		}
		else
		{
			SubjectSettings = NewObject<ULiveLinkSubjectSettings>();
		}

		bool bEnabled = false;
		FLiveLinkCollectionSubjectItem CollectionSubjectItem(InSubjectPreset.Key, MakeUnique<FLiveLinkPlaybackSubject>(SourceItem->TimedData), SubjectSettings, bEnabled);
		CollectionSubjectItem.GetLiveSubject()->Initialize(InSubjectPreset.Key, InSubjectPreset.Role.Get(), this);

		FScopeLock Lock(&CollectionAccessCriticalSection);
		Collection->AddSubject(MoveTemp(CollectionSubjectItem));
		Collection->SetSubjectEnabled(InSubjectPreset.Key, InSubjectPreset.bEnabled);
	}
	return true;
}

void FLiveLinkHubClient::PushSubjectStaticData_AnyThread(const FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& InStaticData)
{
	// Copied and adapted from FLiveLinkClient::PushSubjectStaticData_AnyThread
	SCOPE_CYCLE_COUNTER(STAT_LiveLinkHub_PushStaticData);

	check(Collection);

	// Check if subject should receive data

	if (!FLiveLinkRoleTrait::Validate(Role, InStaticData))
	{
		if (Role == nullptr)
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("Trying to add unsupported static data type with subject '%s'."), *SubjectKey.SubjectName.ToString());
		}
		else
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("Trying to add unsupported static data type to role '%s' with subject '%s'."), *Role->GetName(), *SubjectKey.SubjectName.ToString());
		}
		return;
	}

	bool bShouldLogIfInvalidStaticData = true;
	if (!Role.GetDefaultObject()->IsStaticDataValid(InStaticData, bShouldLogIfInvalidStaticData))
	{
		if (bShouldLogIfInvalidStaticData)
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("Trying to add static data that is not formatted properly to role '%s' with subject '%s'."), *Role->GetName(), *SubjectKey.SubjectName.ToString());
		}
		return;
	}

	const FLiveLinkCollectionSourceItem* SourceItem = nullptr;
	{
		FScopeLock Lock(&CollectionAccessCriticalSection);
		SourceItem = Collection->FindSource(SubjectKey.Source);
	}

	if (SourceItem == nullptr || SourceItem->bPendingKill)
	{
		return;
	}

	if (SourceItem->IsVirtualSource())
	{
		UE_LOG(LogLiveLinkHub, Error, TEXT("Trying to add frame data to the subject '%s'."), *Role->GetName(), *SubjectKey.SubjectName.ToString());
		return;
	}

	FLiveLinkSubject* LiveLinkSubject = nullptr;
	{
		FScopeLock Lock(&CollectionAccessCriticalSection);
		if (FLiveLinkCollectionSubjectItem* SubjectItem = Collection->FindSubject(SubjectKey))
		{
			if (!SubjectItem->bPendingKill)
			{
				LiveLinkSubject = SubjectItem->GetLiveSubject();

				if (LiveLinkSubject->GetRole() != Role)
				{
					UE_LOG(LogLiveLinkHub, Warning, TEXT("Subject '%s' of role '%s' is changing its role to '%s'. Current subject will be removed and a new one will be created"), *SubjectKey.SubjectName.ToString(), *LiveLinkSubject->GetRole().GetDefaultObject()->GetDisplayName().ToString(), *Role.GetDefaultObject()->GetDisplayName().ToString());

					Collection->RemoveSubject(SubjectKey);
					LiveLinkSubject = nullptr;
				}
				else
				{
					LiveLinkSubject->ClearFrames();
				}
			}
		}

	}
	if (LiveLinkSubject == nullptr)
	{
		const ULiveLinkSettings* LiveLinkSettings = GetDefault<ULiveLinkSettings>();
		const FLiveLinkRoleProjectSetting DefaultSetting = LiveLinkSettings->GetDefaultSettingForRole(Role.Get());

		// Setting class should always be valid
		ULiveLinkSubjectSettings* SubjectSettings = nullptr;
		{
			UClass* SettingClass = DefaultSetting.SettingClass.Get();
			if (SettingClass == nullptr)
			{
				SettingClass = ULiveLinkSubjectSettings::StaticClass();
			}

			SubjectSettings = NewObject<ULiveLinkSubjectSettings>(GetTransientPackage(), SettingClass);
			SubjectSettings->Role = Role;

			UClass* FrameInterpolationProcessorClass = DefaultSetting.FrameInterpolationProcessor.Get();
			if (FrameInterpolationProcessorClass != nullptr)
			{
				UClass* InterpolationRole = FrameInterpolationProcessorClass->GetDefaultObject<ULiveLinkFrameInterpolationProcessor>()->GetRole();
				if (Role->IsChildOf(InterpolationRole))
				{
					SubjectSettings->InterpolationProcessor = NewObject<ULiveLinkFrameInterpolationProcessor>(SubjectSettings, FrameInterpolationProcessorClass);
				}
				else
				{
					UE_LOG(LogLiveLinkHub, Warning, TEXT("The interpolator '%s' is not valid for the Role '%s'"), *FrameInterpolationProcessorClass->GetName(), *Role->GetName());
				}
			}
			else
			{
				// If no settings were found for a specific role, check if the default interpolator is compatible with the role
				UClass* FallbackInterpolationProcessorClass = LiveLinkSettings->FrameInterpolationProcessor.Get();
				if (FallbackInterpolationProcessorClass != nullptr)
				{
					UClass* InterpolationRole = FallbackInterpolationProcessorClass->GetDefaultObject<ULiveLinkFrameInterpolationProcessor>()->GetRole();
					if (Role->IsChildOf(InterpolationRole))
					{
						SubjectSettings->InterpolationProcessor = NewObject<ULiveLinkFrameInterpolationProcessor>(SubjectSettings, FallbackInterpolationProcessorClass);
					}
				}
			}

			for (TSubclassOf<ULiveLinkFramePreProcessor> PreProcessor : DefaultSetting.FramePreProcessors)
			{
				if (PreProcessor != nullptr)
				{
					UClass* PreProcessorRole = PreProcessor->GetDefaultObject<ULiveLinkFramePreProcessor>()->GetRole();
					if (Role->IsChildOf(PreProcessorRole))
					{
						SubjectSettings->PreProcessors.Add(NewObject<ULiveLinkFramePreProcessor>(SubjectSettings, PreProcessor.Get()));
					}
					else
					{
						UE_LOG(LogLiveLinkHub, Warning, TEXT("The pre processor '%s' is not valid for the Role '%s'"), *PreProcessor->GetName(), *Role->GetName());
					}
				}
			}
		}


		{
			FScopeLock Lock(&CollectionAccessCriticalSection);

			bool bEnabled = Collection->FindEnabledSubject(SubjectKey.SubjectName) == nullptr;
			FLiveLinkCollectionSubjectItem CollectionSubjectItem(SubjectKey, MakeUnique<FLiveLinkSubject>(SourceItem->TimedData), SubjectSettings, bEnabled);
			CollectionSubjectItem.GetLiveSubject()->Initialize(SubjectKey, Role.Get(), this);

			LiveLinkSubject = CollectionSubjectItem.GetLiveSubject();

			Collection->AddSubject(MoveTemp(CollectionSubjectItem));
			
			// Clear async flag on the newly created uobject to allow it to be garbage collected.
			SubjectSettings->AtomicallyClearInternalFlags(EInternalObjectFlags::Async);
		}
	}

	OnStaticDataReceivedDelegate_AnyThread.Broadcast(SubjectKey, Role, InStaticData);

	if (LiveLinkSubject)
	{
		// Dispatch it on the game thread since FLiveLinkSubject::SetStaticData asserts when called outside the game thread.
		// Note that SetStaticData may not be needed on the hub but we might need it for interpolation or preprocessing in the future.
		FFunctionGraphTask::CreateAndDispatchWhenReady([this, LiveLinkSubject, SubjectKey, Role, StaticData = MoveTemp(InStaticData)]() mutable
			{
				FScopeLock Lock(&CollectionAccessCriticalSection);
				// Validate live link subject hasn't changed
				if (FLiveLinkCollectionSubjectItem* SubjectItem = Collection->FindSubject(SubjectKey))
				{
					const FLiveLinkSubject* CurrentLiveLinkSubject = SubjectItem->GetLiveSubject();
					if (LiveLinkSubject == CurrentLiveLinkSubject)
					{
						LiveLinkSubject->SetStaticData(Role, MoveTemp(StaticData));
					}
				}
			}, TStatId(), nullptr, ENamedThreads::GameThread);
	}

}

void FLiveLinkHubClient::PushSubjectFrameData_AnyThread(const FLiveLinkSubjectKey& SubjectKey, FLiveLinkFrameDataStruct&& FrameData)
{
	SCOPE_CYCLE_COUNTER(STAT_LiveLinkHub_PushFrameData);

	if (const FLiveLinkCollectionSubjectItem* SubjectItem = Collection->FindSubject(SubjectKey))
	{
		if (FLiveLinkSubject* LiveSubject = SubjectItem->GetLiveSubject())
		{
			LiveSubject->SetLastPushTime(FApp::GetCurrentTime());
		}
	}

	OnFrameDataReceivedDelegate_AnyThread.Broadcast(SubjectKey, FrameData);
	BroadcastFrameDataUpdate(SubjectKey, FrameData);
}

FText FLiveLinkHubClient::GetSourceStatus(FGuid InEntryGuid) const
{
	if (LiveLinkHub.Pin()->IsInPlayback())
	{
		return LOCTEXT("PlaybackText", "Playback");
	}

	return FLiveLinkClient::GetSourceStatus(InEntryGuid);
}

bool FLiveLinkHubClient::IsSubjectValid(const FLiveLinkSubjectKey& InSubjectKey) const
{
	FScopeLock Lock(&CollectionAccessCriticalSection);
	if (const FLiveLinkCollectionSubjectItem* SubjectItem = Collection->FindSubject(InSubjectKey))
	{
		if (FLiveLinkSubject* LiveSubject = SubjectItem->GetLiveSubject())
		{
			// We don't store frame snapshots so we instead rely on the subject's last push time.
			return FApp::GetCurrentTime() - LiveSubject->GetLastPushTime() < GetDefault<ULiveLinkSettings>()->GetTimeWithoutFrameToBeConsiderAsInvalid();
		}
		return true;
	}
	return false;
}

void FLiveLinkHubClient::RemoveSubject_AnyThread(const FLiveLinkSubjectKey& InSubjectKey)
{
	OnSubjectMarkedPendingKill_AnyThread().Broadcast(InSubjectKey);
	FLiveLinkClient::RemoveSubject_AnyThread(InSubjectKey);
}

#undef LOCTEXT_NAMESPACE
