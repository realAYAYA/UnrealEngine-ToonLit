// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyHandleArray;
class SBoneSelectionWidget;
class UCustomizableObjectNodeObject;

struct FReferenceSkeleton;

enum class ECheckBoxState : uint8;

class FCustomizableObjectLODReductionSettings : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization interface
	void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {};

private:

	// Gets the Reference Skeleton from the target skeleton
	const FReferenceSkeleton& GetReferenceSkeleton() const;
	
	// Gets the current selected bone in the combo button
	FName GetSelectedBone(bool& bMultipleValues) const;
	
	// OnSelectionChanged callback to return the selected bone
	void OnBoneSelectionChanged(FName Name);
	
	// OnSelectionChanged callback 
	void OnBoneCheckBoxChanged(ECheckBoxState NewState);

	// Returns the object node of the property represented in the widget
	UCustomizableObjectNodeObject* GetObjectNode();

private:

	// Pointer to the object node
	UCustomizableObjectNodeObject* ObjectNode;

	// Property to change after bone has been picked
	TSharedPtr<IPropertyHandle> BoneNameProperty;
	TSharedPtr<IPropertyHandle> IncludeBoneProperty;

	// Parent propery, as array to identify repeated values.
	TSharedPtr<IPropertyHandleArray> ParentArrayProperty;

	// Widget to select the bone to remove
	TSharedPtr<SBoneSelectionWidget> SelectionWidget;

	// Color of the bone name
	FLinearColor DefaultColor;
};
