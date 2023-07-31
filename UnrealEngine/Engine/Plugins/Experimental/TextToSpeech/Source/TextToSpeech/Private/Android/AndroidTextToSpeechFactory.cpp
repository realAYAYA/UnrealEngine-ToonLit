// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_ANDROID
#include "Android/AndroidTextToSpeechFactory.h"
#include "GenericPlatform/ITextToSpeechFactory.h"
#include "GenericPlatform/TextToSpeechBase.h"
#include "Flite/FliteTextToSpeech.h"

TSharedRef<FTextToSpeechBase> FAndroidTextToSpeechFactory::Create()
{
	return MakeShared<FFliteTextToSpeech>();
}
#endif
