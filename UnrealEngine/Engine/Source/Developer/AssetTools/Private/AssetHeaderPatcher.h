// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/Task.h"
#include "Containers/ContainersFwd.h"

struct FAssetHeaderPatcher
{
	enum class EResult
	{
		Success,
		ErrorFailedToLoadSourceAsset,
		ErrorFailedToDeserializeSourceAsset,
		ErrorUnexpectedSectionOrder,
		ErrorBadOffset,
		ErrorUnkownSection,
		ErrorFailedToOpenDestinationFile,
		ErrorFailedToWriteToDestinationFile,
		ErrorEmptyRequireSection,
	};

	static EResult DoPatch(const FString& InSrcAsset, const FString& InDstAsse, const TMap<FString, FString>& InSearchAndReplace, bool bInStarRestrictionInUse = false);

	static EResult Test_DoPatch(FArchive& InSrcReader, FArchive& InDstWriter, const TMap<FString, FString>& InSearchAndReplace);
};
