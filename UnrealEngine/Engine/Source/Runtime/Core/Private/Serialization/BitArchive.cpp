// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/BitArchive.h"

// Globals

TAutoConsoleVariable<int32> CVarMaxNetStringSize(TEXT("net.MaxNetStringSize"), 16 * 1024 * 1024, TEXT("Maximum allowed size for strings sent/received by the netcode (in bytes)."));

