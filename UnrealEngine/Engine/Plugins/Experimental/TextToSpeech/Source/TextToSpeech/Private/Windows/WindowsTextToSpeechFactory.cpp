// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_WINDOWS
#include "Windows/WindowsTextToSpeechFactory.h"
#include "GenericPlatform/ITextToSpeechFactory.h"
#include "GenericPlatform/TextToSpeechBase.h"
#include "Flite/FliteTextToSpeech.h"

TSharedRef<FTextToSpeechBase> FWindowsTextToSpeechFactory::Create()
{
	return MakeShared<FFliteTextToSpeech>();
}

#endif
