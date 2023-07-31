// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_MAC
#include "Mac/MacTextToSpeechFactory.h"
#include "GenericPlatform/ITextToSpeechFactory.h"
#include "Mac/MacTextToSpeech.h"

TSharedRef<FTextToSpeechBase> FMacTextToSpeechFactory::Create()
{
	return MakeShared<FMacTextToSpeech>();
}

#endif
