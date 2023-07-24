// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/ObjectPtr.h"

class UCustomizableObjectNodeGroupProjectorParameter;
class UEdGraphPin;

class FCustomizableObjectNodeGroupProjectorParameterDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// ILayoutDetails interface
	// Do not use. Add details customization in the other CustomizeDetails signature.
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override {}

	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;

private:
	TObjectPtr<UCustomizableObjectNodeGroupProjectorParameter> Node;

	TWeakPtr<IDetailLayoutBuilder> LayoutBuilder;

	void OnPinConnectionListChanged(UEdGraphPin* Pin);
};