// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class ICustomDetailsViewItem;
class FText;
class SWidget;

class ICustomDetailsViewCustomItem
{
public:
	virtual ~ICustomDetailsViewCustomItem() = default;

	/** Sets the text that will appear in the name column. */
	virtual void SetLabel(const FText& InLabel) = 0;

	/** Sets the tooltip that will appear in the name column. */
	virtual void SetToolTip(const FText& InToolTip) = 0;

	/** Sets the widget that will appear in the value column. */
	virtual void SetValueWidget(const TSharedRef<SWidget>& InValueWidget)  = 0;

	virtual void SetExtensionWidget(const TSharedRef<SWidget>& InExpansionWidget) = 0;

	virtual TSharedRef<ICustomDetailsViewItem> AsItem() = 0;
};
