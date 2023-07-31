// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IDetailCustomization.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"

class FString;
class IDetailLayoutBuilder;
class IPropertyHandle;


class FCustomizableObjectNodeMeshClipMorphDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	class UCustomizableObjectNodeMeshClipMorph* Node;
	class IDetailLayoutBuilder* DetailBuilderPtr = nullptr;
	TArray< TSharedPtr<FString> > BoneComboOptions;

	UPROPERTY()
	class USkeletalMesh* SkeletalMesh;

	void OnBoneComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> BoneProperty);

	void OnInvertNormalCheckboxChanged(ECheckBoxState CheckBoxState);
	ECheckBoxState GetInvertNormalCheckBoxState() const;

	void OnReferenceSkeletonIndexChanged();
};
