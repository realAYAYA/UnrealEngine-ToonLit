// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputBoolColumn.h"
#include "ChooserPropertyAccess.h"
#include "BoolColumn.h"

FOutputBoolColumn::FOutputBoolColumn()
{
	InputValue.InitializeAs(FBoolContextProperty::StaticStruct());
}

void FOutputBoolColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		InputValue.Get<FChooserParameterBoolBase>().SetValue(Context, RowValues[RowIndex]);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex];
	}
#endif
}
