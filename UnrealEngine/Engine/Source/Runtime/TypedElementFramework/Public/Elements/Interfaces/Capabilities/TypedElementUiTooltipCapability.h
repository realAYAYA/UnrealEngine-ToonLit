// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementUiCapabilities.h"
#include "Internationalization/Text.h"
#include "Misc/Attribute.h"
#include "Widgets/IToolTip.h"

/**
 * Interface to provide access to tool tips on widgets.
 */
class ITypedElementUiTooltipCapability : public ITypedElementUiCapability
{
public:
	SLATE_METADATA_TYPE(ITypedElementUiTooltipCapability, ITypedElementUiCapability)

	~ITypedElementUiTooltipCapability() override = default;

	virtual void SetToolTipText(const TAttribute<FText>& ToolTipText) = 0;
	virtual void SetToolTipText(const FText& ToolTipText) = 0;
	virtual void SetToolTip(const TAttribute<TSharedPtr<IToolTip>>& ToolTip) = 0;
	virtual TSharedPtr<IToolTip> GetToolTip() = 0;

	virtual void EnableToolTipForceField(const bool bEnableForceField) = 0;
	virtual bool HasToolTipForceField() const = 0;
};

template<typename WidgetType>
class TTypedElementUiTooltipCapability : public ITypedElementUiTooltipCapability
{
public:
	explicit TTypedElementUiTooltipCapability(WidgetType& InWidget) : Widget(InWidget){}

	void SetToolTipText(const TAttribute<FText>& ToolTipText) override
	{
		Widget.SetToolTipText(ToolTipText);	
	}

	void SetToolTipText(const FText& ToolTipText) override
	{
		Widget.SetToolTipText(ToolTipText);
	}

	void SetToolTip(const TAttribute<TSharedPtr<IToolTip>>& ToolTip) override
	{
		Widget.SetToolTip(ToolTip);
	}

	TSharedPtr<IToolTip> GetToolTip() override
	{
		return Widget.GetToolTip();
	}

	void EnableToolTipForceField(const bool bEnableForceField) override
	{
		Widget.EnableToolTipForceField(bEnableForceField);
	}

	bool HasToolTipForceField() const override
	{
		return Widget.HasToolTipForceField();
	}

private:
	WidgetType& Widget;
};