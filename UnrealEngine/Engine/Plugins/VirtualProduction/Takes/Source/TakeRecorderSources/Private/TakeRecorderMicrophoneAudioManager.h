// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSourceProperty.h"

#include "TakeRecorderMicrophoneAudioManager.generated.h"

class IAudioCaptureEditor;
class USoundWave;
struct FTakeRecorderAudioSourceSettings;


DECLARE_MULTICAST_DELEGATE_OneParam(FOnNotifySourcesOfDeviceChange, int);

/** This class exposes the audio input device list via the project settings details. It does this in 
*   conjunction with FAudioInputDevicePropertyCustomization. It also manages the IAudioCaptureEditor 
*   object which handles the low level audio device recording.
*/
UCLASS(config=EditorPerProjectUserSettings, PerObjectConfig, DisplayName = "Audio Input Device")
class TAKERECORDERSOURCES_API UTakeRecorderMicrophoneAudioManager : public UTakeRecorderAudioInputSettings
{
public:
	GENERATED_BODY()

	UTakeRecorderMicrophoneAudioManager(const FObjectInitializer& ObjInit);

	// Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject Interface

	/** Enumerates the audio devices present on the current machine */
	virtual void EnumerateAudioDevices(bool InForceRefresh = false) override;

	/** Returns input channel count for currently selected audio device */
	virtual int32 GetDeviceChannelCount() override;

	/** The audio device to use for this microphone source */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (ShowOnlyInnerProperties))
	FAudioInputDeviceProperty AudioInputDevice;

	/** 
	*  Calls StartRecording on the AudioRecorder object. This is called multiple times (once for each
	*  microphone source, however, only the first call triggers the call to AudioRecorder.
	*/
	void StartRecording(int32 InChannelCount);
	/**
	 * Calls StopRecording on the AudioRecorder object. This is called multiple times (once for each
	 * microphone source, however, only the first call triggers the call to AudioRecorder.
	 */
	void StopRecording();
	/**
	 *  Calls FinalizeRecording on the AudioRecorder object. This is called multiple times (once for each
	 *  microphone source, however, only the first call triggers the call to AudioRecorder.
	 */
	void FinalizeRecording();


	/** Fetches the USoundWave for this source after a Take has been recorded */
	TObjectPtr<class USoundWave> GetRecordedSoundWave(const FTakeRecorderAudioSourceSettings& InSourceSettings);
	/** Accessor for the OnNotifySourcesOfDeviceChange delegate list */
	FOnNotifySourcesOfDeviceChange& GetOnNotifySourcesOfDeviceChange() { return OnNotifySourcesOfDeviceChange; }

private:

	/** Multicast delegate which notifies clients when the currently selected audio device changes. */
	FOnNotifySourcesOfDeviceChange OnNotifySourcesOfDeviceChange;
	/** Calls factory to create the AudioRecorder object */
	TUniquePtr<IAudioCaptureEditor> CreateAudioRecorderObject();
	/** Builds the list of audio devices which will be used in the device menu */
	void BuildDeviceInfoArray();

	/** Returns whether audio device with given Id was found during enumeration */
	bool IsAudioDeviceAvailable(const FString& InDeviceId);

	/** The audio recorder object which manages low level recording of audio data */
	TUniquePtr<IAudioCaptureEditor> AudioRecorder;
};
