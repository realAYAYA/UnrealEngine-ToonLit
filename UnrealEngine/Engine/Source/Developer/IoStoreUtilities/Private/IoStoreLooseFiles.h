// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class IIoStoreWriter;

struct FLooseFilesWriterSettings
{
	FString TocFilePath;
	FString TargetRootPath;
	FName ContainerName;
};

TSharedPtr<IIoStoreWriter> MakeLooseFilesIoStoreWriter(const FLooseFilesWriterSettings& WriterSettings,
	uint32 MaxConcurrentWrites = 64);
