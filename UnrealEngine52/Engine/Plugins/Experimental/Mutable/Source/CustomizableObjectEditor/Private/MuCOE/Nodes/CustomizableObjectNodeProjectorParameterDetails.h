// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class FReply;
namespace ESelectInfo { enum Type : int; }

class FString;
class IDetailLayoutBuilder;
class IPropertyHandle;


class FCustomizableObjectNodeProjectorParameterDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;


private:
	TArray< TSharedPtr<FString> > BoneComboOptions;

	class UCustomizableObjectNodeProjectorConstant* NodeConstant;
	class UCustomizableObjectNodeProjectorParameter* NodeParameter;
	FReply OnProjectorCopyPressed();
	FReply OnProjectorPastePressed();

	class USkeletalMesh* SkeletalMesh;
	class IDetailLayoutBuilder* DetailBuilderPtr;

	void OnBoneComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> BoneProperty);
	void OnReferenceSkeletonIndexChanged();
};
