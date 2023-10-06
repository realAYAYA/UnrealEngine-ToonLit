// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputFloatColumn.h"
#include "ChooserPropertyAccess.h"
#include "FloatRangeColumn.h"

FOutputFloatColumn::FOutputFloatColumn()
{
	InputValue.InitializeAs(FFloatContextProperty::StaticStruct());
}

void FOutputFloatColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		InputValue.Get<FChooserParameterFloatBase>().SetValue(Context, RowValues[RowIndex]);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex];
	}
#endif
}
