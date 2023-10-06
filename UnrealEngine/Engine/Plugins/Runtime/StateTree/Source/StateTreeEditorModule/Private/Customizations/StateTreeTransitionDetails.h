// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
class IDetailChildrenBuilder;
class FDetailWidgetRow;
class UStateTreeEditorData;
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
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	UStateTreeEditorData* GetEditorData() const;

	FText GetDescription() const;

	EStateTreeTransitionTrigger GetTrigger() const;
	bool GetDelayTransition() const;

	void OnCopyTransition() const;
	void OnPasteTransition() const;

	TSharedPtr<IPropertyHandle> TriggerProperty;
	TSharedPtr<IPropertyHandle> PriorityProperty;
	TSharedPtr<IPropertyHandle> EventTagProperty;
	TSharedPtr<IPropertyHandle> StateProperty;
	TSharedPtr<IPropertyHandle> DelayTransitionProperty;
	TSharedPtr<IPropertyHandle> DelayDurationProperty;
	TSharedPtr<IPropertyHandle> DelayRandomVarianceProperty;
	TSharedPtr<IPropertyHandle> ConditionsProperty;
	TSharedPtr<IPropertyHandle> IDProperty;

	TSharedPtr<IPropertyUtilities> PropUtils;
	TSharedPtr<IPropertyHandle> StructProperty;
};
