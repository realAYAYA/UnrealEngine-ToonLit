// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "InstancedStruct.h"
#include "Templates/UniquePtr.h"

#include "LocalizableMessage.generated.h"

USTRUCT()
struct FLocalizableMessageParameterEntry
{
	GENERATED_BODY()

public:

	FLocalizableMessageParameterEntry();
	LOCALIZABLEMESSAGE_API FLocalizableMessageParameterEntry(const FString& InKey, const FInstancedStruct& InValue);
	LOCALIZABLEMESSAGE_API ~FLocalizableMessageParameterEntry();

	bool operator==(const FLocalizableMessageParameterEntry& Other) const;
	bool operator!=(const FLocalizableMessageParameterEntry& Other) const
	{
		return !(*this == Other);
	}

	UPROPERTY()
	FString Key;

	UPROPERTY()
	FInstancedStruct Value;
};

USTRUCT(BlueprintType)
struct FLocalizableMessage
{
	GENERATED_BODY();

	LOCALIZABLEMESSAGE_API FLocalizableMessage();
	LOCALIZABLEMESSAGE_API ~FLocalizableMessage();
	LOCALIZABLEMESSAGE_API bool operator==(const FLocalizableMessage& Other) const;

	void Reset()
	{
		Key.Reset();
		DefaultText.Reset();
		Substitutions.Reset();
	}

	bool operator!=(const FLocalizableMessage& Other) const
	{
		return !(*this == Other);
	}

	bool IsEmpty() const
	{
		return Key.IsEmpty() && DefaultText.IsEmpty();
	}

	UPROPERTY()
	FString Key;

	UPROPERTY()
	FString DefaultText;

	UPROPERTY()
	TArray<FLocalizableMessageParameterEntry> Substitutions;
};
