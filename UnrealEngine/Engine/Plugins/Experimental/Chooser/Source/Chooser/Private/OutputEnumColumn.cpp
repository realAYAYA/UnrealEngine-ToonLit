// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputEnumColumn.h"
#include "EnumColumn.h"
#include "ChooserPropertyAccess.h"
#if WITH_EDITOR
#include "PropertyBag.h"
#endif

FOutputEnumColumn::FOutputEnumColumn()
{
#if WITH_EDITOR
	InputValue.InitializeAs(FEnumContextProperty::StaticStruct());
#endif
}

void FOutputEnumColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		uint8 OutputValue = FallbackValue.Value;
		if (RowValues.IsValidIndex(RowIndex))
		{
			OutputValue = RowValues[RowIndex].Value;
		}
		InputValue.Get<FChooserParameterEnumBase>().SetValue(Context, OutputValue);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex].Value;
	}
#endif
}

#if WITH_EDITOR
	void FOutputEnumColumn::AddToDetails (FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FText DisplayName;
		InputValue.Get<FChooserParameterBase>().GetDisplayName(DisplayName);
		FName PropertyName("RowData",ColumnIndex);

		// make another property bag in place of our struct, just so that the value enum will be correctly typed in the details panel
		
		FPropertyBagPropertyDesc PropertyDesc(PropertyName,  EPropertyBagPropertyType::Enum, InputValue.Get<FChooserParameterEnumBase>().GetEnum());
		PropertyDesc.MetaData.Add(FPropertyBagPropertyDescMetaData("DisplayName", DisplayName.ToString()));
		PropertyBag.AddProperties({PropertyDesc});
		PropertyBag.SetValueEnum("Value", RowValues[RowIndex].Value, InputValue.Get<FChooserParameterEnumBase>().GetEnum());
	}
	
	void FOutputEnumColumn::SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FName PropertyName("RowData",ColumnIndex);
		
		TValueOrError<uint8, EPropertyBagResult> Result = PropertyBag.GetValueEnum(PropertyName, InputValue.Get<FChooserParameterEnumBase>().GetEnum());
		if (uint8* Value = Result.TryGetValue())
		{
			RowValues[RowIndex].Value = *Value;
		}
	}
#endif