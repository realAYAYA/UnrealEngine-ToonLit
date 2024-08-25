// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if ELECTRA_HTTPSTREAM_GENERIC_UE
#include "Generic/GenericElectraHTTPStream.h"
#else
#include COMPILED_PLATFORM_HEADER(PlatformElectraHTTPStream.h)
#endif
