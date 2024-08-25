// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputBoolColumn.h"
#include "ChooserPropertyAccess.h"
#include "BoolColumn.h"
#if WITH_EDITOR
#include "PropertyBag.h"
#endif

FOutputBoolColumn::FOutputBoolColumn()
{
	InputValue.InitializeAs(FBoolContextProperty::StaticStruct());
}

void FOutputBoolColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		bool bOutputValue = bFallbackValue;
		if (RowValues.IsValidIndex(RowIndex))
		{
			bOutputValue = RowValues[RowIndex];
		}
		
		InputValue.Get<FChooserParameterBoolBase>().SetValue(Context, bOutputValue);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex];
	}
#endif
}

#if WITH_EDITOR
	void FOutputBoolColumn::AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FText DisplayName;
		InputValue.Get<FChooserParameterBoolBase>().GetDisplayName(DisplayName);
		FName PropertyName("RowData",ColumnIndex);
		FPropertyBagPropertyDesc PropertyDesc(PropertyName, EPropertyBagPropertyType::Bool);
		PropertyDesc.MetaData.Add(FPropertyBagPropertyDescMetaData("DisplayName", DisplayName.ToString()));
		PropertyBag.AddProperties({PropertyDesc});
		PropertyBag.SetValueFloat(PropertyName, RowValues[RowIndex]);
	}

	void FOutputBoolColumn::SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FName PropertyName("RowData", ColumnIndex);
			
		TValueOrError<bool, EPropertyBagResult> Result = PropertyBag.GetValueBool(PropertyName);
		if (bool* Value = Result.TryGetValue())
		{
			RowValues[RowIndex] = *Value;
		}
	} 

#endif
