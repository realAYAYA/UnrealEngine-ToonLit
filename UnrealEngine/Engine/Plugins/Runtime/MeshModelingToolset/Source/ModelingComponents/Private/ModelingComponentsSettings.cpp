// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingComponentsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModelingComponentsSettings)


#define LOCTEXT_NAMESPACE "ModelingComponentsSettings"

#if WITH_EDITOR
FText UModelingComponentsSettings::GetSectionText() const 
{ 
	return LOCTEXT("ModelingComponentsSettingsName", "Modeling Mode Tools"); 
}

FText UModelingComponentsSettings::GetSectionDescription() const
{
	return LOCTEXT("ModelingComponentsSettingsDescription", "Configure Tool-level Settings for the Modeling Tools Editor Mode plugin");
}
#endif


#undef LOCTEXT_NAMESPACE
