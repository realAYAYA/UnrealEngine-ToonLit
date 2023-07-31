// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusValidatedName.h"

bool FOptimusValidatedName::IsValid(FString const& InName, FText* OutReason, FText const* InErrorCtx)
{
	static const TCHAR* InvalidCharacters = TEXT("\"\\' ,.|&!~\n\r\t@#/(){}[]=;:^%$`");

	return FName::IsValidXName(InName, InvalidCharacters, OutReason, InErrorCtx);
}

bool FOptimusValidatedName::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if (Tag.Type == NAME_NameProperty)
	{
		Slot << Name;
		return true;
	}
	if (Tag.Type == NAME_StrProperty)
	{
		FString InName;
		Slot << InName;
		Name = FName(InName);
		return true;
	}

	return false;
}
