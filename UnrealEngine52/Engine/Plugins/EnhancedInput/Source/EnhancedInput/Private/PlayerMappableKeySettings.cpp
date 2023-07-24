// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerMappableKeySettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerMappableKeySettings)

#define LOCTEXT_NAMESPACE "EnhancedActionKeySetting"

#if WITH_EDITOR
EDataValidationResult UPlayerMappableKeySettings::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(ValidationErrors), EDataValidationResult::Valid);
	if (Name == NAME_None)
	{
		Result = EDataValidationResult::Invalid;
		ValidationErrors.Add(LOCTEXT("InvalidPlayerMappableKeySettingsName", "A Player Mappable Key Settings must have a valid 'Name'"));
	}
	return Result;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
