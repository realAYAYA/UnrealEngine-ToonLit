// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalizableMessage.h"

FLocalizableMessageParameterEntry::FLocalizableMessageParameterEntry() = default;
FLocalizableMessageParameterEntry::FLocalizableMessageParameterEntry(const FString& InKey, const FInstancedStruct& InValue) : Key(InKey), Value(InValue) {}
FLocalizableMessageParameterEntry::~FLocalizableMessageParameterEntry() = default;

bool FLocalizableMessageParameterEntry::operator==(const FLocalizableMessageParameterEntry& Other) const
{
	return Key == Other.Key &&
		   Value == Other.Value;
}

FLocalizableMessage::FLocalizableMessage() = default;
FLocalizableMessage::~FLocalizableMessage() = default;

bool FLocalizableMessage::operator==(const FLocalizableMessage& Other) const
{
	return Key == Other.Key &&
		DefaultText == Other.DefaultText &&
		Substitutions == Other.Substitutions;
}