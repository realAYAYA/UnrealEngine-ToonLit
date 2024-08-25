// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Editors/SDMPropertyEditBool.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditBool"

void SDMPropertyEditBool::Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle)
{
	ensure(InPropertyHandle.IsValid());

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyHandle(InPropertyHandle)
	);
}

TSharedRef<SWidget> SDMPropertyEditBool::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);

	return CreateCheckbox(
		TAttribute<ECheckBoxState>::CreateSP(this, &SDMPropertyEditBool::IsChecked),
		FOnCheckStateChanged::CreateSP(this, &SDMPropertyEditBool::OnValueToggled)
	);
}

ECheckBoxState SDMPropertyEditBool::IsChecked() const
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		bool bValue;
		PropertyHandle->GetValue(bValue);

		return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

void SDMPropertyEditBool::OnValueToggled(ECheckBoxState InNewState)
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		TArray<UObject*> Outers;
		PropertyHandle->GetOuterObjects(Outers);

		bool bCurrentValue = false;
		const bool bNewValue = InNewState == ECheckBoxState::Checked;

		if (PropertyHandle->GetValue(bCurrentValue) == FPropertyAccess::Fail)
		{
			return;
		}

		if (bCurrentValue == bNewValue)
		{
			return;
		}

		if (Outers.IsEmpty() == false && IsValid(Outers[0]))
		{
			StartTransaction(LOCTEXT("TransactionDescription", "Material Designer Value Set (Checkbox)"));
		}

		PropertyHandle->SetValue(bNewValue, EPropertyValueSetFlags::NotTransactable | EPropertyValueSetFlags::InteractiveChange);

		if (Outers.IsEmpty() == false && IsValid(Outers[0]))
		{
			EndTransaction();
		}
	}
}

#undef LOCTEXT_NAMESPACE
