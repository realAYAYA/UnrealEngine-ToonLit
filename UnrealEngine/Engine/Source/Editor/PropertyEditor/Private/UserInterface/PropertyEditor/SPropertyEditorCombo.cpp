// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorCombo.h"
#include "IDocumentation.h"

#include "PropertyEditorHelpers.h"
#include "UserInterface/PropertyEditor/SPropertyComboBox.h"


#define LOCTEXT_NAMESPACE "PropertyEditor"

void SPropertyEditorCombo::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 125.0f;
	OutMaxDesiredWidth = 400.0f;
}

bool SPropertyEditorCombo::Supports( const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const FProperty* Property = InPropertyEditor->GetProperty();
	int32 ArrayIndex = PropertyNode->GetArrayIndex();

	if(	((Property->IsA(FByteProperty::StaticClass()) && CastField<const FByteProperty>(Property)->Enum)
		||	Property->IsA(FEnumProperty::StaticClass())
		|| (Property->IsA(FStrProperty::StaticClass()) && Property->HasMetaData(TEXT("Enum")))
		|| !PropertyEditorHelpers::GetPropertyOptionsMetaDataKey(Property).IsNone()
		)
		&&	( ( ArrayIndex == -1 && Property->ArrayDim == 1 ) || ( ArrayIndex > -1 && Property->ArrayDim > 0 ) ) )
	{
		return true;
	}

	return false;
}

void SPropertyEditorCombo::Construct( const FArguments& InArgs, const TSharedPtr< class FPropertyEditor >& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;
	ComboArgs = InArgs._ComboArgs;

	if (PropertyEditor.IsValid())
	{
		ComboArgs.PropertyHandle = PropertyEditor->GetPropertyHandle();
		if (ComboArgs.PropertyHandle.IsValid())
		{
			ComboArgs.PropertyHandle->SetOnPropertyResetToDefault(FSimpleDelegate::CreateSP(this, &SPropertyEditorCombo::OnResetToDefault));
		}
	}

	ensureMsgf(ComboArgs.PropertyHandle.IsValid() || (ComboArgs.OnGetStrings.IsBound() && ComboArgs.OnGetValue.IsBound() && ComboArgs.OnValueSelected.IsBound()), TEXT("Either PropertyEditor or ComboArgs.PropertyHandle must be set!"));

	TArray<TSharedPtr<FString>> ComboItems;
	TArray<bool> Restrictions;
	TArray<TSharedPtr<SToolTip>> RichToolTips;

	if (!ComboArgs.Font.HasValidFont())
	{
		ComboArgs.Font = FAppStyle::GetFontStyle(PropertyEditorConstants::PropertyFontStyle);
	}

	GenerateComboBoxStrings(ComboItems, RichToolTips, Restrictions);

	SAssignNew(ComboBox, SPropertyComboBox)
		.Font( ComboArgs.Font )
		.RichToolTipList( RichToolTips )
		.ComboItemList( ComboItems )
		.RestrictedList( Restrictions )
		.OnSelectionChanged( this, &SPropertyEditorCombo::OnComboSelectionChanged )
		.OnComboBoxOpening( this, &SPropertyEditorCombo::OnComboOpening )
		.VisibleText( this, &SPropertyEditorCombo::GetDisplayValueAsString )
		.ToolTipText( this, &SPropertyEditorCombo::GetValueToolTip )
		.ShowSearchForItemCount( ComboArgs.ShowSearchForItemCount );

	ChildSlot
	[
		ComboBox.ToSharedRef()
	];

	SetEnabled( TAttribute<bool>( this, &SPropertyEditorCombo::CanEdit ) );
	SetToolTipText( TAttribute<FText>( this, &SPropertyEditorCombo::GetValueToolTip) );
}

FString SPropertyEditorCombo::GetDisplayValueAsString() const
{
	if (ComboArgs.OnGetValue.IsBound())
	{
		return ComboArgs.OnGetValue.Execute();
	}
	else if (bUsesAlternateDisplayValues)
	{
		{
			FString RawValueString;
			ComboArgs.PropertyHandle->GetValueAsFormattedString(RawValueString, PPF_None);

			if (const FString* AlternateDisplayValuePtr = InternalValueToAlternateDisplayValue.Find(RawValueString))
			{
				return *AlternateDisplayValuePtr;
			}
		}

		if (PropertyEditor.IsValid())
		{
			return PropertyEditor->GetValueAsDisplayString();
		}

		FString ValueString;
		ComboArgs.PropertyHandle->GetValueAsDisplayString(ValueString);
		return ValueString;
	}
	else
	{
		if (PropertyEditor.IsValid())
		{
			return PropertyEditor->GetValueAsString();
		}

		FString ValueString;
		ComboArgs.PropertyHandle->GetValueAsFormattedString(ValueString);
		return ValueString;
	}
}

FText SPropertyEditorCombo::GetValueToolTip() const
{
	if (bUsesAlternateDisplayValues)
	{
		FString RawValueString;
		ComboArgs.PropertyHandle->GetValueAsFormattedString(RawValueString, PPF_None);

		if (const FString* AlternateDisplayValuePtr = InternalValueToAlternateDisplayValue.Find(RawValueString))
		{
			return FText::AsCultureInvariant(*AlternateDisplayValuePtr);
		}
	}

	if (PropertyEditor.IsValid())
	{
		return PropertyEditor->GetValueAsText();
	}

	return FText();
}

void SPropertyEditorCombo::GenerateComboBoxStrings( TArray< TSharedPtr<FString> >& OutComboBoxStrings, TArray<TSharedPtr<SToolTip>>& RichToolTips, TArray<bool>& OutRestrictedItems )
{
	if (ComboArgs.OnGetStrings.IsBound())
	{
		ComboArgs.OnGetStrings.Execute(OutComboBoxStrings, RichToolTips, OutRestrictedItems);
		return;
	}

	TArray<FText> BasicTooltips;

	bUsesAlternateDisplayValues = ComboArgs.PropertyHandle->GeneratePossibleValues(OutComboBoxStrings, BasicTooltips, OutRestrictedItems);

	// If we regenerate the entries, let's make sure that the currently selected item has the same shared pointer as
	// the newly generated item with the same value, so that the generation of elements won't immediately result in a
	// value changed event (i.e. at every single `OnComboOpening`).
	if (ComboBox)
	{
		if (const TSharedPtr<FString>& SelectedItem = ComboBox->GetSelectedItem())
		{
			for (TSharedPtr<FString>& Item : OutComboBoxStrings)
			{
				if (Item)
				{
					if (*SelectedItem.Get() == *Item.Get())
					{
						Item = SelectedItem;
						break;
					}
				}
			}
		}
	}

	// Build the reverse LUT for alternate display values
	AlternateDisplayValueToInternalValue.Reset();
	InternalValueToAlternateDisplayValue.Reset();
	if (const FProperty* Property = ComboArgs.PropertyHandle->GetProperty();
		bUsesAlternateDisplayValues && !Property->IsA(FStrProperty::StaticClass()))
	{
		// currently only enum properties can use alternate display values; this 
		// might change, so assert here so that if support is expanded to other 
		// property types without updating this block of code, we'll catch it quickly
		const UEnum* Enum = nullptr;
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			Enum = ByteProperty->Enum;
		}
		else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Enum = EnumProperty->GetEnum();
		}
		check(Enum != nullptr);

		const TMap<FName, FText> EnumValueDisplayNameOverrides = PropertyEditorHelpers::GetEnumValueDisplayNamesFromPropertyOverride(Property, Enum);
		auto FindEnumValueIndex = [&EnumValueDisplayNameOverrides, &Enum](const FString& ValueString) -> int32
		{
			for (const TTuple<FName, FText>& EnumValueDisplayNameOverridePair : EnumValueDisplayNameOverrides)
			{
				if (EnumValueDisplayNameOverridePair.Value.ToString() == ValueString)
				{
					return Enum->GetIndexByName(EnumValueDisplayNameOverridePair.Key);
				}
			}

			for (int32 ValIndex = 0; ValIndex < Enum->NumEnums() - 1; ++ValIndex)
			{
				const FString EnumName = Enum->GetNameStringByIndex(ValIndex);
				const FString DisplayName = Enum->GetDisplayNameTextByIndex(ValIndex).ToString();

				if (DisplayName.Len() > 0)
				{
					if (DisplayName == ValueString)
					{
						return ValIndex;
					}
				}

				if (EnumName == ValueString)
				{
					return ValIndex;
				}
			}

			return INDEX_NONE;
		};

		for (const TSharedPtr<FString>& ValueStringPtr : OutComboBoxStrings)
		{
			const int32 EnumIndex = FindEnumValueIndex(*ValueStringPtr);
			check(EnumIndex != INDEX_NONE);

			const FString EnumValue = Enum->GetNameStringByIndex(EnumIndex);
			if (EnumValue != *ValueStringPtr)
			{
				AlternateDisplayValueToInternalValue.Add(*ValueStringPtr, EnumValue);
				InternalValueToAlternateDisplayValue.Add(EnumValue, *ValueStringPtr);
			}
		}
	}

	// For enums, look for rich tooltip information
	if(ComboArgs.PropertyHandle.IsValid())
	{
		if(const FProperty* Property = ComboArgs.PropertyHandle->GetProperty())
		{
			UEnum* Enum = nullptr;

			if(const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
			{
				Enum = ByteProperty->Enum;
			}
			else if(const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				Enum = EnumProperty->GetEnum();
			}

			if(Enum)
			{
				TArray<FName> ValidPropertyEnums = PropertyEditorHelpers::GetValidEnumsFromPropertyOverride(Property, Enum);
				TArray<FName> InvalidPropertyEnums = PropertyEditorHelpers::GetInvalidEnumsFromPropertyOverride(Property, Enum);
				
				// Get enum doc link (not just GetDocumentationLink as that is the documentation for the struct we're in, not the enum documentation)
				FString DocLink = PropertyEditorHelpers::GetEnumDocumentationLink(Property);

				for(int32 EnumIdx = 0; EnumIdx < Enum->NumEnums() - 1; ++EnumIdx)
				{
					FString Excerpt = Enum->GetNameStringByIndex(EnumIdx);

					bool bShouldBeHidden = Enum->HasMetaData(TEXT("Hidden"), EnumIdx) || Enum->HasMetaData(TEXT("Spacer"), EnumIdx);
					if(!bShouldBeHidden)
					{
						if (ValidPropertyEnums.Num() > 0)
						{
							bShouldBeHidden = ValidPropertyEnums.Find(Enum->GetNameByIndex(EnumIdx)) == INDEX_NONE;
						}

						// If both are specified, the metadata "InvalidEnumValues" takes precedence
						if (InvalidPropertyEnums.Num() > 0)
						{
							bShouldBeHidden = InvalidPropertyEnums.Find(Enum->GetNameByIndex(EnumIdx)) != INDEX_NONE;
						}
					}

					if(!bShouldBeHidden)
					{
						bShouldBeHidden = ComboArgs.PropertyHandle->IsHidden(Excerpt);
					}
				
					if(!bShouldBeHidden)
					{
						RichToolTips.Add(IDocumentation::Get()->CreateToolTip(MoveTemp(BasicTooltips[EnumIdx]), nullptr, DocLink, MoveTemp(Excerpt)));
					}
				}
			}
		}
	}
}

void SPropertyEditorCombo::OnComboSelectionChanged( TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo )
{
	if ( NewValue.IsValid() )
	{
		SendToObjects( *NewValue );
	}
}

void SPropertyEditorCombo::OnResetToDefault()
{
	FString CurrentDisplayValue = GetDisplayValueAsString();
	ComboBox->SetSelectedItem(CurrentDisplayValue);
}

void SPropertyEditorCombo::OnComboOpening()
{
	TArray<TSharedPtr<FString>> ComboItems;
	TArray<TSharedPtr<SToolTip>> RichToolTips;
	TArray<bool> Restrictions;
	GenerateComboBoxStrings(ComboItems, RichToolTips, Restrictions);

	ComboBox->SetItemList(ComboItems, RichToolTips, Restrictions);

	// try and re-sync the selection in the combo list in case it was changed since Construct was called
	// this would fail if the displayed value doesn't match the equivalent value in the combo list
	FString CurrentDisplayValue = GetDisplayValueAsString();
	ComboBox->SetSelectedItem(CurrentDisplayValue);
}

void SPropertyEditorCombo::SendToObjects( const FString& NewValue )
{
	FString Value = NewValue;
	if (ComboArgs.OnValueSelected.IsBound())
	{
		ComboArgs.OnValueSelected.Execute(NewValue);
	}
	else if (ComboArgs.PropertyHandle.IsValid())
	{
		FProperty* Property = ComboArgs.PropertyHandle->GetProperty();

		if (bUsesAlternateDisplayValues)
		{
			if (const FString* InternalValuePtr = AlternateDisplayValueToInternalValue.Find(Value))
			{
				Value = *InternalValuePtr;
			}
		}

		ComboArgs.PropertyHandle->SetValueFromFormattedString(Value);
	}
}

bool SPropertyEditorCombo::CanEdit() const
{
	if (ComboArgs.PropertyHandle.IsValid())
	{
		return ComboArgs.PropertyHandle->IsEditable();
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
