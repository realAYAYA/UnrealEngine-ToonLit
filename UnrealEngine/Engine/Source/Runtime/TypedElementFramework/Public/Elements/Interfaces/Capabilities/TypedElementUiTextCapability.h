// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementUiCapabilities.h"
#include "Internationalization/Text.h"
#include "Misc/Attribute.h"

/**
 * Interface to provide access to widgets that support working with text.
 */
class ITypedElementUiTextCapability : public ITypedElementUiCapability
{
public:
	SLATE_METADATA_TYPE(ITypedElementUiTextCapability, ITypedElementUiCapability)

	~ITypedElementUiTextCapability() override = default;

	virtual void SetText(const TAttribute<FText>& Text) = 0;

	virtual void SetHighlightText(const TAttribute<FText>& Text) = 0;
};

template<typename WidgetType>
class TTypedElementUiTextCapability : public ITypedElementUiTextCapability
{
public:
	explicit TTypedElementUiTextCapability(WidgetType& InWidget) : Widget(InWidget){}
	
	void SetText(const TAttribute<FText>& Text) override
	{
		Widget.SetText(Text);
	}

	void SetHighlightText(const TAttribute<FText>& Text) override
	{
		Widget.SetHighlightText(Text);
	}

private:
	WidgetType& Widget;
};