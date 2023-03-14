// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if ELECTRA_HTTPSTREAM_APPLE

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "ParameterDictionary.h"

class IElectraHTTPStream;

class ELECTRAHTTPSTREAM_API FPlatformElectraHTTPStreamApple
{
public:
	static void Startup();
	static void Shutdown();
	static TSharedPtr<IElectraHTTPStream, ESPMode::ThreadSafe> Create(const Electra::FParamDict& InOptions);
};

typedef FPlatformElectraHTTPStreamApple FPlatformElectraHTTPStream;

#endif
