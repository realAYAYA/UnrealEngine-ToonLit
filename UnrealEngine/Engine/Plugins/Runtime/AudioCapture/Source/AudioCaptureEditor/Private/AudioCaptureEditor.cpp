// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioCaptureEditor.h"
#include "ISequenceRecorder.h"
#include "ISequenceAudioRecorder.h"
#include "Modules/ModuleManager.h"
#include "AudioRecordingManager.h"

class FSequenceAudioRecorder : public ISequenceAudioRecorder
{
public:

	virtual void Start(const FSequenceAudioRecorderSettings& InSettings) override
	{
		Audio::FRecordingSettings RecordingSettings;
		RecordingSettings.AssetName = InSettings.AssetName;
		RecordingSettings.Directory = InSettings.Directory;
		RecordingSettings.GainDb = InSettings.GainDb;
		RecordingSettings.RecordingDuration = InSettings.RecordingDuration;
		RecordingSettings.bSplitChannels = InSettings.bSplitChannels;

		TArray<USoundWave*> OutSoundWaves;
		Audio::FAudioRecordingManager::Get().StartRecording(RecordingSettings, OutSoundWaves);
	}

	virtual void Stop(TArray<USoundWave*>& OutSoundWaves) override
	{
		Audio::FAudioRecordingManager::Get().StopRecording(OutSoundWaves);
	}
};


class FAudioCaptureEditorModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		ISequenceRecorder& Recorder = FModuleManager::Get().LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
		RecorderHandle = Recorder.RegisterAudioRecorder(
			[](){
				return TUniquePtr<ISequenceAudioRecorder>(new FSequenceAudioRecorder());
			}
		);
	}

	virtual void ShutdownModule() override
	{
		ISequenceRecorder* Recorder = FModuleManager::Get().GetModulePtr<ISequenceRecorder>("SequenceRecorder");
		
		if (Recorder)
		{
			Recorder->UnregisterAudioRecorder(RecorderHandle);
		}
	}

	FDelegateHandle RecorderHandle;
};


IMPLEMENT_MODULE( FAudioCaptureEditorModule, AudioCaptureEditor );
