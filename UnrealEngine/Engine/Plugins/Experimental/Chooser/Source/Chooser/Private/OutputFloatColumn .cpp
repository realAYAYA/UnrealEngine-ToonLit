// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputFloatColumn.h"
#include "ChooserPropertyAccess.h"
#include "FloatRangeColumn.h"
#if WITH_EDITOR
#include "PropertyBag.h"
#endif

FOutputFloatColumn::FOutputFloatColumn()
{
	InputValue.InitializeAs(FFloatContextProperty::StaticStruct());
}

void FOutputFloatColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		double OutputValue = FallbackValue;
		if (RowValues.IsValidIndex(RowIndex))
		{
			OutputValue = RowValues[RowIndex];
		}
	
		InputValue.Get<FChooserParameterFloatBase>().SetValue(Context, OutputValue);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex];
	}
#endif
}
#if WITH_EDITOR
	void FOutputFloatColumn::AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FText DisplayName;
		InputValue.Get<FChooserParameterBase>().GetDisplayName(DisplayName);
		FName PropertyName("RowData",ColumnIndex);
		FPropertyBagPropertyDesc PropertyDesc(PropertyName, EPropertyBagPropertyType::Float);
		PropertyDesc.MetaData.Add(FPropertyBagPropertyDescMetaData("DisplayName", DisplayName.ToString()));
		PropertyBag.AddProperties({PropertyDesc});
		PropertyBag.SetValueFloat(PropertyName, RowValues[RowIndex]);
	}

	void FOutputFloatColumn::SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FName PropertyName("RowData", ColumnIndex);
			
		TValueOrError<float, EPropertyBagResult> Result = PropertyBag.GetValueFloat(PropertyName);
		if (float* Value = Result.TryGetValue())
		{
			RowValues[RowIndex] = *Value;
		}
	}
#endif