// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Editors/SDMPropertyEditEnum.h"
#include "PropertyHandle.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditEnum"

void SDMPropertyEditEnum::Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle)
{
	ensure(InPropertyHandle.IsValid());

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyHandle(InPropertyHandle)
	);
}

TSharedRef<SWidget> SDMPropertyEditEnum::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);

	return CreateEnum(
		FOnGetPropertyComboBoxValue::CreateSP(this, &SDMPropertyEditEnum::GetEnumString),
		FOnGetPropertyComboBoxStrings::CreateSP(this, &SDMPropertyEditEnum::GetEnumStrings),
		FOnPropertyComboBoxValueSelected::CreateSP(this, &SDMPropertyEditEnum::OnValueChanged)
	);
}

int64 SDMPropertyEditEnum::GetEnumValue() const
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			TArray<UObject*> Outers;
			PropertyHandle->GetOuterObjects(Outers);

			if (Outers.IsEmpty() == false && IsValid(Outers[0]))
			{
				if (EnumProperty->HasGetter())
				{
					int64 Value = -1;
					EnumProperty->CallGetter(Outers[0], &Value);
					return Value;
				}
				else
				{
					return *EnumProperty->ContainerPtrToValuePtr<uint8>(Outers[0]);
				}
			}
		}
	}

	return -1;
}

FString SDMPropertyEditEnum::GetEnumString() const
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProperty->GetEnum())
			{
				int64 Value = GetEnumValue();

				if (Enum->IsValidEnumValue(Value))
				{
					return Enum->GetDisplayNameTextByValue(Value).ToString();
				}
			}
		}
	}

	return "";
}

void SDMPropertyEditEnum::GetEnumStrings(TArray<TSharedPtr<FString>>& OutComboBoxStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, 
	TArray<bool>& OutRestrictedItems) const
{
	OutComboBoxStrings.Empty();
	OutToolTips.Empty();
	OutRestrictedItems.Empty();

	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProperty->GetEnum())
			{
				int32 EnumCount = Enum->NumEnums();

				OutComboBoxStrings.Reserve(EnumCount);
				OutToolTips.Reserve(EnumCount);
				OutRestrictedItems.Reserve(EnumCount);

				for (int32 EnumIdx = 0; EnumIdx < EnumCount; ++EnumIdx)
				{
					int64 Value = Enum->GetValueByIndex(EnumIdx);

					if (Value == Enum->GetMaxEnumValue())
					{
						break;
					}

					OutComboBoxStrings.Add(MakeShared<FString>(Enum->GetDisplayNameTextByValue(Value).ToString()));

					FText ToolTip = Enum->GetToolTipTextByIndex(EnumIdx);

					if (ToolTip.IsEmpty())
					{
						OutToolTips.Add(SNew(SToolTip).Text(Enum->GetDisplayNameTextByValue(Value)));
					}
					else
					{
						OutToolTips.Add(SNew(SToolTip).Text(ToolTip));
					}

					OutRestrictedItems.Add(false);
				}
			}
		}
	}
}

void SDMPropertyEditEnum::OnValueChanged(const FString& InNewValueString)
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProperty->GetEnum())
			{
				TArray<UObject*> Outers;
				PropertyHandle->GetOuterObjects(Outers);

				if (Outers.IsEmpty() == false && IsValid(Outers[0]))
				{
					int32 EnumCount = Enum->NumEnums();

					for (int32 EnumIdx = 0; EnumIdx < EnumCount; ++EnumIdx)
					{
						const FString EnumString = Enum->GetDisplayNameTextByIndex(EnumIdx).ToString();

						if (EnumString == InNewValueString)
						{
							int64 Value = Enum->GetValueByIndex(EnumIdx);
							uint8* Data = EnumProperty->ContainerPtrToValuePtr<uint8>(Outers[0]);

							if (Value != *Data)
							{
								StartTransaction(LOCTEXT("TransactionDescription", "Material Designer Value Set (Enum)"));

								if (Property->HasSetter())
								{
									Property->CallSetter(Outers[0], &Value);
								}
								else
								{
									Outers[0]->PreEditChange(Property);

									*Data = Value;

									FPropertyChangedEvent PCE = FPropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
									Outers[0]->PostEditChangeProperty(PCE);
								}

								EndTransaction();
							}
						}
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
