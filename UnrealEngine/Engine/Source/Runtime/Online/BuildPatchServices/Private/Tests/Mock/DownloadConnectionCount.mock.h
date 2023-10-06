// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/DownloadConnectionCount.h"

enum class EBuildPatchDownloadHealth;

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockDownloadConnectionCount : public IDownloadConnectionCount
	{
	public:
		FMockDownloadConnectionCount(uint32 InCount)
			:Count(InCount)
		{

		}
		FMockDownloadConnectionCount()
			:Count(8)
		{

		}
		uint32 GetAdjustedCount(uint32 NumProcessing, EBuildPatchDownloadHealth CurrentHealth) { return Count; }
	private:
		const uint32 Count;
	};
}

#endif  //WITH_DEV_AUTOMATION_TESTS