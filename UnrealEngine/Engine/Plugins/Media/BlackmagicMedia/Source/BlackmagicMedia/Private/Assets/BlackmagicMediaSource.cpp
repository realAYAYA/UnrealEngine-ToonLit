// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaSource.h"

#include "Blackmagic.h"
#include "BlackmagicDeviceProvider.h"
#include "BlackmagicMediaPrivate.h"
#include "IBlackmagicMediaModule.h"

#include "MediaIOCorePlayerBase.h"

UBlackmagicMediaSource::UBlackmagicMediaSource()
	: bCaptureAudio(false)
	, AudioChannels(EBlackmagicMediaAudioChannel::Stereo2)
	, MaxNumAudioFrameBuffer(8)
	, bCaptureVideo(true)
	, ColorFormat(EBlackmagicMediaSourceColorFormat::YUV8)
	, bIsSRGBInput(false)
	, MaxNumVideoFrameBuffer(8)
	, bLogDropFrame(false)
	, bEncodeTimecodeInTexel(false)
{
	MediaConfiguration.bIsInput = true;
	if (!MediaConfiguration.IsValid())
	{
		const FBlackmagicDeviceProvider DeviceProvider;
		const TArray<FMediaIOConfiguration> Configurations = DeviceProvider.GetConfigurations();
		if (Configurations.Num())
		{
			MediaConfiguration = Configurations[0];
		}
	}
}

/*
 * IMediaOptions interface
 */

bool UBlackmagicMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == BlackmagicMediaOption::CaptureAudio) { return bCaptureAudio; }
	if (Key == BlackmagicMediaOption::CaptureVideo) { return bCaptureVideo; }
	if (Key == BlackmagicMediaOption::LogDropFrame) { return bLogDropFrame; }
	if (Key == BlackmagicMediaOption::EncodeTimecodeInTexel) { return bEncodeTimecodeInTexel; }
	if (Key == BlackmagicMediaOption::SRGBInput) { return bIsSRGBInput; }

	return Super::GetMediaOption(Key, DefaultValue);
}

int64 UBlackmagicMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	if (Key == FMediaIOCoreMediaOption::FrameRateNumerator) { return MediaConfiguration.MediaMode.FrameRate.Numerator; }
	if (Key == FMediaIOCoreMediaOption::FrameRateDenominator) { return MediaConfiguration.MediaMode.FrameRate.Denominator; }
	if (Key == FMediaIOCoreMediaOption::ResolutionWidth) { return MediaConfiguration.MediaMode.Resolution.X; }
	if (Key == FMediaIOCoreMediaOption::ResolutionHeight) { return MediaConfiguration.MediaMode.Resolution.Y; }

	if (Key == BlackmagicMediaOption::DeviceIndex) { return MediaConfiguration.MediaConnection.Device.DeviceIdentifier; }
	if (Key == BlackmagicMediaOption::TimecodeFormat) { return (int64)AutoDetectableTimecodeFormat; }
	if (Key == BlackmagicMediaOption::AudioChannelOption) { return (int64)AudioChannels; }
	if (Key == BlackmagicMediaOption::MaxAudioFrameBuffer) { return MaxNumAudioFrameBuffer; }
	if (Key == BlackmagicMediaOption::BlackmagicVideoFormat) { return MediaConfiguration.MediaMode.DeviceModeIdentifier; }
	if (Key == BlackmagicMediaOption::ColorFormat) { return (int64)ColorFormat; }
	if (Key == BlackmagicMediaOption::MaxVideoFrameBuffer) { return MaxNumVideoFrameBuffer; }

	return Super::GetMediaOption(Key, DefaultValue);
}

FString UBlackmagicMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	if (Key == FMediaIOCoreMediaOption::VideoModeName)
	{
		return MediaConfiguration.MediaMode.GetModeName().ToString();
	}
	return Super::GetMediaOption(Key, DefaultValue);
}

bool UBlackmagicMediaSource::HasMediaOption(const FName& Key) const
{
	if (   Key == BlackmagicMediaOption::CaptureAudio
		|| Key == BlackmagicMediaOption::CaptureVideo
		|| Key == BlackmagicMediaOption::LogDropFrame
		|| Key == BlackmagicMediaOption::EncodeTimecodeInTexel
		|| Key == BlackmagicMediaOption::SRGBInput)
	{
		return true;
	}

	if (   Key == BlackmagicMediaOption::DeviceIndex
		|| Key == BlackmagicMediaOption::TimecodeFormat
		|| Key == BlackmagicMediaOption::AudioChannelOption
		|| Key == BlackmagicMediaOption::MaxAudioFrameBuffer
		|| Key == BlackmagicMediaOption::BlackmagicVideoFormat
		|| Key == BlackmagicMediaOption::ColorFormat
		|| Key == BlackmagicMediaOption::MaxVideoFrameBuffer)
	{
		return true;
	}

	if (   Key == FMediaIOCoreMediaOption::FrameRateNumerator
		|| Key == FMediaIOCoreMediaOption::FrameRateDenominator
		|| Key == FMediaIOCoreMediaOption::ResolutionWidth
		|| Key == FMediaIOCoreMediaOption::ResolutionHeight
		|| Key == FMediaIOCoreMediaOption::VideoModeName)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}

/*
 * UMediaSource interface
 */

FString UBlackmagicMediaSource::GetUrl() const
{
	return MediaConfiguration.MediaConnection.ToUrl();
}

bool UBlackmagicMediaSource::Validate() const
{
	FString FailureReason;
	if (!MediaConfiguration.IsValid())
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The MediaConfiguration '%s' is invalid."), *GetName());
		return false;
	}

	if (!IBlackmagicMediaModule::Get().IsInitialized())
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("Can't validate MediaSource '%s'. the Blackmagic library was not initialized."), *GetName());
		return false;
	}
	
	TUniquePtr<BlackmagicDesign::BlackmagicDeviceScanner> Scanner = MakeUnique<BlackmagicDesign::BlackmagicDeviceScanner>();
	BlackmagicDesign::BlackmagicDeviceScanner::DeviceInfo DeviceInfo;
	if (!Scanner->GetDeviceInfo(MediaConfiguration.MediaConnection.Device.DeviceIdentifier, DeviceInfo))
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that doesn't exist on this machine."), *GetName(), *MediaConfiguration.MediaConnection.Device.DeviceName.ToString());
		return false;
	}

	if (!DeviceInfo.bIsSupported)
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that is not supported by the Blackmagic SDK."), *GetName(), *MediaConfiguration.MediaConnection.Device.DeviceName.ToString());
		return false;
	}

	if (!DeviceInfo.bCanDoCapture)
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that can't capture."), *GetName(), *MediaConfiguration.MediaConnection.Device.DeviceName.ToString());
		return false;
	}

	if (bUseTimeSynchronization && AutoDetectableTimecodeFormat == EMediaIOAutoDetectableTimecodeFormat::None)
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The MediaSource '%s' use time synchronization but doesn't enabled the timecode."), *GetName());
		return false;
	}

	return true;
}

#if WITH_EDITOR
bool UBlackmagicMediaSource::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UBlackmagicMediaSource, bEncodeTimecodeInTexel))
	{
		return AutoDetectableTimecodeFormat != EMediaIOAutoDetectableTimecodeFormat::None && bCaptureVideo;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTimeSynchronizableMediaSource, bUseTimeSynchronization))
	{
		return AutoDetectableTimecodeFormat != EMediaIOAutoDetectableTimecodeFormat::None;
	}

	return true;
}

void UBlackmagicMediaSource::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	if (InPropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UBlackmagicMediaSource, AutoDetectableTimecodeFormat))
	{
		if (AutoDetectableTimecodeFormat == EMediaIOAutoDetectableTimecodeFormat::None)
		{
			bUseTimeSynchronization = false;
			bEncodeTimecodeInTexel = false;
		}
	}

	Super::PostEditChangeChainProperty(InPropertyChangedEvent);
}
#endif //WITH_EDITOR

void UBlackmagicMediaSource::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITORONLY_DATA
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (TimecodeFormat_DEPRECATED != EMediaIOTimecodeFormat::None)
		{
			switch (TimecodeFormat_DEPRECATED)
			{
			case EMediaIOTimecodeFormat::LTC:
				AutoDetectableTimecodeFormat = EMediaIOAutoDetectableTimecodeFormat::LTC;
				break;
			case EMediaIOTimecodeFormat::VITC:
				AutoDetectableTimecodeFormat = EMediaIOAutoDetectableTimecodeFormat::VITC;
				break;
			default:
				break;
			}
			TimecodeFormat_DEPRECATED = EMediaIOTimecodeFormat::None;
		}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}