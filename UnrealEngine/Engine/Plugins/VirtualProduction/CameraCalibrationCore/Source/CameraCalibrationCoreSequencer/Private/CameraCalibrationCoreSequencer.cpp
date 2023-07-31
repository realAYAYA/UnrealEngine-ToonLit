// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"

#include "Features/IModularFeatures.h"
#include "ISequencerModule.h"
#include "LensComponentTrackEditor.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneLensComponentTrackRecorder.h"

static const FName MovieSceneTrackRecorderFactoryName("MovieSceneTrackRecorderFactory");

class FCameraCalibrationCoreSequencerModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override
	{
		// Register Track Editor for LensComponent track
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		CreateLensComponentTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FLensComponentTrackEditor::CreateTrackEditor));

		// Register modular feature for LensComponent track recorder factory
		IModularFeatures::Get().RegisterModularFeature(MovieSceneTrackRecorderFactoryName, &LensComponentTrackRecorderFactory);
	}

	virtual void ShutdownModule() override
	{
		// Unregister Track Editor for LensComponent track
		if (ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer"))
		{
			SequencerModule->UnRegisterTrackEditor(CreateLensComponentTrackEditorHandle);
		}

		// Unregister modular feature for LensComponent track recorder factory
		IModularFeatures::Get().UnregisterModularFeature(MovieSceneTrackRecorderFactoryName, &LensComponentTrackRecorderFactory);
	}
	//~ End IModuleInterface interface

private:
	FDelegateHandle CreateLensComponentTrackEditorHandle;

	FMovieSceneLensComponentTrackRecorderFactory LensComponentTrackRecorderFactory;
};

IMPLEMENT_MODULE(FCameraCalibrationCoreSequencerModule, CameraCalibrationCoreSequencer);