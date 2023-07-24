// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

struct FLiveCodingManifest
{
	FString LinkerPath;
	TMap<FString, FString> LinkerEnvironment;
	TMap<FString, TArray<FString>> BinaryToObjectFiles;
	TMap<FString, TArray<FString>> Libraries;

	bool Read(const TCHAR* FileName, FString& OutFailReason);
	bool Parse(FJsonObject& Object, FString& OutFailReason);
};
