// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"

/** Structure for templated strings that are displayed in the editor with a allowed args. */
struct FTemplateString
{
	/**
	* The template string.
	*/
	FString Template;

	/** Returns validity based on brace matching, and if provided, arg presence in ValidArgs. */
	bool IsValid(const TArray<FString>& InValidArgs = {}) const;
};
