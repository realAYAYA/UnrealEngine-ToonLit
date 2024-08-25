// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneSettingsCustomization.h"
#include "AvaSceneSettings.h"
#include "DetailLayoutBuilder.h"
#include "IAvaAttributeEditorModule.h"

TSharedRef<IDetailCustomization> FAvaSceneSettingsCustomization::MakeInstance()
{
	return MakeShared<FAvaSceneSettingsCustomization>();
}

void FAvaSceneSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	if (TSharedPtr<IPropertyHandle> AttributesHandle = InDetailBuilder.GetProperty(UAvaSceneSettings::GetSceneAttributesName()))
	{
		IAvaAttributeEditorModule::Get().CustomizeAttributes(AttributesHandle.ToSharedRef(), InDetailBuilder);	
	}
}
