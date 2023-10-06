// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundBaseDetails.h"

#include "DetailLayoutBuilder.h"
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
	// Does nothing currently
}
