// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderMicrophoneAudioManager.h"

#include "TakeRecorderSettings.h"
#include "TakeRecorderSources.h"
#include "TakesUtils.h"

#include "IAudioCaptureEditor.h"
#include "IAudioCaptureEditorModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TakeRecorderMicrophoneAudioManager)


UTakeRecorderMicrophoneAudioManager::UTakeRecorderMicrophoneAudioManager(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
}

TUniquePtr<IAudioCaptureEditor> UTakeRecorderMicrophoneAudioManager::CreateAudioRecorderObject()
{
	IAudioCaptureEditorModule& Factory = FModuleManager::Get().LoadModuleChecked<IAudioCaptureEditorModule>("AudioCaptureEditor");
	return Factory.CreateAudioRecorder();
}

void UTakeRecorderMicrophoneAudioManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		SaveConfig();
	}

	// Notify the Mic sources of the new device channel count
	GetOnNotifySourcesOfDeviceChange().Broadcast(GetDeviceChannelCount());
	// Notify the UI of the change so it can update if needed
	GetOnAudioInputDeviceChanged().Broadcast();
}

void UTakeRecorderMicrophoneAudioManager::EnumerateAudioDevices(bool InForceRefresh)
{
	if (!AudioRecorder.IsValid() || InForceRefresh)
	{
		AudioRecorder = CreateAudioRecorderObject();
	}

	if (AudioInputDevice.DeviceInfoArray.Num() == 0 || InForceRefresh)
	{
		BuildDeviceInfoArray();
	}
}

void UTakeRecorderMicrophoneAudioManager::StartRecording(int32 InChannelCount)
{
	if (InChannelCount <= 0)
	{
		UE_LOG(LogTakesCore, Error, TEXT("Microphone Audio Source will not start. No active Mic Sources have been assigned audio device channels."));
		return;
	}

	if (AudioRecorder.IsValid())
	{
		if (AudioRecorder->IsReadyToRecord())
		{
			UE_LOG(LogTakesCore, Verbose, TEXT("Microphone Audio Source AudioRecorder Device: %s"), *(AudioInputDevice.DeviceId));

			FTakeRecorderAudioSettings RecorderSettings;
			RecorderSettings.AudioCaptureDeviceId = AudioInputDevice.DeviceId;
			RecorderSettings.NumRecordChannels = InChannelCount;
			RecorderSettings.AudioInputBufferSize = AudioInputDevice.AudioInputBufferSize;

			AudioRecorder->Start(RecorderSettings);
		}
	}
	else
	{
		UE_LOG(LogTakesCore, Error, TEXT("Microphone Audio Source could not start. Please check that the AudioCapture plugin is enabled"));
	}
}

void UTakeRecorderMicrophoneAudioManager::StopRecording()
{
	if (AudioRecorder.IsValid() && AudioRecorder->IsRecording())
	{
		AudioRecorder->Stop();
	}
}

TObjectPtr<USoundWave> UTakeRecorderMicrophoneAudioManager::GetRecordedSoundWave(const FTakeRecorderAudioSourceSettings& InSourceSettings)
{
	if (AudioRecorder.IsValid() && AudioRecorder->IsStopped())
	{
		return AudioRecorder->GetRecordedSoundWave(InSourceSettings);
	}

	return nullptr;
}

void UTakeRecorderMicrophoneAudioManager::FinalizeRecording()
{
	if (AudioRecorder.IsValid() && !AudioRecorder->IsReadyToRecord())
	{
		AudioRecorder.Reset();
	}

	if (!AudioRecorder.IsValid())
	{
		AudioRecorder = CreateAudioRecorderObject();
		check(AudioRecorder.IsValid());
	}
}

int32 UTakeRecorderMicrophoneAudioManager::GetDeviceChannelCount()
{
	if (!AudioInputDevice.DeviceId.IsEmpty())
	{
		for (const FAudioInputDeviceInfoProperty& DeviceInfo : AudioInputDevice.DeviceInfoArray)
		{
			if (DeviceInfo.DeviceId == AudioInputDevice.DeviceId)
			{
				return DeviceInfo.InputChannels;
			}
		}
	}

	return 0;
}

bool UTakeRecorderMicrophoneAudioManager::IsAudioDeviceAvailable(const FString& InDeviceId)
{
	if (!InDeviceId.IsEmpty())
	{
		for (const FAudioInputDeviceInfoProperty& DeviceInfo : AudioInputDevice.DeviceInfoArray)
		{
			if (DeviceInfo.DeviceId == InDeviceId)
			{
				return true;
			}
		}
	}

	return false;
}

void UTakeRecorderMicrophoneAudioManager::BuildDeviceInfoArray()
{
	AudioInputDevice.DeviceInfoArray.Empty();

	FTakeRecorderAudioDeviceInfo DefaultDeviceInfo;
	const bool bFoundDefaultDevice = AudioRecorder->GetCaptureDeviceInfo(DefaultDeviceInfo);

	TArray<FTakeRecorderAudioDeviceInfo> CaptureDevicesAvailable;
	AudioRecorder->GetCaptureDevicesAvailable(CaptureDevicesAvailable);
	for (const FTakeRecorderAudioDeviceInfo& DeviceInfo : CaptureDevicesAvailable)
	{
		const bool bIsDefaultDevice = bFoundDefaultDevice && (DeviceInfo.DeviceId == DefaultDeviceInfo.DeviceId);
		AudioInputDevice.DeviceInfoArray.Add(FAudioInputDeviceInfoProperty(DeviceInfo.DeviceName, DeviceInfo.DeviceId, DeviceInfo.InputChannels, DeviceInfo.PreferredSampleRate, bIsDefaultDevice));
	}

	// Device names often have numbers in them so perform
	// an alpha-numeric sort.
	Algo::Sort(AudioInputDevice.DeviceInfoArray,
		[](const FAudioInputDeviceInfoProperty& Left, const FAudioInputDeviceInfoProperty& Right) -> bool
		{
			auto StrToNum = [](const FString& InString, int32& InIndex) -> int32
			{
				const int32 StrLen = InString.Len();
				int32 Value = InString[InIndex] - '0';

				while ((InIndex + 1) < StrLen && FChar::IsDigit(InString[InIndex + 1]))
				{
					Value = (Value * 10) + (InString[++InIndex] - '0');
				}

				return Value;
			};

			const FString& LeftString = Left.DeviceName;
			const FString& RightString = Right.DeviceName;
			const int32 LeftLen = LeftString.Len();
			const int32 RightLen = RightString.Len();
			int32 LeftIndex = 0;
			int32 RightIndex = 0;

			while (LeftIndex < LeftLen && RightIndex < RightLen)
			{
				FString::ElementType LeftChar = LeftString[LeftIndex];
				FString::ElementType RightChar = RightString[RightIndex];
				if (FChar::IsDigit(LeftChar) && FChar::IsDigit(RightChar))
				{
					int32 LeftValue = StrToNum(LeftString, LeftIndex);
					int32 RightValue = StrToNum(RightString, RightIndex);

					if (LeftValue != RightValue)
					{
						return LeftValue < RightValue;
					}
				}
				else if (LeftChar != RightChar)
				{
					return LeftChar < RightChar;
				}

				++LeftIndex;
				++RightIndex;
			}

			return LeftLen < RightLen;
		});

	if (bFoundDefaultDevice)
	{
		if (AudioInputDevice.DeviceId.IsEmpty())
		{
			// Default to using the default input device if not already set
			AudioInputDevice.DeviceId = DefaultDeviceInfo.DeviceId;
		}
		else if (!IsAudioDeviceAvailable(AudioInputDevice.DeviceId))
		{
			// Revert to using the default input device if previously saved device is not available
			AudioInputDevice.DeviceId = DefaultDeviceInfo.DeviceId;
			UE_LOG(LogTakesCore, Error, TEXT("Previously saved audio input device unavailable. Falling back to default device \"%s\""), *DefaultDeviceInfo.DeviceName);
		}
	}
}
