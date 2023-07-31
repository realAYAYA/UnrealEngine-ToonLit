// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebAPIOpenAPIFactoryBase.h"

#define LOCTEXT_NAMESPACE "WebAPIOpenAPIFactoryBase"

const FName UWebAPIOpenAPIFactoryBase::JsonFileType = FName(TEXT("json"));
const FName UWebAPIOpenAPIFactoryBase::YamlFileType = FName(TEXT("yaml"));

UWebAPIOpenAPIFactoryBase::UWebAPIOpenAPIFactoryBase()
{
	Formats.Add(TEXT("json;JavaScript Object Notation"));
}

bool UWebAPIOpenAPIFactoryBase::IsValidFileExtension(const FString& InFileExtension) const
{
	return InFileExtension.Equals(JsonFileType.ToString(), ESearchCase::IgnoreCase);
}

#undef LOCTEXT_NAMESPACE
