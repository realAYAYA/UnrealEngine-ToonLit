// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/VCamStateSwitcherWidgetFactory.h"

#include "UI/Switcher/VCamStateSwitcherWidget.h"

#define LOCTEXT_NAMESPACE "VCamStateSwitcherWidgetFactory"

UVCamStateSwitcherWidgetFactory::UVCamStateSwitcherWidgetFactory()
{
	ParentClass = UVCamStateSwitcherWidget::StaticClass();
}

FText UVCamStateSwitcherWidgetFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "VCam State Switcher Widget");
}

FText UVCamStateSwitcherWidgetFactory::GetToolTip() const
{
	return LOCTEXT("Tooltip", "Extends VCamWidget with a states.\n\nYou can switch between states using SetCurrentState.\nFor each state you can specify a set of VCam subwidgets (widgets you've added to the state switcher) and connections points those widgets should bind to in the given state.");
}

#undef LOCTEXT_NAMESPACE
