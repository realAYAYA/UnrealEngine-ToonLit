// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FString;

namespace CryptoKeys
{
	CRYPTOKEYS_API void GenerateEncryptionKey(FString& OutBase64Key);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Modules/ModuleManager.h"
#endif
