// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoHash.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

FString LexToString(const FIoHash& Hash)
{
	FString Output;
	TArray<TCHAR, FString::AllocatorType>& CharArray = Output.GetCharArray();
	CharArray.AddUninitialized(sizeof(FIoHash::ByteArray) * 2 + 1);
	UE::String::BytesToHexLower(Hash.GetBytes(), CharArray.GetData());
	CharArray.Last() = TEXT('\0');
	return Output;
}
