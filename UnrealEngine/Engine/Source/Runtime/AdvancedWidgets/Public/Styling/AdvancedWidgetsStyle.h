// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/PropertyViewer/FieldIconFinder.h"

class ISlateStyle;
class FSlateStyleSet;

namespace UE::AdvancedWidgets
{
/**
 * Style for advanced widgets
 */
class ADVANCEDWIDGETS_API FAdvancedWidgetsStyle
{
public:
	static void Create();
	static void Destroy();
	static const ISlateStyle& Get();

	static const ::UE::PropertyViewer::FFieldColorSettings& GetColorSettings()
	{
		return ColorSettings;
	}

private:
	/** Singleton instances of this style. */
	static TUniquePtr<FSlateStyleSet> Instance;

	/**  */
	static ::UE::PropertyViewer::FFieldColorSettings ColorSettings;
};

} // namespace