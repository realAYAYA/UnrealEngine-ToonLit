// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Internationalization/Text.h"

class PROPERTYEDITOR_API FPropertyRestriction
{
public:

	FPropertyRestriction(const FText& InReason)
		: Reason(InReason)
	{
	}

	const FText& GetReason() const { return Reason; }

	bool IsValueHidden(const FString& Value) const;
	bool IsValueDisabled(const FString& Value) const;
	void AddHiddenValue(FString Value);
	void AddDisabledValue(FString Value);

	void RemoveHiddenValue(FString Value);
	void RemoveDisabledValue(FString Value);
	void RemoveAll();

	TArray<FString>::TConstIterator GetHiddenValuesIterator() const 
	{
		return HiddenValues.CreateConstIterator();
	}

	TArray<FString>::TConstIterator GetDisabledValuesIterator() const
	{
		return DisabledValues.CreateConstIterator();
	}

private:

	TArray<FString> HiddenValues;
	TArray<FString> DisabledValues;
	FText Reason;
};
