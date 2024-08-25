// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundBaseDetails.h"

#include "DetailLayoutBuilder.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundBase.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

TSharedRef<IDetailCustomization> FSoundBaseDetails::MakeInstance()
{
	return MakeShareable(new FSoundBaseDetails);
}

void FSoundBaseDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!FModuleManager::Get().IsModuleLoaded("AudioProperties"))
	{
		DetailBuilder.HideCategory("AudioProperties");
	}
}
