// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/PropertyViewer/SBoolPropertyValue.h"

#include "Framework/PropertyViewer/INotifyHook.h"
#include "Styling/SlateTypes.h"
#include "Styling/AppStyle.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

#include "Widgets/Input/SCheckBox.h"

namespace UE::PropertyViewer
{

TSharedPtr<SWidget> SBoolPropertyValue::CreateInstance(const FPropertyValueFactory::FGenerateArgs Args)
{
	return SNew(SBoolPropertyValue)
		.Path(Args.Path)
		.NotifyHook(Args.NotifyHook)
		.IsEnabled(Args.bCanEditValue);
}


void SBoolPropertyValue::Construct(const FArguments& InArgs)
{
	Path = InArgs._Path;
	NotifyHook = InArgs._NotifyHook;

	const FProperty* Property = Path.GetLastProperty();
	if (CastField<const FBoolProperty>(Property) && Property->ArrayDim == 1)
	{
		ChildSlot
		[
			SNew(SCheckBox)
			.IsChecked(this, &SBoolPropertyValue::HandleIsChecked)
			.OnCheckStateChanged(this, &SBoolPropertyValue::HandleCheckStateChanged)
 		];
	}
}


ECheckBoxState SBoolPropertyValue::HandleIsChecked() const
{
	if (const FBoolProperty* Property = CastField<const FBoolProperty>(Path.GetLastProperty()))
	{
		if (const void* Container = Path.GetContainerPtr())
		{
			const bool bValue = Property->GetPropertyValue_InContainer(Container);
			return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
	}
	return ECheckBoxState::Undetermined;
}


void SBoolPropertyValue::HandleCheckStateChanged(ECheckBoxState NewState)
{
	if (const FBoolProperty* Property = CastField<const FBoolProperty>(Path.GetLastProperty()))
	{
		if (void* Container = Path.GetContainerPtr())
		{
			if (NotifyHook)
			{
				NotifyHook->OnPreValueChange(Path);
			}
			bool bValue = NewState == ECheckBoxState::Checked;
			Property->SetPropertyValue_InContainer(Container, bValue);
			if (NotifyHook)
			{
				NotifyHook->OnPostValueChange(Path);
			}
		}
	}
}

} //namespace
