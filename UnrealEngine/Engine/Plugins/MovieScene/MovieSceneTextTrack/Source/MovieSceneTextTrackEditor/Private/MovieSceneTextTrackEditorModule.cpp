// Copyright Epic Games, Inc. All Rights Reserved.

#include "ISequencerModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneTextChannel.h"
#include "SequencerChannelInterface.h"
#include "TextClipboardTypes.h"
#include "TextPropertyTrackEditor.h"

class FMovieSceneTextTrackEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	FDelegateHandle TextPropertyTrackCreateEditorHandle;
};

IMPLEMENT_MODULE(FMovieSceneTextTrackEditorModule, MovieSceneTextTrackEditor)

void FMovieSceneTextTrackEditorModule::StartupModule()
{
	if (!GIsEditor)
	{
		return;
	}

	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	TextPropertyTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FTextPropertyTrackEditor>();
	SequencerModule.RegisterChannelInterface<FMovieSceneTextChannel>();
}

void FMovieSceneTextTrackEditorModule::ShutdownModule()
{
	ISequencerModule* SequencerModule = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
	if (!SequencerModule)
	{
		return;
	}

	SequencerModule->UnRegisterTrackEditor(TextPropertyTrackCreateEditorHandle);
	TextPropertyTrackCreateEditorHandle.Reset();
}
