// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IDetailLayoutBuilder;
class SCustomizableInstanceProperties;
class UCustomizableObjectInstance;
class UCustomizableSkeletalComponent;


class FCustomizableInstanceDetails : public IDetailCustomization
{
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it.
	static TSharedRef<IDetailCustomization> MakeInstance();
	
	// ILayoutDetails interface
	/** Do not use. Add details customization in the other CustomizeDetails signature. */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override {};

	/** Customize details here. */
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;
	
	// Own interface	
	/** Refresh the custom details. */
	void Refresh() const;

private:
	TWeakObjectPtr<UCustomizableObjectInstance> CustomInstance;
	TWeakObjectPtr<UCustomizableSkeletalComponent> CustomizableSkeletalComponent;
	TSharedPtr<SCustomizableInstanceProperties> InstancePropertiesWidget;
	
	TWeakPtr<IDetailLayoutBuilder> LayoutBuilder;
};

