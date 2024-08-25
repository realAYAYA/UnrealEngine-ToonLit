// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkCustomTimeStep.h"

#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "Misc/App.h"



bool ULiveLinkCustomTimeStep::Initialize(UEngine* InEngine)
{
	State = ECustomTimeStepSynchronizationState::Synchronizing;
	EventLiveLink->Reset();

	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ModularFeatures.OnModularFeatureRegistered().AddUObject(this, &ULiveLinkCustomTimeStep::OnLiveLinkClientRegistered);
	ModularFeatures.OnModularFeatureUnregistered().AddUObject(this, &ULiveLinkCustomTimeStep::OnLiveLinkClientUnregistered);

	InitLiveLinkClient();

	return true;
}

void ULiveLinkCustomTimeStep::Shutdown(UEngine* InEngine)
{
	UninitLiveLinkClient();
}

bool ULiveLinkCustomTimeStep::UpdateTimeStep(UEngine* InEngine)
{
	if (State != ECustomTimeStepSynchronizationState::Synchronized)
	{
		EventWaitForSync->Trigger(); // Prevents start/restart quasi-deadlock.

		return true; // means that the Engine's TimeStep should be performed.
	}

	// Update anythread variable shadows
	{
		bLockStepModeAnyThread = bLockStepMode;
		FrameRateDividerAnyThread = FrameRateDivider;
		TimeoutInSecondsAnyThread = TimeoutInSeconds;
	}

	UpdateApplicationLastTime(); // Copies "CurrentTime" (used during the previous frame) in "LastTime"
	
	const double StartPlatformTime = FPlatformTime::Seconds();

	if (!WaitForSync())
	{
		State = ECustomTimeStepSynchronizationState::Synchronizing;
		return true;
	}

	const double EndPlatformTime = FPlatformTime::Seconds();

	const double ElapsedTimeWaitingForSync = EndPlatformTime - StartPlatformTime;

	// Subtract the multiple of the sync counts that corresponds to the desired divided live link data frame rate
	// For example if there are 3 sync counts and the frame rate divider is 2, then we'd want to use up 2 of the 3
	// sync counts and leave the 3rd one for the next cycle.

	LastSyncCountDelta = SyncCount;

	if (FrameRateDivider > 1)
	{
		LastSyncCountDelta -= (LastSyncCountDelta % FrameRateDivider);
	}

	SyncCount -= LastSyncCountDelta;

	// We are using real elapsed time for the time before sync, which will ultimately be used to calculate idle time. 
	// These values won't make sense if the frame rate settings are not correct.
	const double TimeAfterSync = FApp::GetLastTime() + GetLastSyncCountDelta() * GetSyncRate().AsInterval();
	const double TimeBeforeSync = TimeAfterSync - ElapsedTimeWaitingForSync;

	UpdateAppTimes(TimeBeforeSync, TimeAfterSync);

	return false; // false means that the Engine's TimeStep should NOT be performed.
}

ECustomTimeStepSynchronizationState ULiveLinkCustomTimeStep::GetSynchronizationState() const
{
	return State;
}

FFrameRate ULiveLinkCustomTimeStep::GetFixedFrameRate() const
{
	FFrameRate Rate = LiveLinkDataRate;

	if (FrameRateDivider > 1)
	{
		Rate.Denominator *= FrameRateDivider;
	}

	return Rate;
}

FFrameRate ULiveLinkCustomTimeStep::GetSyncRate() const
{
	return LiveLinkDataRate;
}

uint32 ULiveLinkCustomTimeStep::GetLastSyncCountDelta() const
{
	return LastSyncCountDelta;
}

bool ULiveLinkCustomTimeStep::IsLastSyncDataValid() const
{
	return true;
}

bool ULiveLinkCustomTimeStep::WaitForSync()
{	
	EventWaitForSync->Trigger();
	return EventLiveLink->Wait(FTimespan(TimeoutInSeconds * ETimespan::TicksPerSecond));
}


void ULiveLinkCustomTimeStep::BeginDestroy()
{
	UninitLiveLinkClient();
	Super::BeginDestroy();
}

#if WITH_EDITOR
void ULiveLinkCustomTimeStep::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULiveLinkCustomTimeStep, SubjectKey))
	{
		// If was already registered
		if (RegisterForFrameDataReceivedHandle.IsValid() && RegisteredSubjectKey != SubjectKey)
		{
			UnregisterLiveLinkSubject();
			RegisterLiveLinkSubject();
		}
		// If was waiting for the subject to be added to the client
		else if (LiveLinkClient && LiveLinkClient->GetSubjectSettings(SubjectKey))
		{
			OnLiveLinkSubjectAdded(SubjectKey);
		}
	}
}
#endif

void ULiveLinkCustomTimeStep::InitLiveLinkClient()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();

	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

		LiveLinkClient->OnLiveLinkSubjectAdded().AddUObject(this, &ULiveLinkCustomTimeStep::OnLiveLinkSubjectAdded);
		LiveLinkClient->OnLiveLinkSubjectRemoved().AddUObject(this, &ULiveLinkCustomTimeStep::OnLiveLinkSubjectRemoved);

		// if the subject already exist
		if (LiveLinkClient->GetSubjectSettings(SubjectKey))
		{
			OnLiveLinkSubjectAdded(SubjectKey);
		}
	}
}


void ULiveLinkCustomTimeStep::UninitLiveLinkClient()
{
	if (LiveLinkClient)
	{
		UnregisterLiveLinkSubject();

		LiveLinkClient->OnLiveLinkSubjectAdded().RemoveAll(this);
		LiveLinkClient->OnLiveLinkSubjectRemoved().RemoveAll(this);
		LiveLinkClient = nullptr;
		State = ECustomTimeStepSynchronizationState::Closed;
	}
}


void ULiveLinkCustomTimeStep::RegisterLiveLinkSubject()
{
	RegisteredSubjectKey = SubjectKey;

	FDelegateHandle Tmp;
	LiveLinkClient->RegisterForFrameDataReceived(
		RegisteredSubjectKey
		, FOnLiveLinkSubjectStaticDataReceived::FDelegate()
		, FOnLiveLinkSubjectFrameDataReceived::FDelegate::CreateUObject(this, &ULiveLinkCustomTimeStep::OnLiveLinkFrameDataReceived_AnyThread)
		, Tmp
		, RegisterForFrameDataReceivedHandle);
}


void ULiveLinkCustomTimeStep::UnregisterLiveLinkSubject()
{
	if (RegisterForFrameDataReceivedHandle.IsValid())
	{
		LiveLinkClient->UnregisterForFrameDataReceived(RegisteredSubjectKey, FDelegateHandle(), RegisterForFrameDataReceivedHandle);
		RegisterForFrameDataReceivedHandle.Reset();
	}

	RegisteredSubjectKey = FLiveLinkSubjectKey();
}


void ULiveLinkCustomTimeStep::OnLiveLinkClientRegistered(const FName& Type, class IModularFeature* ModularFeature)
{
	if (Type == ILiveLinkClient::ModularFeatureName && !LiveLinkClient)
	{
		InitLiveLinkClient();
	}
}


void ULiveLinkCustomTimeStep::OnLiveLinkClientUnregistered(const FName& Type, class IModularFeature* ModularFeature)
{
	if (Type == ILiveLinkClient::ModularFeatureName && ModularFeature == LiveLinkClient)
	{
		UninitLiveLinkClient();
		InitLiveLinkClient();
	}
}


void ULiveLinkCustomTimeStep::OnLiveLinkSubjectAdded(FLiveLinkSubjectKey InSubjectKey)
{
	if (InSubjectKey == SubjectKey)
	{
		RegisterLiveLinkSubject();
	}
}


void ULiveLinkCustomTimeStep::OnLiveLinkSubjectRemoved(FLiveLinkSubjectKey InSubjectKey)
{
	if (InSubjectKey == RegisteredSubjectKey)
	{
		State = ECustomTimeStepSynchronizationState::Error;
		UnregisterLiveLinkSubject();
	}
}


void ULiveLinkCustomTimeStep::OnLiveLinkFrameDataReceived_AnyThread(const FLiveLinkFrameDataStruct& InFrameData)
{
	// Increment sync counter based on frame id. For example, if frame id is incremented by 2, it means that a frame
	// was skipped and we must report 2 sync signals, even though only one was received. The purpose of this is to
	// reflect source delta times in the engine delta times.

	const FLiveLinkFrameIdentifier FrameId = InFrameData.GetBaseData()->FrameId;

	// Receiving data from the desired live link source automatically means that we are in synchronized state.
	if (State != ECustomTimeStepSynchronizationState::Synchronized)
	{
		State = ECustomTimeStepSynchronizationState::Synchronized;

		// Ensure our initial delta frame id is 1.
		LastFrameId = FrameId - 1;
	}

	// Loop around cannot be correctly handled unless we know the lower and upper bounds of the frame id.
	// For now, consider them as single increments even though we may miss skipped frames.
	// If FrameId is not changing, then we assume that the source does not support this, and we treat this
	// as a single sync increment as we otherwise lack the means of detecting skipped source frames.
	if (LastFrameId >= FrameId)
	{
		SyncCount++;
	}
	else
	{
		SyncCount += FrameId - LastFrameId;
	}

	// Note: We don't expect OnLiveLinkFrameDataReceived_AnyThread to be called for the same live link source concurrently, 
	// otherwise we would need to avoid race conditions here when updating and comparing with FrameId.
	LastFrameId = FrameId;

	// Trigger the event to release WaitForSync.
	// If the engine cannot keep up, it means that SyncCount will be greater than FrameRateDividerAnyThread and delta time will be multiplied accordingly
	if (SyncCount >= FrameRateDividerAnyThread)
	{
		EventLiveLink->Trigger();

		// In lockstep mode, the engine can stall the live link producer until it has had a chance to process its current data.
		// We can't block if this function is being called in the game thread because it would deadlock.
		if (bLockStepModeAnyThread && !IsInGameThread())
		{
			EventWaitForSync->Wait(FTimespan((TimeoutInSecondsAnyThread / 2) * ETimespan::TicksPerSecond));
		}
	}
}

