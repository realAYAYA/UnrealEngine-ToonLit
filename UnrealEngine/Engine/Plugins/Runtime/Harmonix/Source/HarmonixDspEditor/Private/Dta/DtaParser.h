// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"

#include "Templates/SharedPointer.h"


DECLARE_LOG_CATEGORY_EXTERN(LogDtaParser, Log, All);


struct FDtaParser
{
	static void DtaStringToJsonString(const FString& InDtaString, FString& OutJsonString);

private:

	static void Tokenize(const FString& InDtaString, TArray<FString>& OutTokens);
	static void ParseToJson(const TArray<FString>& Tokens, FString& OutString);
};