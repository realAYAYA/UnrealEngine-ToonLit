// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSDeviceHelper.h"
#include "IOSTargetPlatform.h"
#include "IOSTargetDeviceOutput.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "MiscIOSMessages"

DEFINE_LOG_CATEGORY_STATIC(LogIOSDeviceHelper, Log, All);

    enum DeviceConnectionInterface
    {
        NoValue = 0,
        USB = 1,
        Network = 2,
        Max = 4
    };

    struct FDeviceNotificationCallbackInformation
    {
        FString UDID;
        FString DeviceName;
        FString ProductType;
        DeviceConnectionInterface DeviceInterface;
        uint32 msgType;
		bool isAuthorized;
    };


    struct LibIMobileDevice
    {
        FString DeviceName;
        FString DeviceID;
        FString DeviceType;
		bool isAuthorized;
        DeviceConnectionInterface DeviceInterface;
    };

    static TArray<LibIMobileDevice> GetLibIMobileDevices()
    {
        FString OutStdOut;
        FString OutStdErr;
        FString LibimobileDeviceId = GetLibImobileDeviceExe("idevice_id");
        int ReturnCode;
        // get the list of devices UDID
        FPlatformProcess::ExecProcess(*LibimobileDeviceId, TEXT(""), &ReturnCode, &OutStdOut, &OutStdErr, NULL, true);
        

        TArray<LibIMobileDevice> ToReturn;
        // separate out each line
        TArray<FString> OngoingDeviceIds;
        
        OutStdOut.ParseIntoArray(OngoingDeviceIds, TEXT("\n"), true);
        TArray<FString> DeviceStrings;
        for (int32 StringIndex = 0; StringIndex < OngoingDeviceIds.Num(); ++StringIndex)
        {
            const FString& DeviceID = OngoingDeviceIds[StringIndex];
            DeviceConnectionInterface OngoingDeviceInterface = DeviceConnectionInterface::NoValue;

            FString OutStdOutInfo;
            FString OutStdErrInfo;
            FString LibimobileDeviceInfo = GetLibImobileDeviceExe("ideviceinfo");
            int ReturnCodeInfo;
            FString Arguments;

            if (OngoingDeviceIds[StringIndex].Contains("USB"))
            {
                OngoingDeviceInterface = DeviceConnectionInterface::USB;
            }
            else if (OngoingDeviceIds[StringIndex].Contains("Network"))
            {
                OngoingDeviceInterface = DeviceConnectionInterface::Network;
            }
            OngoingDeviceIds[StringIndex].Split(TEXT(" "), &OngoingDeviceIds[StringIndex], nullptr, ESearchCase::CaseSensitive, ESearchDir::FromStart);

            if (OngoingDeviceInterface == DeviceConnectionInterface::USB)
            {
                Arguments = "-u " + DeviceID;
            }
            else if (OngoingDeviceInterface == DeviceConnectionInterface::Network)
            {
                Arguments = "-n -u " + DeviceID;
            }

            FPlatformProcess::ExecProcess(*LibimobileDeviceInfo, *Arguments, &ReturnCodeInfo, &OutStdOutInfo, &OutStdErrInfo, NULL, true);
            
			LibIMobileDevice ToAdd;

            // ideviceinfo can fail when the connected device is untrusted. It can be "Pairing dialog response pending (-19)", "Invalid HostID (-21)" or "User denied pairing (-18)"
            // the only thing we can do is to make sure the Trust popup is correctly displayed.
            if (OutStdErrInfo.Contains("ERROR: "))
            {
				if (OutStdErrInfo.Contains("Could not connect to lockdownd"))
				{
					UE_LOG(LogIOSDeviceHelper, Warning, TEXT("Could not pair with connected iOS/tvOS device. Trust this computer by accepting the popup on device."));
					FString LibimobileDevicePair = GetLibImobileDeviceExe("idevicepair");
					FString PairArguments = "-u " + DeviceID + " pair";
					FPlatformProcess::ExecProcess(*LibimobileDevicePair, *PairArguments, &ReturnCodeInfo, &OutStdOutInfo, &OutStdErrInfo, NULL, true);
				}
				else
				{
					UE_LOG(LogIOSDeviceHelper, Warning, TEXT("Libimobile call failed : %s"), *OutStdErrInfo);
				}
				OutStdOutInfo.Empty();
				OutStdErrInfo.Empty();
				ToAdd.isAuthorized = false;
			}
			else
			{
				ToAdd.isAuthorized = true;
			}

			// parse product type and device name
			FString DeviceName;
            OutStdOutInfo.Split(TEXT("DeviceName: "), nullptr, &DeviceName, ESearchCase::CaseSensitive, ESearchDir::FromStart);
			DeviceName.Split(LINE_TERMINATOR, &DeviceName, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromStart);
			if (!ToAdd.isAuthorized)
            {
				DeviceName = LOCTEXT("IosTvosUnauthorizedDevice", "iOS / tvOS (Unauthorized)").ToString();
	        }
            FString ProductType;
            OutStdOutInfo.Split(TEXT("ProductType: "), nullptr, &ProductType, ESearchCase::CaseSensitive, ESearchDir::FromStart);
            ProductType.Split(LINE_TERMINATOR, &ProductType, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromStart);
            ToAdd.DeviceID = DeviceID;
            ToAdd.DeviceName = DeviceName;
            ToAdd.DeviceType = ProductType;
            ToAdd.DeviceInterface = OngoingDeviceInterface;
            ToReturn.Add(ToAdd);
        }
        return ToReturn;
    }

class FIOSDevice
{
public:
    FIOSDevice(FString InID, FString InName)
        : UDID(InID)
        , Name(InName)
    {
    }
    
    ~FIOSDevice()
    {
    }

    FString SerialNumber() const
    {
        return UDID;
    }

private:
    FString UDID;
    FString Name;
};

/**
 * Delegate type for devices being connected or disconnected from the machine
 *
 * The first parameter is newly added or removed device
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDeviceNotification, void*)

// recheck once per minute
#define        RECHECK_COUNTER_RESET            12

class FDeviceQueryTask
    : public FRunnable
{
public:
    FDeviceQueryTask()
        : Stopping(false)
        , bCheckDevices(true)
        , NeedSDKCheck(true)
        , RetryQuery(5)
    {}

    virtual bool Init() override
    {
        return true;
    }

    virtual uint32 Run() override
    {
        while (!Stopping)
        {
            if (IsEngineExitRequested())
            {
                break;
            }
            if (GetTargetPlatformManager())
            {
                FString OutTutorialPath;
                const ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(TEXT("IOS"));
                if (Platform)
                {
                    if (Platform->IsSdkInstalled(false, OutTutorialPath))
                    {
                        break;
                    }
                }
                Enable(false);
                return 0;
            }
            else
            {
                FPlatformProcess::Sleep(1.0f);
            }
        }
        int RecheckCounter = RECHECK_COUNTER_RESET;
        while (!Stopping)
        {
            if (IsEngineExitRequested())
            {
                break;
            }
            if (bCheckDevices)
            {
#if WITH_EDITOR
                if (!IsRunningCommandlet())
                {
                    //if (NeedSDKCheck)
                    //{
                    //    NeedSDKCheck = false;
                    //    FProjectStatus ProjectStatus;
                    //    if (!IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus) || (!ProjectStatus.IsTargetPlatformSupported(TEXT("IOS")) && !ProjectStatus.IsTargetPlatformSupported(TEXT("TVOS"))))
                    //    {
                    //        Enable(false);
                    //    }
                    //}
                    //else
                    {
                        // BHP - Turning off device check to prevent it from interfering with packaging
                        QueryDevices();
                    }
                }
#else
				 QueryDevices();
#endif
            }
            RecheckCounter--;
            if (RecheckCounter < 0)
            {
                RecheckCounter = RECHECK_COUNTER_RESET;
                bCheckDevices = true;
                NeedSDKCheck = true;
            }

            FPlatformProcess::Sleep(5.0f);
        }

        return 0;
    }

    virtual void Stop() override
    {
        Stopping = true;
    }

    virtual void Exit() override
    {}

    FDeviceNotification& OnDeviceNotification()
    {
        return DeviceNotification;
    }

    void Enable(bool bInCheckDevices)
    {
        bCheckDevices = bInCheckDevices;
    }

private:
    
    void NotifyDeviceChange(LibIMobileDevice& Device, bool bAdd)
    {
        FDeviceNotificationCallbackInformation CallbackInfo;

        if (bAdd)
        {
            CallbackInfo.DeviceName = Device.DeviceName;
            CallbackInfo.UDID = Device.DeviceID;
            CallbackInfo.DeviceInterface = Device.DeviceInterface;
            CallbackInfo.ProductType = Device.DeviceType;
            CallbackInfo.msgType = 1;
			CallbackInfo.isAuthorized = Device.isAuthorized;
        }
        else
        {
            CallbackInfo.UDID = Device.DeviceID;
            CallbackInfo.msgType = 2;
            DeviceNotification.Broadcast(&CallbackInfo);

        }
        DeviceNotification.Broadcast(&CallbackInfo);
    }
    
    void QueryDevices()
    {
        FString OutStdOut;
        FString OutStdErr;
		FString LibimobileDeviceId = GetLibImobileDeviceExe("idevice_id");
		int ReturnCode;
		if (LibimobileDeviceId.Len() == 0)
		{
			UE_LOG(LogIOSDeviceHelper, Log, TEXT("idevice_id (iOS device detection) executable missing. Turning off iOS/tvOS device detection."));
			Enable(false);
			return;
		}

        // get the list of devices UDID
        FPlatformProcess::ExecProcess(*LibimobileDeviceId, TEXT(""), &ReturnCode, &OutStdOut, &OutStdErr, NULL, true);
        if (OutStdOut.Len() == 0)
        {
            RetryQuery--;
            if (RetryQuery < 0)
            {
                UE_LOG(LogIOSDeviceHelper, Verbose, TEXT("IOS device listing is disabled for 1 minute (too many failed attempts)!"));
                Enable(false);
            }
			for (LibIMobileDevice device : CachedDevices)
			{
				NotifyDeviceChange(device, false);
			}
			CachedDevices.Empty();
            return;
        }
        RetryQuery = 5;
    
        TArray<LibIMobileDevice> ParsedDevices = GetLibIMobileDevices();

        for (int32 Index = 0; Index < ParsedDevices.Num(); ++Index)
        {
			TObjectPtr<LibIMobileDevice> Found = CachedDevices.FindByPredicate(
				[&](LibIMobileDevice Element) {
					return Element.DeviceID == ParsedDevices[Index].DeviceID;
				});
			if (Found != nullptr)
			{
				if (Found->isAuthorized != ParsedDevices[Index].isAuthorized)
				{
					NotifyDeviceChange(ParsedDevices[Index], false);
					NotifyDeviceChange(ParsedDevices[Index], true);
				}
				Found->DeviceID = "DealtWith";
			}
			else
			{
				NotifyDeviceChange(ParsedDevices[Index], true);
			}
	
        }

        for (int32 Index = 0; Index < CachedDevices.Num(); ++Index)
        {
            if (CachedDevices[Index].DeviceID != "DealtWith")
            {
                NotifyDeviceChange(CachedDevices[Index], false);
            }
        }

        CachedDevices.Empty();
		CachedDevices = ParsedDevices;
    }

    bool Stopping;
    bool bCheckDevices;
    bool NeedSDKCheck;
    int RetryQuery;
    TArray<LibIMobileDevice> CachedDevices;
    FDeviceNotification DeviceNotification;
};

/* FIOSDeviceHelper structors
 *****************************************************************************/
static TMap<FIOSDevice*, FIOSLaunchDaemonPong> ConnectedDevices;
static FDeviceQueryTask* QueryTask = NULL;
static FRunnableThread* QueryThread = NULL;
static TArray<FDeviceNotificationCallbackInformation> NotificationMessages;
static FTickerDelegate TickDelegate;

bool FIOSDeviceHelper::MessageTickDelegate(float DeltaTime)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FIOSDeviceHelper_MessageTickDelegate);

    for (int Index = 0; Index < NotificationMessages.Num(); ++Index)
    {
        FDeviceNotificationCallbackInformation cbi = NotificationMessages[Index];
        FIOSDeviceHelper::DeviceCallback(&cbi);
    }
    NotificationMessages.Empty();

    return true;
}

void FIOSDeviceHelper::Initialize(bool bIsTVOS)
{
    if(!bIsTVOS)
    {
        // add the message pump
        TickDelegate = FTickerDelegate::CreateStatic(MessageTickDelegate);
        FTSTicker::GetCoreTicker().AddTicker(TickDelegate, 5.0f);

        // kick off a thread to query for connected devices
        QueryTask = new FDeviceQueryTask();
        QueryTask->OnDeviceNotification().AddStatic(FIOSDeviceHelper::DeviceCallback);

        static int32 QueryTaskCount = 1;
        if (QueryTaskCount == 1)
        {
            // create the socket subsystem (loadmodule in game thread)
            ISocketSubsystem* SSS = ISocketSubsystem::Get();
            QueryThread = FRunnableThread::Create(QueryTask, *FString::Printf(TEXT("FIOSDeviceHelper.QueryTask_%d"), QueryTaskCount++), 128 * 1024, TPri_Normal);
        }
    }
}

void FIOSDeviceHelper::DeviceCallback(void* CallbackInfo)
{
    struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;

    if (!IsInGameThread())
    {
        NotificationMessages.Add(*cbi);
    }
    else
    {
        switch(cbi->msgType)
        {
        case 1:
            FIOSDeviceHelper::DoDeviceConnect(CallbackInfo);
            break;

        case 2:
            FIOSDeviceHelper::DoDeviceDisconnect(CallbackInfo);
            break;
        }
    }
}

void FIOSDeviceHelper::DoDeviceConnect(void* CallbackInfo)
{
    // connect to the device
    struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;
    FIOSDevice* Device = new FIOSDevice(cbi->UDID, cbi->DeviceName);

    // fire the event
    FIOSLaunchDaemonPong Event;
    Event.DeviceID = FString::Printf(TEXT("%s@%s"), cbi->ProductType.Contains(TEXT("AppleTV")) ? TEXT("TVOS") : TEXT("IOS"), *(cbi->UDID));
    Event.DeviceName = cbi->DeviceName;
    Event.DeviceType = cbi->ProductType;
	Event.bIsAuthorized = cbi->isAuthorized;
    Event.bCanReboot = false;
    Event.bCanPowerOn = false;
    Event.bCanPowerOff = false;
    FIOSDeviceHelper::OnDeviceConnected().Broadcast(Event);

    // add to the device list
    ConnectedDevices.Add(Device, Event);
}

void FIOSDeviceHelper::DoDeviceDisconnect(void* CallbackInfo)
{
    struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;
    FIOSDevice* device = NULL;
    for (auto DeviceIterator = ConnectedDevices.CreateIterator(); DeviceIterator; ++DeviceIterator)
    {
        if (DeviceIterator.Key()->SerialNumber() == cbi->UDID)
        {
            device = DeviceIterator.Key();
            break;
        }
    }
    if (device != NULL)
    {
        // extract the device id from the connected list
        FIOSLaunchDaemonPong Event = ConnectedDevices.FindAndRemoveChecked(device);

        // fire the event
        FIOSDeviceHelper::OnDeviceDisconnected().Broadcast(Event);

        // delete the device
        delete device;
    }
}

bool FIOSDeviceHelper::InstallIPAOnDevice(const FTargetDeviceId& DeviceId, const FString& IPAPath)
{
    return false;
}

void FIOSDeviceHelper::EnableDeviceCheck(bool OnOff)
{
    QueryTask->Enable(OnOff);
}

#undef LOCTEXT_NAMESPACE
