// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralMorphEditorProjectSettings.h"

#define LOCTEXT_NAMESPACE "NeuralMorphEditorProjectSettings"

const UNeuralMorphEditorProjectSettings* UNeuralMorphEditorProjectSettings::Get()
{ 
	return GetDefault<UNeuralMorphEditorProjectSettings>();
}

#if WITH_EDITOR
	FText UNeuralMorphEditorProjectSettings::GetSectionText() const 
	{ 
		return LOCTEXT("NeuralMorphModelProjectSettingsName", "Neural Morph Model"); 
	}

	FText UNeuralMorphEditorProjectSettings::GetSectionDescription() const
	{
		return LOCTEXT("NeuralMorphModelProjectSettingsDescription", "Configure project wide settings related to the Neural Morph Model, which is part of the ML Deformer.");
	}
#endif

#undef LOCTEXT_NAMESPACE
