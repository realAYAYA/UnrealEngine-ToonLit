// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Switcher/WidgetConnectionConfig.h"

#include "UI/Switcher/VCamStateSwitcherWidget.h"

#include "Blueprint/WidgetTree.h"

UVCamWidget* FWidgetConnectionConfig::ResolveWidget(UVCamStateSwitcherWidget* OwnerWidget) const
{
	return OwnerWidget
		? Widget.ResolveVCamWidget(*OwnerWidget)
		: nullptr;
}

bool FWidgetConnectionConfig::HasNoWidgetSet() const
{
	return Widget.HasNoWidgetSet();
}
