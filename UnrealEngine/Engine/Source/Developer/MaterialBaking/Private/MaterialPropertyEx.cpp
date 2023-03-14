// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialPropertyEx.h"
#include "UObject/NameTypes.h"

FString FMaterialPropertyEx::ToString() const
{
	if (!IsCustomOutput())
	{
		const UEnum* Enum = StaticEnum<EMaterialProperty>();
		FName Name = Enum->GetNameByValue(Type);
		FString TrimmedName = Name.ToString();
		TrimmedName.RemoveFromStart(TEXT("MP_"), ESearchCase::CaseSensitive);
		return TrimmedName;
	}

	return CustomOutput.ToString();
}
