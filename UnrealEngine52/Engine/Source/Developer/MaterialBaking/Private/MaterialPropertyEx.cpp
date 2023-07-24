// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialPropertyEx.h"
#include "UObject/NameTypes.h"

const FMaterialPropertyEx FMaterialPropertyEx::ClearCoatBottomNormal(TEXT("ClearCoatBottomNormal"));
const FMaterialPropertyEx FMaterialPropertyEx::TransmittanceColor(TEXT("TransmittanceColor"));

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
