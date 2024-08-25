// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
class UStateTree;
class UStateTreeState;
class UStateTreeEditorData;

/**
* Type customization for FStateTreeStateParameters.
*/

class FStateTreeStateParametersDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	void FindOuterObjects();

	TSharedPtr<IPropertyUtilities> PropUtils;

	TSharedPtr<IPropertyHandle> ParametersProperty;
	TSharedPtr<IPropertyHandle> FixedLayoutProperty;
	TSharedPtr<IPropertyHandle> IDProperty;
	TSharedPtr<IPropertyHandle> StructProperty;

	bool bFixedLayout = false;

	TWeakObjectPtr<UStateTreeEditorData> WeakEditorData = nullptr;
	TWeakObjectPtr<UStateTree> WeakStateTree = nullptr;
	TWeakObjectPtr<UStateTreeState> WeakState = nullptr;
};
