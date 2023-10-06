// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet2/Kismet2NameValidators.h"

class FEnhancedInputPlayerMappableNameValidator : public FStringSetNameValidator
{
public:
	FEnhancedInputPlayerMappableNameValidator(FName InExistingName);

	// Begin FNameValidatorInterface
	virtual EValidatorResult IsValid(const FString& Name, bool bOriginal) override;
	// End FNameValidatorInterface

	/**
	 * If the error code is EValidatorResult::AlreadyInUse, this
	 * returns special error text that says what asset is using the name.
	 * 
	 * Otherwise it will return INameValidatorInterface::GetErrorText.
	 */
	static FText GetErrorText(const FString& Name, EValidatorResult ErrorCode);
};