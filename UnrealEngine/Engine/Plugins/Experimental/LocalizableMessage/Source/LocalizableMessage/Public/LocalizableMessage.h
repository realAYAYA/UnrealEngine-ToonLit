// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "InstancedStruct.h"
#include "Templates/UniquePtr.h"

#include "LocalizableMessage.generated.h"

USTRUCT()
struct LOCALIZABLEMESSAGE_API FLocalizableMessageParameterEntry
{
	GENERATED_BODY()

public:

	FLocalizableMessageParameterEntry();
	FLocalizableMessageParameterEntry(const FString& InKey, const FInstancedStruct& InValue);
	~FLocalizableMessageParameterEntry();

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
struct LOCALIZABLEMESSAGE_API FLocalizableMessage
{
	GENERATED_BODY();

	FLocalizableMessage();
	~FLocalizableMessage();
	bool operator==(const FLocalizableMessage& Other) const;

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
