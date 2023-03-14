// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMPropertyPathCustomization.h"

#include "Algo/RemoveIf.h"
#include "Algo/Transform.h"
#include "Bindings/MVVMBindingHelper.h"
#include "BlueprintEditor.h"
#include "Components/Widget.h"
#include "DetailWidgetRow.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMSubsystem.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "PropertyHandle.h"
#include "SSimpleButton.h"
#include "Templates/UnrealTemplate.h"
#include "WidgetBlueprint.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SComboBox.h" 
#include "Widgets/SMVVMFieldIcon.h"
#include "Widgets/SMVVMFieldSelector.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MVVMPropertyPathCustomization"

namespace UE::MVVM
{

FPropertyPathCustomization::FPropertyPathCustomization(UWidgetBlueprint* InWidgetBlueprint) :
	WidgetBlueprint(InWidgetBlueprint)
{
	check(WidgetBlueprint != nullptr);

	OnBlueprintChangedHandle = WidgetBlueprint->OnChanged().AddRaw(this, &FPropertyPathCustomization::HandleBlueprintChanged);
}

FPropertyPathCustomization::~FPropertyPathCustomization()
{
	WidgetBlueprint->OnChanged().Remove(OnBlueprintChangedHandle);
}

void FPropertyPathCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;

	const FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, WidgetPath))
	{
		bIsWidget = true;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, ViewModelPath))
	{
		bIsWidget = false;
	}
	else
	{
		ensureMsgf(false, TEXT("MVVMPropertyPathCustomization used in unknown context."));
	}

	TSharedPtr<IPropertyHandle> ParentHandle = PropertyHandle->GetParentHandle();
	TSharedPtr<IPropertyHandle> OtherHandle;

	if (bIsWidget)
	{
		OtherHandle = ParentHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, ViewModelPath));
	}
	else
	{
		OtherHandle = ParentHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, WidgetPath));
	}

	if (OtherHandle.IsValid() && OtherHandle->IsValidHandle())
	{
		OtherHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPropertyPathCustomization::OnOtherPropertyChanged));
	}
	else
	{
		ensureMsgf(false, TEXT("MVVMPropertyPathCustomization used in unknown context."));
	}

	TSharedPtr<IPropertyHandle> BindingTypeHandle = ParentHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, BindingType));
	if (BindingTypeHandle.IsValid() && BindingTypeHandle->IsValidHandle())
	{
		BindingTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPropertyPathCustomization::OnOtherPropertyChanged));
	}
	else
	{
		ensureMsgf(false, TEXT("MVVMPropertyPathCustomization used in unknown context."));
	}

	uint8 Value;
	BindingTypeHandle->GetValue(Value);

	HeaderRow
		.NameWidget
		.VAlign(VAlign_Center)
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueWidget
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(FieldSelector, SFieldSelector, WidgetBlueprint, !bIsWidget)
			.TextStyle(FAppStyle::Get(), "SmallText")
			.OnFieldSelectionChanged(this, &FPropertyPathCustomization::OnFieldSelectionChanged)
			.SelectedField(this, &FPropertyPathCustomization::OnGetSelectedField)
			.BindingMode(this, &FPropertyPathCustomization::GetCurrentBindingMode)
		];
}

void FPropertyPathCustomization::OnOtherPropertyChanged()
{
	FieldSelector->Refresh();
}

EMVVMBindingMode FPropertyPathCustomization::GetCurrentBindingMode() const
{
	TSharedPtr<IPropertyHandle> BindingTypeHandle = PropertyHandle->GetParentHandle()->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, BindingType));
	uint8 BindingMode;
	if (BindingTypeHandle->GetValue(BindingMode) == FPropertyAccess::Success)
	{
		return (EMVVMBindingMode) BindingMode;
	}

	return EMVVMBindingMode::OneWayToDestination;
}

void FPropertyPathCustomization::OnFieldSelectionChanged(FMVVMBlueprintPropertyPath Selected)
{
	if (bPropertySelectionChanging)
	{
		return;
	}
	TGuardValue<bool> ReentrantGuard(bPropertySelectionChanging, true);

	PropertyHandle->NotifyPreChange();

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	for (void* RawPtr : RawData)
	{
		if (RawPtr == nullptr)
		{
			continue;
		}

		FMVVMBlueprintPropertyPath* PropertyPath = (FMVVMBlueprintPropertyPath*) RawPtr;
		*PropertyPath = Selected;
	}

	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

void FPropertyPathCustomization::HandleBlueprintChanged(UBlueprint* Blueprint)
{
	//FieldSelector->Refresh();
}

FBindingSource FPropertyPathCustomization::OnGetSelectedSource() const
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	FGuid SelectedGuid;
	FName SelectedName;

	bool bFirst = true;

	for (void* RawPtr : RawData)
	{
		if (RawPtr == nullptr)
		{
			continue;
		}

		FMVVMBlueprintPropertyPath* PropertyPath = (FMVVMBlueprintPropertyPath*) RawPtr;
		if (bIsWidget)
		{
			if (bFirst)
			{
				SelectedName = PropertyPath->GetWidgetName();
			}
			else if (SelectedName != PropertyPath->GetWidgetName())
			{
				SelectedName = FName();
				break;
			}
		}
		else
		{
			if (bFirst)
			{
				SelectedGuid = PropertyPath->GetViewModelId();
			}
			else if (SelectedGuid != PropertyPath->GetViewModelId())
			{
				SelectedGuid = FGuid();
				break;
			}
		}

		bFirst = false;
	}

	if (bIsWidget)
	{
		return FBindingSource::CreateForWidget(WidgetBlueprint, SelectedName);
	}
	else
	{
		return FBindingSource::CreateForViewModel(WidgetBlueprint, SelectedGuid);
	}
}

FMVVMBlueprintPropertyPath FPropertyPathCustomization::OnGetSelectedField() const
{
	// TODO: This crashes when removing the viewmodel that the selected binding is pointed at
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	FMVVMBlueprintPropertyPath SelectedField;

	bool bFirst = true;

	for (void* RawPtr : RawData)
	{
		if (RawPtr == nullptr)
		{
			continue;
		}

		FMVVMBlueprintPropertyPath* PropertyPath = (FMVVMBlueprintPropertyPath*) RawPtr;
		if (bFirst)
		{
			SelectedField = *PropertyPath;
		}
		else if (SelectedField != *PropertyPath)
		{
			SelectedField = FMVVMBlueprintPropertyPath();
			break;
		}
		bFirst = false;
	}

	return SelectedField;
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE