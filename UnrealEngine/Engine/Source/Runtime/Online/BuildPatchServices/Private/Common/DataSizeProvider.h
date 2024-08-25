// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/ContainersFwd.h"

namespace BuildPatchServices
{
	class IDataSizeProvider
	{
	public:
		virtual ~IDataSizeProvider() {}

		virtual uint64 GetDownloadSize(const FString& Uri) const = 0;
		virtual void GetDownloadSize(TConstArrayView<FString> InUris, TArray<uint64>& OutSizes) const = 0;
	};
}