// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Editors/SDMPropertyEditObject.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditObject"

void SDMPropertyEditObject::Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle, UClass* InAllowedClass)
{
	ensure(InPropertyHandle.IsValid());

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyHandle(InPropertyHandle)
	);

	AllowedClass = InAllowedClass;
}

TSharedRef<SWidget> SDMPropertyEditObject::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);

	return CreateAssetPicker(
		AllowedClass.Get(),
		TAttribute<FString>::CreateSP(this, &SDMPropertyEditObject::GetObjectPath),
		FOnSetObject::CreateSP(this, &SDMPropertyEditObject::OnValueChanged)
	);
}

FString SDMPropertyEditObject::GetObjectPath() const
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		UObject* Object = nullptr;
		PropertyHandle->GetValue(Object);

		if (!IsValid(Object))
		{
			return "";
		}

		return Object->GetPathName();
	}

	return "";
}

void SDMPropertyEditObject::OnValueChanged(const FAssetData& InAssetData)
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		UObject* CurrentValue = nullptr;

		if (PropertyHandle->GetValue(CurrentValue) == FPropertyAccess::Fail)
		{
			return;
		}

		UObject* NewValue = InAssetData.GetAsset();

		if (CurrentValue == NewValue)
		{
			return;
		}

		TArray<UObject*> Outers;
		PropertyHandle->GetOuterObjects(Outers);
		const bool bHasValidOuter = Outers.IsEmpty() == false && IsValid(Outers[0]);

		if (bHasValidOuter)
		{
			StartTransaction(LOCTEXT("TransactionDescription", "Material Designer Value Set (Asset)"));
		}

		PropertyHandle->SetValue(NewValue, EPropertyValueSetFlags::NotTransactable | EPropertyValueSetFlags::InteractiveChange);

		if (bHasValidOuter)
		{
			EndTransaction();
		}
	}
}

#undef LOCTEXT_NAMESPACE
