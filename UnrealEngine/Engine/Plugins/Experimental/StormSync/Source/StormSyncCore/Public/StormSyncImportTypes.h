// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "StormSyncCommonTypes.h"
#include "StormSyncPackageDescriptor.h"
#include "StormSyncImportTypes.generated.h"

/**
 * Data holder during an extraction process.
 *
 * Used to compute a list of files that needs extraction.
 */
USTRUCT()
struct FStormSyncImportFileInfo
{
	GENERATED_BODY()
	
	/** Holds info such as PackageName, FileSize, Timestamp and hash*/
	FStormSyncFileDependency FileDependency;

	/** The associated fully qualified path of the .uasset */
	FString DestFilepath;

	/** Holds a shared ptr to the serialized buffer */
	FStormSyncBufferPtr FileBuffer = nullptr;

	/** UI-exposed reason of the import */
	FText ImportReason;
	
	/** UI-exposed extended info for reason of the import */
	FText ImportReasonTooltip;

	FStormSyncImportFileInfo() = default;

	FStormSyncImportFileInfo(const FStormSyncFileDependency& InFileDependency, const FString& InDestFilepath, const FStormSyncBufferPtr& InFileBuffer)
		: FileDependency(InFileDependency)
		, DestFilepath(InDestFilepath)
		, FileBuffer(InFileBuffer)
	{
	}
};
