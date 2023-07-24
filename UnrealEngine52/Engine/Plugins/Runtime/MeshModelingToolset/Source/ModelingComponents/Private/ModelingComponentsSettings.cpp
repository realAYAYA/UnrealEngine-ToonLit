// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingComponentsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModelingComponentsSettings)


#define LOCTEXT_NAMESPACE "ModelingComponentsSettings"

#if WITH_EDITOR
FText UModelingComponentsSettings::GetSectionText() const 
{ 
	return LOCTEXT("ModelingComponentsProjectSettingsName", "Modeling Mode Tools"); 
}

FText UModelingComponentsSettings::GetSectionDescription() const
{
	return LOCTEXT("ModelingComponentsProjectSettingsDescription", "Configure Tool-level Settings for the Modeling Tools Editor Mode plugin");
}

FText UModelingComponentsEditorSettings::GetSectionText() const 
{ 
	return LOCTEXT("ModelingComponentsEditorSettingsName", "Modeling Mode Tools"); 
}

FText UModelingComponentsEditorSettings::GetSectionDescription() const
{
	return LOCTEXT("ModelingComponentsEditorSettingsDescription", "Configure Tool-level Settings for the Modeling Tools Editor Mode plugin");
}
#endif


#undef LOCTEXT_NAMESPACE
