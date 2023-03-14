// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace MediaIOCoreFileWriter
{
	MEDIAIOCORE_API void WriteRawFile(const FString& InFilename, uint8* InBuffer, uint32 InSize, bool bAppend = true);
};