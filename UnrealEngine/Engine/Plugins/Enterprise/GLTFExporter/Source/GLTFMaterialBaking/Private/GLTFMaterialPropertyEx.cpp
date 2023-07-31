// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFMaterialPropertyEx.h"
#include "UObject/NameTypes.h"

FString FGLTFMaterialPropertyEx::ToString() const
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
