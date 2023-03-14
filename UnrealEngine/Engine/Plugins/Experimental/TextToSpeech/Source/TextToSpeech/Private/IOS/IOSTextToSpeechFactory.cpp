// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_IOS
#include "IOS/IOSTextToSpeechFactory.h"
#include "GenericPlatform/ITextToSpeechFactory.h"
#include "IOS/IOSTextToSpeech.h"

TSharedRef<FTextToSpeechBase> FIOSTextToSpeechFactory::Create()
{
	return MakeShared<FIOSTextToSpeech>();
}

#endif
