// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::DisplayClusterDisplayDeviceStrings
{
	namespace material
	{
		namespace asset
		{
			static constexpr const TCHAR* mesh = TEXT("/nDisplay/Materials/Preview/M_DisplayDeviceMesh");

			static constexpr const TCHAR* preview_mesh = TEXT("/nDisplay/Materials/Preview/M_DisplayDevicePreview");
			static constexpr const TCHAR* preview_techvis_mesh = TEXT("/nDisplay/Materials/Preview/M_DisplayDevicePreviewTechvis");
		}

		namespace attr
		{
			static constexpr const TCHAR* Preview = TEXT("Preview");
			static constexpr const TCHAR* Opacity = TEXT("Opacity");
			static constexpr const TCHAR* Exposure = TEXT("Exposure");
			static constexpr const TCHAR* Gamma = TEXT("Gamma");
		}
	}
};
