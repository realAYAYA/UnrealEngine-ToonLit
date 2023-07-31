// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class FUnrealSourceFile;
class FUnrealTypeDefinitionInfo;

enum class EHeaderProviderSourceType
{
	ClassName,
	ScriptStructName,
	TypeDef,
	FileName,
};

class FHeaderProvider
{
	friend bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
public:
	FHeaderProvider(EHeaderProviderSourceType Type, FString&& Id);
	explicit FHeaderProvider(FUnrealTypeDefinitionInfo& InTypeDef);

	FUnrealSourceFile* Resolve(const FUnrealSourceFile& ParentSourceFile);

	FString ToString() const;

	const FString& GetId() const;

private:
	EHeaderProviderSourceType Type;
	FString Id;
	FUnrealTypeDefinitionInfo* TypeDef = nullptr;
	FUnrealSourceFile* Cache = nullptr;
	bool bResolved = false;
};

bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
