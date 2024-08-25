// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyAnimatorEditorModule.h"

#include "MovieScene/Easing/PropertyAnimatorEasingDoubleChannel.h"
#include "MovieScene/Wave/PropertyAnimatorWaveDoubleChannel.h"
#include "Styles/PropertyAnimatorEditorStyle.h"

void FPropertyAnimatorEditorModule::StartupModule()
{
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	RegisterCurveChannelInterface<FPropertyAnimatorWaveDoubleChannel>(SequencerModule);
	RegisterCurveChannelInterface<FPropertyAnimatorEasingDoubleChannel>(SequencerModule);

	// Init once
	FPropertyAnimatorEditorStyle::Get();
}

IMPLEMENT_MODULE(FPropertyAnimatorEditorModule, PropertyAnimatorEditor)
