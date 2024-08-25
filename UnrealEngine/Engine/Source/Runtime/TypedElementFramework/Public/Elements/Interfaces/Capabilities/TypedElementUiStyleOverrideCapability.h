// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementUiCapabilities.h"
#include "Misc/Attribute.h"
#include "Math/Color.h"

/**
 * Interface to provide access to widgets that support working with style overrides.
 */
class ITypedElementUiStyleOverrideCapability : public ITypedElementUiCapability
{
public:
	SLATE_METADATA_TYPE(ITypedElementUiStyleOverrideCapability, ITypedElementUiCapability)

	~ITypedElementUiStyleOverrideCapability() override = default;

	virtual void SetColorAndOpacity(const TAttribute<FLinearColor>& InColorAndOpacity) = 0;
};

template<typename WidgetType>
class TTypedElementUiStyleOverrideCapability : public ITypedElementUiStyleOverrideCapability
{
public:
	explicit TTypedElementUiStyleOverrideCapability(WidgetType& InWidget) : Widget(InWidget){}
	
	void SetColorAndOpacity(const TAttribute<FLinearColor>& InColorAndOpacity) override
	{
		Widget.SetColorAndOpacity(InColorAndOpacity);
	}

private:
	WidgetType& Widget;
};