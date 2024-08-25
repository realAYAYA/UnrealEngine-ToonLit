// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Editors/SDMPropertyEditVector.h"
#include "DynamicMaterialEditorSettings.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditVector"

void SDMPropertyEditVector::Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle, int InComponentCount)
{
	ensure(InPropertyHandle.IsValid());
	ComponentCount = InComponentCount;

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(InComponentCount)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyHandle(InPropertyHandle)
	);
}

TSharedRef<SWidget> SDMPropertyEditVector::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex >= 0 && InIndex < ComponentCount);
	static const TArray<FText> Labels = {
		LOCTEXT("X", "X:"),
		LOCTEXT("Y", "Y:"),
		LOCTEXT("Z", "Z:"),
		LOCTEXT("W", "W:"),
	};

	if (InIndex < 4)
	{
		return AddWidgetLabel(
			Labels[InIndex],
			CreateSpinBox(
				TAttribute<float>::CreateSP(this, &SDMPropertyEditVector::GetVectorValue, InIndex),
				FOnFloatValueChanged::CreateSP(this, &SDMPropertyEditVector::OnValueChanged, InIndex),
				LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (Vector)")
			)
		);

	}

	return CreateSpinBox(
		TAttribute<float>::CreateSP(this, &SDMPropertyEditVector::GetVectorValue, InIndex),
		FOnFloatValueChanged::CreateSP(this, &SDMPropertyEditVector::OnValueChanged, InIndex),
		LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (Vector)")
	);
}

float SDMPropertyEditVector::GetMaxWidthForWidget(int32 InIndex) const
{
	ensure(InIndex >= 0 && InIndex < ComponentCount);

	return UDynamicMaterialEditorSettings::Get()->MaxFloatSliderWidth;
}

float SDMPropertyEditVector::GetVectorValue(int32 InComponent) const
{
	ensure(InComponent >= 0 && InComponent < ComponentCount);

	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
			{
				FVector2D Value = FVector2D::ZeroVector;
				PropertyHandle->GetValue(Value);
				return Value[InComponent];
			}

			if (StructProperty->Struct == TBaseStructure<FVector>::Get())
			{
				FVector Value = FVector::ZeroVector;
				PropertyHandle->GetValue(Value);
				return Value[InComponent];
			}

			if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				FRotator Value = FRotator::ZeroRotator;
				PropertyHandle->GetValue(Value);
				EAxis::Type Axis;

				switch (InComponent)
				{
					case 0:
						Axis = EAxis::X;
						break;

					case 1:
						Axis = EAxis::Y;
						break;

					case 2:
						Axis = EAxis::Z;
						break;

					default:
						return 0.f;
				}

				return Value.GetComponentForAxis(Axis);
			}
		}
	}

	return 0.f;	
}

void SDMPropertyEditVector::OnValueChanged(float InNewValue, int32 InComponent) const
{
	ensure(InComponent >= 0 && InComponent < ComponentCount);

	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
			{
				FVector2D CurrentValue;

				if (PropertyHandle->GetValue(CurrentValue) == FPropertyAccess::Fail)
				{
					return;
				}

				if (FMath::IsNearlyEqual(CurrentValue[InComponent], InNewValue) == false)
				{
					CurrentValue[InComponent] = InNewValue;
					PropertyHandle->SetValue(CurrentValue, EPropertyValueSetFlags::NotTransactable | EPropertyValueSetFlags::InteractiveChange);
				}
			}

			if (StructProperty->Struct == TBaseStructure<FVector>::Get())
			{
				FVector CurrentValue;

				if (PropertyHandle->GetValue(CurrentValue) == FPropertyAccess::Fail)
				{
					return;
				}

				if (FMath::IsNearlyEqual(CurrentValue[InComponent], InNewValue) == false)
				{
					CurrentValue[InComponent] = InNewValue;
					PropertyHandle->SetValue(CurrentValue, EPropertyValueSetFlags::NotTransactable | EPropertyValueSetFlags::InteractiveChange);
				}
			}

			if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				FRotator CurrentValue;

				if (PropertyHandle->GetValue(CurrentValue) == FPropertyAccess::Fail)
				{
					return;
				}

				EAxis::Type Axis;

				switch (InComponent)
				{
					case 0:
						Axis = EAxis::X;
						break;

					case 1:
						Axis = EAxis::Y;
						break;

					case 2:
						Axis = EAxis::Z;
						break;

					default:
						return;
				}

				if (FMath::IsNearlyEqual(CurrentValue.GetComponentForAxis(Axis), InNewValue) == false)
				{
					CurrentValue.SetComponentForAxis(Axis, InNewValue);
					PropertyHandle->SetValue(CurrentValue, EPropertyValueSetFlags::NotTransactable | EPropertyValueSetFlags::InteractiveChange);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
