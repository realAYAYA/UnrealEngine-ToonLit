// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dom/WebAPIParameter.h"

#if WITH_EDITOR
void UWebAPIParameter::SetCodeText(const FString& InCodeText)
{
	GeneratedCodeText = InCodeText;
}

void UWebAPIParameter::AppendCodeText(const FString& InCodeText)
{
	GeneratedCodeText += TEXT("\n") + InCodeText;
}
#endif
