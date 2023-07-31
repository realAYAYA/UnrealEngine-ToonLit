// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

namespace UE
{
	namespace Interchange
	{
		class FImportResult;

		using FAssetImportResultRef = TSharedRef< FImportResult, ESPMode::ThreadSafe >;
		using FSceneImportResultRef = TSharedRef< FImportResult, ESPMode::ThreadSafe >;
	}
}
