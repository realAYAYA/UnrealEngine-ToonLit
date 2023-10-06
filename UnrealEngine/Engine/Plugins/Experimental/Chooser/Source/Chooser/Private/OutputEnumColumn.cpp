// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputEnumColumn.h"
#include "EnumColumn.h"
#include "ChooserPropertyAccess.h"

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
		InputValue.Get<FChooserParameterEnumBase>().SetValue(Context, RowValues[RowIndex].Value);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex].Value;
	}
#endif
}