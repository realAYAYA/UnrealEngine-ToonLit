// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace BuildPatchServices
{
	class IDataSizeProvider
	{
	public:
		virtual ~IDataSizeProvider() {}

		virtual uint64 GetDownloadSize(const FString& Uri) const = 0;
	};
}