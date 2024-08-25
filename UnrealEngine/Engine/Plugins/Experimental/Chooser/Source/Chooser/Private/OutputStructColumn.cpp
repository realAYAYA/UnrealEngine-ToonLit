// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputStructColumn.h"
#include "ChooserPropertyAccess.h"
#include "OutputStructColumn.h"

#if WITH_EDITOR
#include "IPropertyAccessEditor.h"
#include "PropertyBag.h"
#endif

bool FStructContextProperty::SetValue(FChooserEvaluationContext& Context, const FInstancedStruct& InValue) const
{
	void* TargetData;
	if (Binding.GetValuePtr(Context, TargetData))
	{
		InValue.GetScriptStruct()->CopyScriptStruct(TargetData, InValue.GetMemory());
		return true;
	}
	return false;
}

#if WITH_EDITOR

void FOutputStructColumn::StructTypeChanged()
{
	if (InputValue.IsValid())
	{
		const UScriptStruct* Struct = InputValue.Get<FChooserParameterStructBase>().GetStructType();

		if (DefaultRowValue.GetScriptStruct() != Struct)
		{
			DefaultRowValue.InitializeAs(Struct);
		}

		if (FallbackValue.GetScriptStruct() != Struct)
		{
			FallbackValue.InitializeAs(Struct);
		}

		for (FInstancedStruct& RowValue : RowValues)
		{
			if (RowValue.GetScriptStruct() != Struct)
			{
				RowValue.InitializeAs(Struct);
			}
		}
	}
}

void FOutputStructColumn::AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
{
	FText DisplayName;
	InputValue.Get<FChooserParameterStructBase>().GetDisplayName(DisplayName);
	FName PropertyName("RowData",ColumnIndex);
	FPropertyBagPropertyDesc PropertyDesc(PropertyName,  EPropertyBagPropertyType::Struct, RowValues[RowIndex].GetScriptStruct());
	PropertyDesc.MetaData.Add(FPropertyBagPropertyDescMetaData("DisplayName", DisplayName.ToString()));
	PropertyBag.AddProperties({PropertyDesc});
	PropertyBag.SetValueStruct(PropertyName, FConstStructView(RowValues[RowIndex].GetScriptStruct(), RowValues[RowIndex].GetMemory()));
}

void FOutputStructColumn::SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
{
	FName PropertyName("RowData", ColumnIndex);
	
	TValueOrError<FStructView, EPropertyBagResult> Result = PropertyBag.GetValueStruct(PropertyName, RowValues[RowIndex].GetScriptStruct());
	if (FStructView* StructView = Result.TryGetValue())
	{
		RowValues[RowIndex].GetScriptStruct()->CopyScriptStruct(RowValues[RowIndex].GetMutableMemory(), StructView->GetMemory());
	}
}

#endif // WITH_EDITOR

FOutputStructColumn::FOutputStructColumn()
{
	InputValue.InitializeAs(FStructContextProperty::StaticStruct());
}

void FOutputStructColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		const FInstancedStruct* OutputValue = &FallbackValue;
		if (RowValues.IsValidIndex(RowIndex))
		{
			OutputValue = &RowValues[RowIndex];
		}

		if (OutputValue && OutputValue->IsValid())
		{
			InputValue.Get<FChooserParameterStructBase>().SetValue(Context, *OutputValue);
		}
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex];
	}
#endif
}
