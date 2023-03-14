// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class UStateTree;
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

	class IPropertyUtilities* PropUtils = nullptr;

	TSharedPtr<IPropertyHandle> ParametersProperty;
	TSharedPtr<IPropertyHandle> FixedLayoutProperty;
	TSharedPtr<IPropertyHandle> IDProperty;
	TSharedPtr<IPropertyHandle> StructProperty;

	bool bFixedLayout = false;

	UStateTreeEditorData* EditorData = nullptr;
	UStateTree* StateTree = nullptr;
};
