// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomBindingDetailsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "GroomBindingAsset.h"
#include "GroomCreateBindingOptions.h"

// Customization for UGroomBindingAsset
void FGroomBindingDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder)
{
	TSharedRef<IPropertyHandle> GroomBindingType = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomBindingAsset, GroomBindingType));
	
	uint8 EnumValue;
	GroomBindingType->GetValue(EnumValue);
	if (EnumValue == (uint8) EGroomBindingMeshType::SkeletalMesh)
	{
		LayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UGroomBindingAsset, SourceGeometryCache));
		LayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UGroomBindingAsset, TargetGeometryCache));
	}
	else
	{
		LayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UGroomBindingAsset, SourceSkeletalMesh));
		LayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UGroomBindingAsset, TargetSkeletalMesh));
	}
}

TSharedRef<IDetailCustomization> FGroomBindingDetailsCustomization::MakeInstance()
{
	return MakeShared<FGroomBindingDetailsCustomization>();
}

// Customization for UGroomCreateBindingOptions
void FGroomCreateBindingDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder)
{
	TSharedRef<IPropertyHandle> GroomBindingType = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomCreateBindingOptions, GroomBindingType));

	uint8 EnumValue;
	GroomBindingType->GetValue(EnumValue);
	if (EnumValue == (uint8) EGroomBindingMeshType::SkeletalMesh)
	{
		LayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UGroomCreateBindingOptions, SourceGeometryCache));
		LayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UGroomCreateBindingOptions, TargetGeometryCache));
	}
	else
	{
		LayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UGroomCreateBindingOptions, SourceSkeletalMesh));
		LayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UGroomCreateBindingOptions, TargetSkeletalMesh));
	}

	FSimpleDelegate OnImportTypeChangedDelegate = FSimpleDelegate::CreateSP(this, &FGroomCreateBindingDetailsCustomization::OnGroomBindingTypeChanged, &LayoutBuilder);
	GroomBindingType->SetOnPropertyValueChanged(OnImportTypeChangedDelegate);
}

TSharedRef<IDetailCustomization> FGroomCreateBindingDetailsCustomization::MakeInstance()
{
	return MakeShared<FGroomCreateBindingDetailsCustomization>();
}

void FGroomCreateBindingDetailsCustomization::OnGroomBindingTypeChanged(IDetailLayoutBuilder* LayoutBuilder)
{
	LayoutBuilder->ForceRefreshDetails();
}
