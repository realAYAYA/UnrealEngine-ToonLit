// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParamPropertyCustomization.h"

#include "DetailWidgetRow.h"
#include "EdGraphSchema_K2.h"
#include "EditorUtils.h"
#include "PropertyHandle.h"
#include "SParameterPickerCombo.h"
#include "Param/AnimNextParam.h"
#include "Param/ParamCompatibility.h"
#include "Param/ParamUtils.h"

#define LOCTEXT_NAMESPACE "ParamPropertyCustomization"

namespace UE::AnimNext::Editor
{

void FParamPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	auto GetMetadataProperty = [](const FProperty* InProperty)
	{
		if (const FProperty* OuterProperty = InProperty->GetOwner<FProperty>())
		{
			if (OuterProperty->IsA<FArrayProperty>()
				|| OuterProperty->IsA<FSetProperty>()
				|| OuterProperty->IsA<FMapProperty>())
			{
				return OuterProperty;
			}
		}

		return InProperty;
	};

	PropertyHandle = InPropertyHandle;
	NamePropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAnimNextParam, Name));
	TypePropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAnimNextParam, Type));

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
	PickerArgs.OnParameterPicked = FOnParameterPicked::CreateLambda([this](const FParameterBindingReference& InParameterBinding)
	{
		if(PropertyHandle && NamePropertyHandle && TypePropertyHandle)
		{
			PropertyHandle->NotifyPreChange();
			NamePropertyHandle->SetValue(InParameterBinding.Parameter);
			TArray<void*> TypeValues;
			TypePropertyHandle->AccessRawData(TypeValues);
			for(void* TypePtr : TypeValues)
			{
				FAnimNextParamType& Type = *static_cast<FAnimNextParamType*>(TypePtr);
				Type = InParameterBinding.Type;
			}
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

void FParamPropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FParamPropertyTypeCustomization::Refresh()
{
	TArray<void*> TypeValues;
	TypePropertyHandle->AccessRawData(TypeValues);
	if(TypeValues.Num() == 1 && TypeValues[0] != nullptr)
	{
		CachedType = *static_cast<FAnimNextParamType*>(TypeValues[0]);
	}
	NamePropertyHandle->GetValue(CachedName);
}

}

#undef LOCTEXT_NAMESPACE