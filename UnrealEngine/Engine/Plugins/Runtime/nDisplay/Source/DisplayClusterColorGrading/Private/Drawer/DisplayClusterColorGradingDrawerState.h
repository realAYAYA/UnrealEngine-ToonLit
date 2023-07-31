// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "SDisplayClusterColorGradingColorWheel.h"

/** Drawer modes that the color grading drawer can be in */
enum EDisplayClusterColorGradingDrawerMode
{
	ColorGrading,
	DetailsView
};

/** Stores the state of the drawer UI that can be reloaded in cases where the drawer or any of its elements are reloaded (such as when the drawer is reopened or docked) */
struct FDisplayClusterColorGradingDrawerState
{
	/** The objects that are selected in the list */
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;

	/** The color grading group that is selected */
	int32 SelectedColorGradingGroup = INDEX_NONE;

	/** The color grading element that is selected */
	int32 SelectedColorGradingElement = INDEX_NONE;

	/** Indicates which color wheels are hidden */
	TArray<bool> HiddenColorWheels;

	/** The selected orientation of the color wheels */
	EOrientation ColorWheelOrientation = EOrientation::Orient_Vertical;

	/** The color display mode of the color wheels */
	SDisplayClusterColorGradingColorWheel::EColorDisplayMode ColorDisplayMode;

	/** The mode the drawer is in */
	EDisplayClusterColorGradingDrawerMode DrawerMode;

	/** Indicates which subsections were selected for each section in the details panel */
	TArray<int32> SelectedDetailsSubsections;
};