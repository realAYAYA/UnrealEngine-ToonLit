// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if ELECTRA_HTTPSTREAM_GENERIC_UE

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "ParameterDictionary.h"

class IElectraHTTPStream;

class ELECTRAHTTPSTREAM_API FPlatformElectraHTTPStreamGeneric
{
public:
	static void Startup();
	static void Shutdown();
	static TSharedPtr<IElectraHTTPStream, ESPMode::ThreadSafe> Create(const Electra::FParamDict& InOptions);
};

typedef FPlatformElectraHTTPStreamGeneric FPlatformElectraHTTPStream;

#endif // ELECTRA_HTTPSTREAM_GENERIC_UE
