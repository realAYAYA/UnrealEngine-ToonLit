// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterColorGradingStyle.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

/** Command list for the color grading drawer */
class FDisplayClusterColorGradingCommands
	: public TCommands<FDisplayClusterColorGradingCommands>
{
public:
	FDisplayClusterColorGradingCommands()
		: TCommands<FDisplayClusterColorGradingCommands>(TEXT("DisplayClusterColorGrading"),
			NSLOCTEXT("Contexts", "DisplayClusterColorGrading", "Display Cluster Color Grading"), NAME_None, FDisplayClusterColorGradingStyle::Get().GetStyleSetName())
	{ }

	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> SaturationColorWheelVisibility;
	TSharedPtr<FUICommandInfo> ContrastColorWheelVisibility;
	TSharedPtr<FUICommandInfo> ColorWheelSliderOrientationHorizontal;
	TSharedPtr<FUICommandInfo> ColorWheelSliderOrientationVertical;

	TSharedPtr<FUICommandInfo> ColorGradingMode;
	TSharedPtr<FUICommandInfo> DetailViewMode;

	TSharedPtr<FUICommandInfo> OpenColorGradingDrawer;
};