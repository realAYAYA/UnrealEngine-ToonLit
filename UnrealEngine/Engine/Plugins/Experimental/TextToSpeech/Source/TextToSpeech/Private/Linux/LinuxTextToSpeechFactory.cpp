// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_LINUX
#include "Linux/LinuxTextToSpeechFactory.h"
#include "Flite/FliteTextToSpeech.h"

TSharedRef<FTextToSpeechBase> FLinuxTextToSpeechFactory::Create()
{
	return MakeShared<FFliteTextToSpeech>();
}
#endif
