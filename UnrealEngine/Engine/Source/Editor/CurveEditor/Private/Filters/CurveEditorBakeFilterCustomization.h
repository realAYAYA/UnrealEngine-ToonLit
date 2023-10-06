// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class UCurveEditorBakeFilter;

class IPropertyHandle;

/**
 * Customizes a UCurveEditorBakeFilter
 */
class FCurveEditorBakeFilterCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FCurveEditorBakeFilterCustomization>();
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
	// End IDetailCustomization interface
};