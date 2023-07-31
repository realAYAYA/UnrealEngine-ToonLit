// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#if PLATFORM_MAC
#include "CoreMinimal.h"
#include "GenericPlatform/ITextToSpeechFactory.h"

class FTextToSpeechBase;
/** The platform default text to speech factory for Mac. */
class FMacTextToSpeechFactory : public ITextToSpeechFactory
{
public:
	FMacTextToSpeechFactory() = default;
	virtual ~FMacTextToSpeechFactory() = default;

	// ITextToSpeechFactory
	virtual TSharedRef<FTextToSpeechBase> Create() override;
	// ~
};
typedef FMacTextToSpeechFactory FPlatformTextToSpeechFactory;
#endif

