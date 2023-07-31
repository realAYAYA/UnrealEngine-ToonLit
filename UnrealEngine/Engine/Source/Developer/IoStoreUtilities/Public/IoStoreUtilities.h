// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"

struct FGuid;
struct FKeyChain;

IOSTOREUTILITIES_API int32 CreateIoStoreContainerFiles(const TCHAR* CmdLine);

IOSTOREUTILITIES_API bool DumpIoStoreContainerInfo(const TCHAR* InContainerFilename, const FKeyChain& InKeyChain);

IOSTOREUTILITIES_API bool LegacyListIoStoreContainer(
	const TCHAR* InContainerFilename,
	int64 InSizeFilter,
	const FString& InCSVFilename,
	const FKeyChain& InKeyChain);

IOSTOREUTILITIES_API bool LegacyDiffIoStoreContainers(
	const TCHAR* InContainerFilename1,
	const TCHAR* InContainerFilename2,
	bool bInLogUniques1,
	bool bInLogUniques2,
	const FKeyChain& InKeyChain);

IOSTOREUTILITIES_API bool ExtractFilesFromIoStoreContainer(
	const TCHAR* InContainerFilename,
	const TCHAR* InDestPath,
	const FKeyChain& InKeyChain,
	const FString* InFilter,
	TMap<FString, uint64>* OutOrderMap,
	TArray<FGuid>* OutUsedEncryptionKeys,
	bool* bOutIsSigned);
