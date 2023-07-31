// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkXRSource.h"

#include "Async/Async.h"
#include "Engine/Engine.h"
#include "ILiveLinkClient.h"
#include "LiveLinkSubjectSettings.h"
#include "LiveLinkXR.h"
#include "LiveLinkXRSourceSettings.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "Roles/LiveLinkTransformRole.h"

#define LOCTEXT_NAMESPACE "LiveLinkXRSource"

static TAutoConsoleVariable<float> CVarLiveLinkXRDeviceEnumerationInterval(TEXT("LiveLinkXR.DeviceEnumerationInterval"), 1.0f, TEXT("Interval in seconds at which available SteamVR devices should be verified."));


FLiveLinkXRSource::FLiveLinkXRSource(const FLiveLinkXRConnectionSettings& Settings)
: Client(nullptr)
, Stopping(false)
, Thread(nullptr)
{
	SourceStatus = LOCTEXT("SourceStatus_NoData", "No data");
	SourceType = LOCTEXT("SourceType_XR", "XR");
	SourceMachineName = LOCTEXT("XRSourceMachineName", "Local XR");

	if (!GEngine->XRSystem.IsValid())
	{
		UE_LOG(LogLiveLinkXR, Error, TEXT("LiveLinkXRSource: Couldn't find a valid XR System!"));
		return;
	}

	if (GEngine->XRSystem->GetSystemName() != FName(TEXT("SteamVR")))
	{
		UE_LOG(LogLiveLinkXR, Error, TEXT("LiveLinkXRSource: Couldn't find a compatible XR System - currently, only SteamVR is supported!"));
		return;
	}

	bTrackTrackers = Settings.bTrackTrackers;
	bTrackControllers = Settings.bTrackControllers;
	bTrackHMDs = Settings.bTrackHMDs;
	LocalUpdateRateInHz = Settings.LocalUpdateRateInHz;

	DeferredStartDelegateHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FLiveLinkXRSource::Start);
}

FLiveLinkXRSource::~FLiveLinkXRSource()
{
	// This could happen if the object is destroyed before FCoreDelegates::OnEndFrame calls FLiveLinkXRSource::Start
	if (DeferredStartDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(DeferredStartDelegateHandle);
	}

	if (Client)
	{
		Client->OnLiveLinkSubjectAdded().Remove(OnSubjectAddedDelegate);
	}

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FLiveLinkXRSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;

	OnSubjectAddedDelegate = Client->OnLiveLinkSubjectAdded().AddRaw(this, &FLiveLinkXRSource::OnLiveLinkSubjectAdded);
}

void FLiveLinkXRSource::InitializeSettings(ULiveLinkSourceSettings* Settings)
{
}

void FLiveLinkXRSource::Update()
{
	const double CurrentTime = FPlatformTime::Seconds();
	if((CurrentTime - LastEnumerationTimestamp) > CVarLiveLinkXRDeviceEnumerationInterval.GetValueOnGameThread())
	{
		LastEnumerationTimestamp = CurrentTime;
		EnumerateTrackedDevices();
	}	
}

bool FLiveLinkXRSource::IsSourceStillValid() const
{
	// Source is valid if we have a valid thread
	const bool bIsSourceValid = !Stopping && (Thread != nullptr);
	return bIsSourceValid;
}

bool FLiveLinkXRSource::RequestSourceShutdown()
{
	Stop();

	return true;
}

void FLiveLinkXRSource::OnSettingsChanged(ULiveLinkSourceSettings* Settings, const FPropertyChangedEvent& PropertyChangedEvent)
{
	ILiveLinkSource::OnSettingsChanged(Settings, PropertyChangedEvent);
}

void FLiveLinkXRSource::EnumerateTrackedDevices()
{
	if (!GEngine->XRSystem.IsValid())
	{
		return;
	}

	if (GEngine->XRSystem->GetSystemName() != FName(TEXT("SteamVR")))
	{
		return;
	}

	// Create subject names for all requested tracked devices
	TArray<int32> AllTrackedDevices;

	if (!GEngine->XRSystem->EnumerateTrackedDevices(AllTrackedDevices, EXRTrackedDeviceType::Any))
	{
		return;
	}

	FScopeLock Lock(&TrackedDeviceCriticalSection);

	for (int32 TrackerIndex = 0; TrackerIndex < AllTrackedDevices.Num(); TrackerIndex++)
	{
		FString SubjectStringId = GEngine->XRSystem->GetSystemName().ToString();
		bool bValidDevice = false;
		switch ((int32)GEngine->XRSystem->GetTrackedDeviceType(AllTrackedDevices[TrackerIndex]))
		{
		case (int32)EXRTrackedDeviceType::Other:
			if (bTrackTrackers)
			{
				SubjectStringId += TEXT("Tracker_");
				bValidDevice = true;
			}
			break;

		case (int32)EXRTrackedDeviceType::HeadMountedDisplay:
			if (bTrackHMDs)
			{
				SubjectStringId += TEXT("HMD_");
				bValidDevice = true;
			}
			break;

		case (int32)EXRTrackedDeviceType::Controller:
			if (bTrackControllers)
			{
				SubjectStringId += TEXT("Controller_");
				bValidDevice = true;
			}
			break;
		}

		if (bValidDevice)
		{
			SubjectStringId += GEngine->XRSystem->GetTrackedDevicePropertySerialNumber(AllTrackedDevices[TrackerIndex]);
			const FName SubjectName(SubjectStringId);
			if(TrackedDevices.ContainsByPredicate([SubjectName](const FTrackedDeviceInfo& Other){ return Other.SubjectName == SubjectName; }) == false)
			{
				FTrackedDeviceInfo NewDevice(AllTrackedDevices[TrackerIndex], GEngine->XRSystem->GetTrackedDeviceType(AllTrackedDevices[TrackerIndex]), SubjectName);
				UE_LOG(LogLiveLinkXR, Log, TEXT("LiveLinkXRSource: Found a tracked device with DeviceId %d and named it %s"), NewDevice.DeviceId, *SubjectStringId);
				TrackedDevices.Add(MoveTemp(NewDevice));
			}
		}
	}
}

// FRunnable interface
void FLiveLinkXRSource::Start()
{
	check(DeferredStartDelegateHandle.IsValid());

	FCoreDelegates::OnEndFrame.Remove(DeferredStartDelegateHandle);
	DeferredStartDelegateHandle.Reset();

	EnumerateTrackedDevices();
	
	SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");

	ThreadName = "LiveLinkXR Receiver ";
	ThreadName.AppendInt(FAsyncThreadIndex::GetNext());

	Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}

void FLiveLinkXRSource::Stop()
{
	Stopping = true;
}

uint32 FLiveLinkXRSource::Run()
{
	const double CheckDeltaTime = 1.0f / (double)LocalUpdateRateInHz;
	double StartTime = FApp::GetCurrentTime();
	double EndTime = StartTime + CheckDeltaTime;

	while (!Stopping)
	{
		// Send new poses at the user specified update rate
		if (EndTime - StartTime >= CheckDeltaTime)
		{
			StartTime = FApp::GetCurrentTime();

			// Make a copy of tracked device array to avoid locking for too long
			TArray<FTrackedDeviceInfo> TrackedDevicesCopy;
			{
				FScopeLock Lock(&TrackedDeviceCriticalSection);
				TrackedDevicesCopy = TrackedDevices;
			}

			// Send new poses at the user specified update rate
			for (const FTrackedDeviceInfo& Device : TrackedDevicesCopy)
			{
				FQuat Orientation;
				FVector Position;

				if (GEngine->XRSystem->IsTracking(Device.DeviceId) && GEngine->XRSystem->GetCurrentPose(Device.DeviceId, Orientation, Position))
				{
					FLiveLinkFrameDataStruct FrameData(FLiveLinkTransformFrameData::StaticStruct());
					FLiveLinkTransformFrameData* TransformFrameData = FrameData.Cast<FLiveLinkTransformFrameData>();
					TransformFrameData->Transform = FTransform(Orientation, Position);

					// These don't change frame to frame, so they really should be in static data. However, there is no MetaData in LiveLink static data :(
					TransformFrameData->MetaData.StringMetaData.Add(FName(TEXT("DeviceId")), FString::Printf(TEXT("%d"), Device.DeviceId));
					TransformFrameData->MetaData.StringMetaData.Add(FName(TEXT("DeviceType")), GetDeviceTypeName(Device.DeviceType));
					TransformFrameData->MetaData.StringMetaData.Add(FName(TEXT("DeviceControlId")), Device.SubjectName.ToString());

					Send(&FrameData, Device.SubjectName);
				}
			}

			FrameCounter++;
		}

		FPlatformProcess::Sleep(0.001f);
		EndTime = FApp::GetCurrentTime();
	}
	
	return 0;
}

void FLiveLinkXRSource::Send(FLiveLinkFrameDataStruct* FrameDataToSend, FName SubjectName)
{
	if (Stopping || (Client == nullptr))
	{
		return;
	}

	if (!EncounteredSubjects.Contains(SubjectName))
	{
		// If the LiveLink client already knows about this subject, then it must have been added via a preset
		// Only new subjects should be set to rebroadcast by default. Presets should respect the existing settings
		if (!Client->GetSubjects(true, true).Contains(FLiveLinkSubjectKey(SourceGuid, SubjectName)))
		{
			SubjectsToRebroadcast.Add(SubjectName);
		}

		FLiveLinkStaticDataStruct StaticData(FLiveLinkTransformStaticData::StaticStruct());
		Client->PushSubjectStaticData_AnyThread({SourceGuid, SubjectName}, ULiveLinkTransformRole::StaticClass(), MoveTemp(StaticData));
		EncounteredSubjects.Add(SubjectName);
	}

	Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(*FrameDataToSend));
}

void FLiveLinkXRSource::OnLiveLinkSubjectAdded(FLiveLinkSubjectKey InSubjectKey)
{
	// Set rebroadcast to true for any new subjects
	if (SubjectsToRebroadcast.Contains(InSubjectKey.SubjectName))
	{
		ULiveLinkSubjectSettings* SubjectSettings = Cast<ULiveLinkSubjectSettings>(Client->GetSubjectSettings(InSubjectKey));
		if (SubjectSettings)
		{
			SubjectSettings->bRebroadcastSubject = true;
		}
	}
}

const FString FLiveLinkXRSource::GetDeviceTypeName(EXRTrackedDeviceType DeviceType)
{
	switch ((int32)DeviceType)
	{
		case (int32)EXRTrackedDeviceType::Invalid:				return TEXT("Invalid");
		case (int32)EXRTrackedDeviceType::HeadMountedDisplay:	return TEXT("HMD");
		case (int32)EXRTrackedDeviceType::Controller:			return TEXT("Controller");
		case (int32)EXRTrackedDeviceType::Other:				return TEXT("Tracker");
	}

	return FString::Printf(TEXT("Unknown (%i)"), (int32)DeviceType);
}

#undef LOCTEXT_NAMESPACE
