// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class UStateTree;
class UStateTreeState;
enum class EStateTreeTransitionTrigger : uint8;

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

	EStateTreeTransitionTrigger GetTrigger() const;
	bool GetDelayTransition() const;
	
	TSharedPtr<IPropertyHandle> TriggerProperty;
	TSharedPtr<IPropertyHandle> PriorityProperty;
	TSharedPtr<IPropertyHandle> EventTagProperty;
	TSharedPtr<IPropertyHandle> StateProperty;
	TSharedPtr<IPropertyHandle> DelayTransitionProperty;
	TSharedPtr<IPropertyHandle> DelayDurationProperty;
	TSharedPtr<IPropertyHandle> DelayRandomVarianceProperty;
	TSharedPtr<IPropertyHandle> ConditionsProperty;

	class IPropertyUtilities* PropUtils;
	TSharedPtr<IPropertyHandle> StructProperty;
};
