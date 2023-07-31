// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkSource.h"
#include "LiveLinkVRPNConnectionSettings.h"
#include "LiveLinkVRPNSourceSettings.h"
#include "Roles/LiveLinkTransformTypes.h"

#include "Delegates/IDelegateInstance.h"
#include "MessageEndpoint.h"
#include "IMessageContext.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/Runnable.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
// Disable macro redefinition warning
#pragma warning(push)
#pragma warning(disable:4005)
#include "VRPN/vrpn_Analog.h"
#include "VRPN/vrpn_Dial.h"
#include "VRPN/vrpn_Button.h"
#include "VRPN/vrpn_Tracker.h"
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

struct ULiveLinkVRPNSettings;

class ILiveLinkClient;

union VRPNDeviceUnion
{
	vrpn_Analog_Remote*		Analog;
	vrpn_Dial_Remote*		Dial;
	vrpn_Button_Remote*		Button;
	vrpn_Tracker_Remote*	Tracker;
	void*					RawPointer;
};

class LIVELINKVRPN_API FLiveLinkVRPNSource : public ILiveLinkSource, public FRunnable, public TSharedFromThis<FLiveLinkVRPNSource>
{
public:

	FLiveLinkVRPNSource(const FLiveLinkVRPNConnectionSettings& ConnectionSettings);

	virtual ~FLiveLinkVRPNSource();

	// Begin ILiveLinkSource Interface
	
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;

	virtual bool IsSourceStillValid() const override;

	virtual bool RequestSourceShutdown() override;

	virtual FText GetSourceType() const override { return SourceType; };
	virtual FText GetSourceMachineName() const override { return SourceMachineName; }
	virtual FText GetSourceStatus() const override { return SourceStatus; }

	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override { return ULiveLinkVRPNSourceSettings::StaticClass(); }

	// End ILiveLinkSource Interface

	// Begin FRunnable Interface

	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	void Start();
	virtual void Stop() override;
	virtual void Exit() override { }

	// End FRunnable Interface

private:
	const FString& DeviceTypeToString(const EVRPNDeviceType InDeviceType);

	void Send(FLiveLinkFrameDataStruct* FrameDataToSend, const int32 NumProperties);

private:
	ILiveLinkClient* Client;

	// Our identifier in LiveLink
	FGuid SourceGuid;

	FMessageAddress ConnectionAddress;

	FText SourceType;
	FText SourceMachineName;
	FText SourceStatus;
	
	// Threadsafe Bool for terminating the main thread loop
	FThreadSafeBool Stopping;
	
	// Thread to run updates on
	FRunnableThread* Thread;
	
	// Name of the updates thread
	FString ThreadName;

	// List of subjects we've already encountered
	TSet<FName> EncounteredSubjects;

	// Deferred start delegate handle
	FDelegateHandle DeferredStartDelegateHandle;

	// IP address of the VRPN server
	FString IPAddress = TEXT("127.0.0.1");

	// Maximum rate at which to refresh the server
	uint32 LocalUpdateRateInHz = 120;

	// VRPN device parameters
	EVRPNDeviceType DeviceType;
	FString DeviceName;
	FName SubjectName;

	// Actual VRPN device
	VRPNDeviceUnion VRPNDevice;

	// Callbacks for VRPN change handlers
	static void VRPN_CALLBACK OnAnalogChange(void* UserData, vrpn_ANALOGCB const AnalogData);
	static void VRPN_CALLBACK OnDialChange(void* UserData, vrpn_DIALCB const DialData);
	static void VRPN_CALLBACK OnButtonChange(void* UserData, vrpn_BUTTONCB const ButtonData);
	static void VRPN_CALLBACK OnTrackerChange(void* UserData, vrpn_TRACKERCB const TrackerData);
};
