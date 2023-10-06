// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyle.h"
#include "Styling/ToolBarStyle.h"


/** Styleset for the nDisplay color grading UI elements */
class FDisplayClusterColorGradingStyle final : public FSlateStyleSet
{
public:

	FDisplayClusterColorGradingStyle()
		: FSlateStyleSet("DisplayClusterColorGradingStyle")
	{
		const FVector2D Icon16x16(16.0f, 16.0f);

		SetParentStyleName(FAppStyle::GetAppStyleSetName());

		// Set miscellaneous icons
		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Runtime/nDisplay/Content/Icons/"));
		SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

		Set("ColorGradingDrawer.Icon", new IMAGE_BRUSH_SVG("Components/nDisplayCamera", Icon16x16));
		Set("DisplayClusterColorGrading.ColorGradingMode", new IMAGE_BRUSH_SVG("OperatorPanel/Colors", Icon16x16));
		Set("DisplayClusterColorGrading.DetailViewMode", new IMAGE_BRUSH_SVG("OperatorPanel/Sliders", Icon16x16));
		Set("ColorGradingDrawer.Viewports", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Viewports", Icon16x16));
		Set("ColorGradingDrawer.Nodes", new IMAGE_BRUSH_SVG("Cluster/ClusterNode", Icon16x16));

		// Ultra slim toolbar style with smaller icons and smaller padding for the color grading drawer toolbars
		FToolBarStyle UltraSlimToolbarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar");

		UltraSlimToolbarStyle
			.SetBackgroundPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.SetIconSize(Icon16x16);

		UltraSlimToolbarStyle.ToggleButton
			.SetPadding(FMargin(16.0f, 2.0f));

		Set("UltraSlimToolBar", UltraSlimToolbarStyle);

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	virtual ~FDisplayClusterColorGradingStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

	static FDisplayClusterColorGradingStyle& Get()
	{
		static FDisplayClusterColorGradingStyle Inst;
		return Inst;
	}
};