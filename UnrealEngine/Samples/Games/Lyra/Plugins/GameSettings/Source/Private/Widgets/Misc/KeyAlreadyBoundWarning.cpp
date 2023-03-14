// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Misc/KeyAlreadyBoundWarning.h"

void UKeyAlreadyBoundWarning::SetWarningText(const FText& InText)
{
	WarningText->SetText(InText);
}

void UKeyAlreadyBoundWarning::SetCancelText(const FText& InText)
{
	CancelText->SetText(InText);
}
