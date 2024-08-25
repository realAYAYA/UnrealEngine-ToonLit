// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/InputDataTypes/AvaUserInputDataTypeBase.h"

FAvaUserInputDataTypeBase::FAvaUserInputDataTypeBase()
{
}

void FAvaUserInputDataTypeBase::OnUserCommit()
{
	OnCommit.ExecuteIfBound();
}
