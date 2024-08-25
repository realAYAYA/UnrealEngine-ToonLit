// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementUiCapabilities.h"
#include "Misc/Attribute.h"

/**
 * Interface to provide access to widget that have explicit editing modes.
 */
class ITypedElementUiEditableCapability : public ITypedElementUiCapability
{
public:
	SLATE_METADATA_TYPE(ITypedElementUiEditableCapability, ITypedElementUiCapability)

	~ITypedElementUiEditableCapability() override = default;

	virtual void EnterEditingMode() = 0;
	virtual void ExitEditingMode() = 0;
	virtual void SetIsReadOnly(const TAttribute<bool>& InIsReadOnly) = 0;
};

template<typename WidgetType>
class TTypedElementUiEditableCapability : public ITypedElementUiEditableCapability
{
public:
	explicit TTypedElementUiEditableCapability(WidgetType& InWidget) : Widget(InWidget){}
	
	void EnterEditingMode() override 
	{ 
		Widget.EnterEditingMode();
	}
	void ExitEditingMode() override
	{
		Widget.ExitEditingMode();
	}
	
	void SetIsReadOnly(const TAttribute<bool>& InIsReadOnly) override
	{
		Widget.SetReadOnly(InIsReadOnly);
	}

private:
	WidgetType& Widget;
};