// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#if PLATFORM_IOS
#include "CoreMinimal.h"
#include "GenericPlatform/ITextToSpeechFactory.h"

class FTextToSpeechBase;
/** The platform default text to speech factory for IOS. */
class FIOSTextToSpeechFactory : public ITextToSpeechFactory
{
public:
	FIOSTextToSpeechFactory() = default;
	virtual ~FIOSTextToSpeechFactory() = default;

	virtual TSharedRef<FTextToSpeechBase> Create() override;
};
typedef FIOSTextToSpeechFactory FPlatformTextToSpeechFactory;
#endif

