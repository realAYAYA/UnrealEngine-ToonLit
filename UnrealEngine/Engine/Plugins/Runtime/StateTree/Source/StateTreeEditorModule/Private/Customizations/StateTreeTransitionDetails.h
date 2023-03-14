// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClient.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class UStateTree;
class UStateTreeState;

/**
 * Type customization for FStateTreeTransition.
 */

class FStateTreeTransitionDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	FText GetDescription() const;

	TSharedPtr<IPropertyHandle> TriggerProperty;
	TSharedPtr<IPropertyHandle> EventTagProperty;
	TSharedPtr<IPropertyHandle> StateProperty;
	TSharedPtr<IPropertyHandle> GateDelayProperty;
	TSharedPtr<IPropertyHandle> ConditionsProperty;

	class IPropertyUtilities* PropUtils;
	TSharedPtr<IPropertyHandle> StructProperty;
};
