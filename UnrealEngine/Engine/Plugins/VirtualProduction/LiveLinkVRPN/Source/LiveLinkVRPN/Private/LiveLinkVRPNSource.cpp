// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkVRPNSource.h"
#include "LiveLinkVRPN.h"
#include "ILiveLinkClient.h"
#include "Engine/Engine.h"
#include "Async/Async.h"
#include "LiveLinkVRPNSourceSettings.h"
#include "Misc/CoreDelegates.h"
#include "Roles/LiveLinkTransformRole.h"

#define LOCTEXT_NAMESPACE "LiveLinkVRPNSourceFactory"

FLiveLinkVRPNSource::FLiveLinkVRPNSource(const FLiveLinkVRPNConnectionSettings& ConnectionSettings)
: Client(nullptr)
, Stopping(false)
, Thread(nullptr)
, IPAddress(ConnectionSettings.IPAddress)
, LocalUpdateRateInHz(ConnectionSettings.LocalUpdateRateInHz)
, DeviceType(ConnectionSettings.Type)
, DeviceName(ConnectionSettings.DeviceName)
, SubjectName(FName(ConnectionSettings.SubjectName))
{
	SourceStatus = LOCTEXT("SourceStatus_NoData", "No data");
	SourceType = LOCTEXT("SourceType_VRPN", "VRPN");

	FString NewDeviceString = ConnectionSettings.DeviceName + TEXT("@") + ConnectionSettings.IPAddress;
	FString NewSourceNameString = ConnectionSettings.DeviceName + TEXT("(") + DeviceTypeToString(ConnectionSettings.Type) + TEXT(")@") + ConnectionSettings.IPAddress;
	SourceMachineName = FText::Format(LOCTEXT("VRPNSourceMachineName", "{0}"), FText::FromString(NewSourceNameString));

	VRPNDevice.RawPointer = nullptr;

	switch (DeviceType)
	{
		case EVRPNDeviceType::Analog:	if (vrpn_Analog_Remote* NewDevice = new vrpn_Analog_Remote(TCHAR_TO_ANSI(*NewDeviceString)))
										{
											if (NewDevice->register_change_handler(this, &FLiveLinkVRPNSource::OnAnalogChange) != 0)
											{
												delete NewDevice;
											}
											else
											{
												VRPNDevice.Analog = NewDevice;
											}
										}
										break;

		case EVRPNDeviceType::Dial:		if (vrpn_Dial_Remote* NewDevice = new vrpn_Dial_Remote(TCHAR_TO_ANSI(*NewDeviceString)))
										{
											if (NewDevice->register_change_handler(this, &FLiveLinkVRPNSource::OnDialChange) != 0)
											{
												delete NewDevice;
											}
											else
											{
												VRPNDevice.Dial = NewDevice;
											}
										}
										break;

		case EVRPNDeviceType::Button:	if (vrpn_Button_Remote* NewDevice = new vrpn_Button_Remote(TCHAR_TO_ANSI(*NewDeviceString)))
										{
											if (NewDevice->register_change_handler(this, &FLiveLinkVRPNSource::OnButtonChange) != 0)
											{
												delete NewDevice;
											}
											else
											{
												VRPNDevice.Button = NewDevice;
											}
										}
										break;

		case EVRPNDeviceType::Tracker:	if (vrpn_Tracker_Remote* NewDevice = new vrpn_Tracker_Remote(TCHAR_TO_ANSI(*NewDeviceString)))
										{
											if (NewDevice->register_change_handler(this, &FLiveLinkVRPNSource::OnTrackerChange) != 0)
											{
												delete NewDevice;
											}
											else
											{
												VRPNDevice.Tracker = NewDevice;
											}
										}
										break;

		default:						UE_LOG(LogLiveLinkVRPN, Error, TEXT("LiveLinkVRPNSource: Device type %d is invalid - this should never happen!"), DeviceType);
										break;
	}

	if (VRPNDevice.RawPointer != nullptr)
	{
		DeferredStartDelegateHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FLiveLinkVRPNSource::Start);
		UE_LOG(LogLiveLinkVRPN, Log, TEXT("LiveLinkVRPNSource: Opened device %s as type %s (%d)"), *DeviceName, *DeviceTypeToString(DeviceType), DeviceType);
	}
	else
	{
		UE_LOG(LogLiveLinkVRPN, Error, TEXT("LiveLinkVRPNSource: Failed to create device or register change handler for device %s as type %s (%d)"), *DeviceName, *DeviceTypeToString(DeviceType), DeviceType);
	}
}

FLiveLinkVRPNSource::~FLiveLinkVRPNSource()
{
	// This could happen if the object is destroyed before FCoreDelegates::OnEndFrame calls FLiveLinkVRPNSource::Start
	if (DeferredStartDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(DeferredStartDelegateHandle);
	}

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}

	if (VRPNDevice.RawPointer != nullptr)
	{
		bool bUnregisterFailed = false;
		switch (DeviceType)
		{
			case EVRPNDeviceType::Analog:	if (VRPNDevice.Analog->unregister_change_handler(this, &FLiveLinkVRPNSource::OnAnalogChange) != 0)
											{
												bUnregisterFailed = true;
											}
											delete VRPNDevice.Analog;
											break;

			case EVRPNDeviceType::Dial:		if (VRPNDevice.Dial->unregister_change_handler(this, &FLiveLinkVRPNSource::OnDialChange) != 0)
											{
												bUnregisterFailed = true;
											}
											delete VRPNDevice.Dial;
											break;

			case EVRPNDeviceType::Button:	if (VRPNDevice.Button->unregister_change_handler(this, &FLiveLinkVRPNSource::OnButtonChange) != 0)
											{
												bUnregisterFailed = true;
											}
											delete VRPNDevice.Button;
											break;

			case EVRPNDeviceType::Tracker:	if (VRPNDevice.Tracker->unregister_change_handler(this, &FLiveLinkVRPNSource::OnTrackerChange) != 0)
											{
												bUnregisterFailed = true;
											}
											delete VRPNDevice.Tracker;
											break;

			default:						UE_LOG(LogLiveLinkVRPN, Error, TEXT("LiveLinkVRPNSource: Delete device type %d is invalid - this should never happen!"), DeviceType);
											break;
		}

		if (bUnregisterFailed)
		{
			UE_LOG(LogLiveLinkVRPN, Error, TEXT("LiveLinkVRPNSource: Failed to unregister change handler for device %s as type %s (%d)"), *DeviceName, *DeviceTypeToString(DeviceType), DeviceType);
		}

		VRPNDevice.RawPointer = nullptr;
		UE_LOG(LogLiveLinkVRPN, Log, TEXT("LiveLinkVRPNSource: Closed device %s as type %s (%d)"), *DeviceName, *DeviceTypeToString(DeviceType), DeviceType);
	}
}

void FLiveLinkVRPNSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
}

bool FLiveLinkVRPNSource::IsSourceStillValid() const
{
	// Source is valid if we have a valid thread
	bool bIsSourceValid = !Stopping && (Thread != nullptr);
	return bIsSourceValid;
}

bool FLiveLinkVRPNSource::RequestSourceShutdown()
{
	Stop();

	return true;
}

const FString& FLiveLinkVRPNSource::DeviceTypeToString(const EVRPNDeviceType InDeviceType)
{
	static const FString DeviceTypeString_Analog(TEXT("Analog"));
	static const FString DeviceTypeString_Dial(TEXT("Dial"));
	static const FString DeviceTypeString_Button(TEXT("Button"));
	static const FString DeviceTypeString_Tracker(TEXT("Tracker"));
	static const FString DeviceTypeString_Invalid(TEXT("Invalid"));

	switch (InDeviceType)
	{
		case EVRPNDeviceType::Analog:	return DeviceTypeString_Analog; break;
		case EVRPNDeviceType::Dial:		return DeviceTypeString_Dial; break;
		case EVRPNDeviceType::Button:	return DeviceTypeString_Button; break;
		case EVRPNDeviceType::Tracker:	return DeviceTypeString_Tracker; break;
	}

	return DeviceTypeString_Invalid;
}

void VRPN_CALLBACK FLiveLinkVRPNSource::OnAnalogChange(void* UserData, vrpn_ANALOGCB const AnalogData)
{
	FLiveLinkVRPNSource* This = reinterpret_cast<FLiveLinkVRPNSource*>(UserData);

	if (This != nullptr)
	{
		FLiveLinkFrameDataStruct FrameData(FLiveLinkBaseFrameData::StaticStruct());
		FLiveLinkBaseFrameData* BaseFrameData = FrameData.Cast<FLiveLinkBaseFrameData>();
		for (int32 Index = 0; Index < AnalogData.num_channel; Index++)
		{
			BaseFrameData->PropertyValues.Add((float)AnalogData.channel[Index]);
		}

		const int32 NumProperties = AnalogData.num_channel;
		This->Send(&FrameData, NumProperties);
	}
}

void VRPN_CALLBACK FLiveLinkVRPNSource::OnDialChange(void* UserData, vrpn_DIALCB const DialData)
{
	FLiveLinkVRPNSource* This = reinterpret_cast<FLiveLinkVRPNSource*>(UserData);

	if (This != nullptr)
	{
		FLiveLinkFrameDataStruct FrameData(FLiveLinkBaseFrameData::StaticStruct());
		FLiveLinkBaseFrameData* BaseFrameData = FrameData.Cast<FLiveLinkBaseFrameData>();
		BaseFrameData->PropertyValues.Add((float)DialData.dial);
		BaseFrameData->PropertyValues.Add((float)DialData.change);

		const int32 NumProperties = 2;
		This->Send(&FrameData, NumProperties);
	}
}

void VRPN_CALLBACK FLiveLinkVRPNSource::OnButtonChange(void* UserData, vrpn_BUTTONCB const ButtonData)
{
	FLiveLinkVRPNSource* This = reinterpret_cast<FLiveLinkVRPNSource*>(UserData);

	if (This != nullptr)
	{
		FLiveLinkFrameDataStruct FrameData(FLiveLinkBaseFrameData::StaticStruct());
		FLiveLinkBaseFrameData* BaseFrameData = FrameData.Cast<FLiveLinkBaseFrameData>();
		BaseFrameData->PropertyValues.Add((float)ButtonData.button);
		BaseFrameData->PropertyValues.Add((float)ButtonData.state);

		const int32 NumProperties = 2;
		This->Send(&FrameData, NumProperties);
	}
}

void VRPN_CALLBACK FLiveLinkVRPNSource::OnTrackerChange(void* UserData, vrpn_TRACKERCB const TrackerData)
{
	FLiveLinkVRPNSource* This = reinterpret_cast<FLiveLinkVRPNSource*>(UserData);

	if (This != nullptr)
	{
		FLiveLinkFrameDataStruct FrameData(FLiveLinkTransformFrameData::StaticStruct());
		FLiveLinkTransformFrameData* TransformFrameData = FrameData.Cast<FLiveLinkTransformFrameData>();

		const float MetersToUnrealUnits = 100.0f;
		FVector NewPosition(TrackerData.pos[0] * MetersToUnrealUnits, TrackerData.pos[1] * MetersToUnrealUnits, TrackerData.pos[2] * MetersToUnrealUnits);
		FQuat NewRotation(TrackerData.quat[0], TrackerData.quat[1], TrackerData.quat[2], TrackerData.quat[3]);

		TransformFrameData->Transform = FTransform(NewRotation, NewPosition);

		const int32 NumProperties = 0;
		This->Send(&FrameData, NumProperties);
	}
}


// FRunnable interface
void FLiveLinkVRPNSource::Start()
{
	check(DeferredStartDelegateHandle.IsValid());

	FCoreDelegates::OnEndFrame.Remove(DeferredStartDelegateHandle);
	DeferredStartDelegateHandle.Reset();

	SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");

	ThreadName = "LiveLinkVRPN Receiver ";
	ThreadName.AppendInt(FAsyncThreadIndex::GetNext());

	Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}

void FLiveLinkVRPNSource::Stop()
{
	Stopping = true;
}

uint32 FLiveLinkVRPNSource::Run()
{
	const double SleepDeltaTime = 1.0 / (double)LocalUpdateRateInHz;

	while (!Stopping)
	{
		if (VRPNDevice.RawPointer != nullptr)
		{
			switch (DeviceType)
			{
				case EVRPNDeviceType::Analog:	VRPNDevice.Analog->mainloop();
												break;

				case EVRPNDeviceType::Dial:		VRPNDevice.Dial->mainloop();
												break;

				case EVRPNDeviceType::Button:	VRPNDevice.Button->mainloop();
												break;

				case EVRPNDeviceType::Tracker:	VRPNDevice.Tracker->mainloop();
												break;

				default:						UE_LOG(LogLiveLinkVRPN, Error, TEXT("LiveLinkVRPNSource: Run device type %d is invalid - this should never happen!"), DeviceType);
												break;
			}
		}

		FPlatformProcess::Sleep(SleepDeltaTime);
	}
	
	return 0;
}

void FLiveLinkVRPNSource::Send(FLiveLinkFrameDataStruct* FrameDataToSend, int32 NumProperties)
{
	static const FName PropertyName_Dial(TEXT("Dial"));
	static const FName PropertyName_Change(TEXT("Change"));
	static const FName PropertyName_Button(TEXT("Button"));
	static const FName PropertyName_State(TEXT("State"));

	if (Stopping || (Client == nullptr))
	{
		return;
	}

	if (!EncounteredSubjects.Contains(SubjectName))
	{
		if (NumProperties > 0)
		{
			FLiveLinkStaticDataStruct StaticData(FLiveLinkBaseStaticData::StaticStruct());
			FLiveLinkBaseStaticData& BaseData = *StaticData.Cast<FLiveLinkBaseStaticData>();

			switch (DeviceType)
			{
				case EVRPNDeviceType::Analog:	for (int32 Index = 0; Index < NumProperties; Index++)
												{
													BaseData.PropertyNames.Add(FName(FString::Printf(TEXT("Channel%d"), Index)));
												}
												break;

				case EVRPNDeviceType::Dial:		BaseData.PropertyNames.Add(PropertyName_Dial);
												BaseData.PropertyNames.Add(PropertyName_Change);
												break;

				case EVRPNDeviceType::Button:	BaseData.PropertyNames.Add(PropertyName_Button);
												BaseData.PropertyNames.Add(PropertyName_State);
												break;

				case EVRPNDeviceType::Tracker:	UE_LOG(LogLiveLinkVRPN, Error, TEXT("LiveLinkVRPNSource: Send device type Tracker should not have any properties - this should never happen!"));
												break;

				default:						UE_LOG(LogLiveLinkVRPN, Error, TEXT("LiveLinkVRPNSource: Send invalid device type %d - this should never happen!"), DeviceType);
												break;
			}

			Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkBasicRole::StaticClass(), MoveTemp(StaticData));
		}
		else
		{
			// If there are no float properties, then we are by default a Tracker device type which is just a transform
			FLiveLinkStaticDataStruct StaticData(FLiveLinkTransformStaticData::StaticStruct());
			Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkTransformRole::StaticClass(), MoveTemp(StaticData));
		}

		EncounteredSubjects.Add(SubjectName);
	}

	Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(*FrameDataToSend));
}

#undef LOCTEXT_NAMESPACE
