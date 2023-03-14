// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class IPropertyHandle;
class IPropertyHandleArray;
class SBoneSelectionWidget;

class FMeshReshapeBonesReferenceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization interface
	void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {};
	
	// Gets the Reference Skeleton from the target skeleton
	const struct FReferenceSkeleton& GetReferenceSkeleton() const;
	// Gets the current selected bone in the combo button
	FName GetSelectedBone(bool& bMultipleValues) const;
	// OnSelectionChanged callback
	void OnBoneSelectionChanged(FName Name);

private:

	class USkeletalMesh* GetSkeletalMesh(TSharedPtr<IPropertyHandle> BoneNamePropertyHandle) const; 

private:

	// Property to change after bone has been picked
	TSharedPtr<IPropertyHandle> BoneNameProperty;

	// Parent propery, as array to identify repeated values.
	TSharedPtr<IPropertyHandleArray> ParentArrayProperty;

	TSharedPtr<SBoneSelectionWidget> SelectionWidget;
	FLinearColor DefaultColor;
};
