// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Editors/SDMPropertyEditColor.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditColor"

void SDMPropertyEditColor::Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle)
{
	ensure(InPropertyHandle.IsValid());

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyHandle(InPropertyHandle)
	);
}

TSharedRef<SWidget> SDMPropertyEditColor::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);

	return CreateColorPicker(
		true, // Use alpha
		TAttribute<FLinearColor>::CreateSP(this, &SDMPropertyEditColor::GetColorValue),
		FOnLinearColorValueChanged::CreateSP(this, &SDMPropertyEditColor::OnColorChanged),
		LOCTEXT("TransactionDescription", "Material Designer Value Set (Color)")
	);
}

FLinearColor SDMPropertyEditColor::GetColorValue() const
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle() && Property)
	{
		TArray<UObject*> Outers;
		PropertyHandle->GetOuterObjects(Outers);

		if (Outers.IsEmpty() == false && IsValid(Outers[0]))
		{
			FLinearColor Value = FLinearColor::Black;
			Property->GetValue_InContainer(Outers[0], &Value);
			return Value;
		}
	}

	return FLinearColor::Black;
}

void SDMPropertyEditColor::OnColorChanged(FLinearColor InNewValue)
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle() && Property)
	{
		TArray<UObject*> Outers;
		PropertyHandle->GetOuterObjects(Outers);

		if (Outers.IsEmpty() == false && IsValid(Outers[0]))
		{
			FLinearColor CurrentValue = FLinearColor::Black;
			Property->GetValue_InContainer(Outers[0], &CurrentValue);

			const bool bSameValue = FMath::IsNearlyEqual(InNewValue.R, CurrentValue.R)
				&& FMath::IsNearlyEqual(InNewValue.G, CurrentValue.G)
				&& FMath::IsNearlyEqual(InNewValue.B, CurrentValue.B)
				&& FMath::IsNearlyEqual(InNewValue.A, CurrentValue.A);

			if (bSameValue)
			{
				return;
			}

			StartTransaction(LOCTEXT("TransactionDescription", "Material Designer Value Set (Color)"));

			if (Property->HasSetter())
			{
				Property->CallSetter(Outers[0], &InNewValue);
			}
			else
			{
				Outers[0]->PreEditChange(Property);

				Property->SetValue_InContainer(Outers[0], &InNewValue);

				FPropertyChangedEvent PCE = FPropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
				Outers[0]->PostEditChangeProperty(PCE);
			}

			EndTransaction();
		}		
	}
}

#undef LOCTEXT_NAMESPACE
