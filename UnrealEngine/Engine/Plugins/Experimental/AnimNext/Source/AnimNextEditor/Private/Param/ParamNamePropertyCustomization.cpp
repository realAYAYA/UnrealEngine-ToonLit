// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParamNamePropertyCustomization.h"

#include "DetailWidgetRow.h"
#include "EdGraphSchema_K2.h"
#include "EditorUtils.h"
#include "PropertyHandle.h"
#include "SParameterPickerCombo.h"
#include "UncookedOnlyUtils.h"
#include "Param/ParamCompatibility.h"
#include "Param/ParamUtils.h"
#include "RigVMCore/RigVMStruct.h"

#define LOCTEXT_NAMESPACE "ParamNamePropertyCustomization"

namespace UE::AnimNext::Editor
{

static const FProperty* GetMetadataProperty(const FProperty* Property)
{
	if (const FProperty* OuterProperty = Property->GetOwner<FProperty>())
	{
		if (OuterProperty->IsA<FArrayProperty>()
			|| OuterProperty->IsA<FSetProperty>()
			|| OuterProperty->IsA<FMapProperty>())
		{
			return OuterProperty;
		}
	}

	return Property;
}

bool FParamNamePropertyTypeIdentifier::IsPropertyTypeCustomized(const IPropertyHandle& InPropertyHandle) const
{
	const FProperty* Property = GetMetadataProperty(InPropertyHandle.GetProperty());
	if(Property->GetMetaData(FRigVMStruct::CustomWidgetMetaName) == "ParamName")
	{
		return true;
	}

	return false;
}

void FParamNamePropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	WeakPropertyHandle = InPropertyHandle;
	
	const FProperty* Property = GetMetadataProperty(InPropertyHandle->GetProperty());
	const FString ParamTypeString = Property->GetMetaData("AllowedParamType");
	const bool bAllowNone = Property->HasMetaData("AllowNone");
	FAnimNextParamType FilterType = FAnimNextParamType::FromString(ParamTypeString);
	
	FParameterPickerArgs PickerArgs;
	PickerArgs.bShowBlocks = false;
	PickerArgs.bMultiSelect = false;
	PickerArgs.OnFilterParameterType = FOnFilterParameterType::CreateLambda([FilterType](const FAnimNextParamType& InParameterType)
	{
		if(!FilterType.IsValid() || FParamUtils::GetCompatibility(FilterType, InParameterType).IsCompatible())
		{
			return EFilterParameterResult::Include;
		}
		return EFilterParameterResult::Exclude;
	});
	PickerArgs.NewParameterType = FilterType;
	PickerArgs.OnParameterPicked = FOnParameterPicked::CreateLambda([this, WeakPropertyHandle = WeakPropertyHandle](const FParameterBindingReference& InParameterBinding)
	{
		if(TSharedPtr<IPropertyHandle> PropertyHandle = WeakPropertyHandle.Pin())
		{
			PropertyHandle->NotifyPreChange();
			PropertyHandle->SetValue(InParameterBinding.Parameter);
			PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

			Refresh();
		}
	});
	PickerArgs.bAllowNone = bAllowNone;
	
	InHeaderRow
	.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SParameterPickerCombo)
		.PickerArgs(PickerArgs)
		.OnGetParameterName_Lambda([this]()
		{
			return CachedName;
		})
		.OnGetParameterType_Lambda([this]()
		{
			return CachedType;
		})
	];

	Refresh();
}

void FParamNamePropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FParamNamePropertyTypeCustomization::Refresh()
{
	if(TSharedPtr<IPropertyHandle> PropertyHandle = WeakPropertyHandle.Pin())
	{
		if(PropertyHandle->GetValue(CachedName) == FPropertyAccess::Success)
		{
			CachedType = UncookedOnly::FUtils::GetParameterTypeFromName(CachedName);
		}
	}
}

}

#undef LOCTEXT_NAMESPACE