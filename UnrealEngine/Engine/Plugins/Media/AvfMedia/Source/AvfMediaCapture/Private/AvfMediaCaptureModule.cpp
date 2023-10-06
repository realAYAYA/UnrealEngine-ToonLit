// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvfMediaCapturePrivate.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Paths.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/NameTypes.h"
#include "IMediaCaptureSupport.h"
#include "Player/AvfMediaCapturePlayer.h"

#if WITH_EDITOR
	#include "ISettingsModule.h"
	#include "ISettingsSection.h"
	#include "UObject/Class.h"
	#include "UObject/WeakObjectPtr.h"
#endif

#import <AVFoundation/AVFoundation.h>


DEFINE_LOG_CATEGORY(LogAvfMediaCapture);

#define LOCTEXT_NAMESPACE "FAvfMediaCaptureFactoryModule"


// new API doesn't compile on old IOS sdk
#if (PLATFORM_IOS && (defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0))
	#define USE_NEW_MICROPHONE_API 1
#else
	#define USE_NEW_MICROPHONE_API 0
#endif


/**
 * Implements the AvfMediaCapture module.
 */
class FAvfMediaCaptureModule
	: public IModuleInterface
	, public IMediaPlayerFactory
	, public IMediaCaptureSupport
{
public:
	void EnumerateCaptureDevices(TArray<FMediaCaptureDeviceInfo>& OutDeviceInfos, EMediaCaptureDeviceType TargetDeviceType)
	{
		SCOPED_AUTORELEASE_POOL

        NSArray* DeviceTypes = nil;
        
    #if USE_NEW_MICROPHONE_API
        DeviceTypes = @[AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeMicrophone];
    #else
        DeviceTypes = @[AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeBuiltInMicrophone];
    #endif

		AVCaptureDeviceDiscoverySession* LocalDiscoverySession = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:DeviceTypes mediaType:nil position:AVCaptureDevicePositionUnspecified];
		if(LocalDiscoverySession != nil)
		{
			NSArray<AVCaptureDevice*>* Devices = LocalDiscoverySession.devices;
			for(uint32 i = 0;i < Devices.count;++i)
			{
				AVCaptureDevice* AvailableDevice = Devices[i];
            #if USE_NEW_MICROPHONE_API
                if(TargetDeviceType == EMediaCaptureDeviceType::Audio && AvailableDevice.deviceType == AVCaptureDeviceTypeMicrophone)
            #else
                if(TargetDeviceType == EMediaCaptureDeviceType::Audio && AvailableDevice.deviceType == AVCaptureDeviceTypeBuiltInMicrophone)
            #endif
				{
					FMediaCaptureDeviceInfo DeviceInfo;
					
					DeviceInfo.Type = TargetDeviceType;
					DeviceInfo.DisplayName = FText::FromString(FString(AvailableDevice.localizedName));
					DeviceInfo.Url = TEXT("audcap://") + FString(AvailableDevice.uniqueID);
					DeviceInfo.Info = FString(AvailableDevice.manufacturer);
					
					OutDeviceInfos.Add(MoveTemp(DeviceInfo));
				}
            #if USE_NEW_MICROPHONE_API
                else if(TargetDeviceType == EMediaCaptureDeviceType::Video && AvailableDevice.deviceType != AVCaptureDeviceTypeMicrophone)
            #else
                else if(TargetDeviceType == EMediaCaptureDeviceType::Video && AvailableDevice.deviceType != AVCaptureDeviceTypeBuiltInMicrophone)
            #endif
				{
					FMediaCaptureDeviceInfo DeviceInfo;
				
					DeviceInfo.Type = TargetDeviceType;
					DeviceInfo.DisplayName = FText::FromString(FString(AvailableDevice.localizedName));
					DeviceInfo.Url = TEXT("vidcap://") + FString(AvailableDevice.uniqueID);
					DeviceInfo.Info = FString(AvailableDevice.manufacturer);
					
					OutDeviceInfos.Add(MoveTemp(DeviceInfo));
				}
			}
		}
	}

	//~ IMediaCaptureSupport interface
	
	virtual void EnumerateAudioCaptureDevices(TArray<FMediaCaptureDeviceInfo>& OutDeviceInfos)
	{
		EnumerateCaptureDevices(OutDeviceInfos, EMediaCaptureDeviceType::Audio);
	}
	
	virtual void EnumerateVideoCaptureDevices(TArray<FMediaCaptureDeviceInfo>& OutDeviceInfos)
	{
		EnumerateCaptureDevices(OutDeviceInfos, EMediaCaptureDeviceType::Video);
	}

	//~ IMediaPlayerFactory interface

	virtual bool CanPlayUrl(const FString& Url, const IMediaOptions* /*Options*/, TArray<FText>* /*OutWarnings*/, TArray<FText>* OutErrors) const override
	{
		FString Scheme;
		FString Location;

		// check scheme
		if (!Url.Split(TEXT("://"), &Scheme, &Location, ESearchCase::CaseSensitive))
		{
			if (OutErrors != nullptr)
			{
				OutErrors->Add(LOCTEXT("NoSchemeFound", "No URI scheme found"));
			}

			return false;
		}

		if (!SupportedUriSchemes.Contains(Scheme))
		{
			if (OutErrors != nullptr)
			{
				OutErrors->Add(FText::Format(LOCTEXT("SchemeNotSupported", "The URI scheme '{0}' is not supported"), FText::FromString(Scheme)));
			}

			return false;
		}

		return true;
	}

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		return MakeShared<FAvfMediaCapturePlayer, ESPMode::ThreadSafe>(EventSink);
	}

	virtual FText GetDisplayName() const override
	{
		return LOCTEXT("MediaCaptureDisplayName", "Apple AV Foundation");
	}

	virtual FName GetPlayerName() const override
	{
		static FName PlayerName(TEXT("AvfMediaCapture"));
		return PlayerName;
	}

	virtual FGuid GetPlayerPluginGUID() const override
	{
		static FGuid PlayerPluginGUID(0xcf78bfd2, 0x0c1111ed, 0x861d0242, 0xac120002);
		return PlayerPluginGUID;
	}

	virtual const TArray<FString>& GetSupportedPlatforms() const override
	{
		return SupportedPlatforms;
	}

	virtual bool SupportsFeature(EMediaFeature Feature) const override
	{
		return ((Feature == EMediaFeature::AudioSamples) ||
				(Feature == EMediaFeature::VideoSamples));
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		// supported schemes
		SupportedUriSchemes.Add(TEXT("vidcap"));
		SupportedUriSchemes.Add(TEXT("audcap"));

		// supported platforms
		SupportedPlatforms.Add(TEXT("Mac"));
		SupportedPlatforms.Add(TEXT("iOS"));

		// register factory support functions
		auto MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");
		if (MediaModule != nullptr)
		{
			MediaModule->RegisterPlayerFactory(*this);
			MediaModule->RegisterCaptureSupport(*this);
		}
	}

	virtual void ShutdownModule() override
	{
		// unregister factory support functions
		auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");
		if (MediaModule != nullptr)
		{
			MediaModule->UnregisterPlayerFactory(*this);
			MediaModule->UnregisterCaptureSupport(*this);
		}
	}

private:

	/** List of platforms that the media player support. */
	TArray<FString> SupportedPlatforms;

	/** List of supported URI schemes. */
	TArray<FString> SupportedUriSchemes;
};


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvfMediaCaptureModule, AvfMediaCapture);
