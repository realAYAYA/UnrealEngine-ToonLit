// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/StructOnScope.h"
#include "UObject/Package.h"

FStructOnScope::FStructOnScope()
	: SampleStructMemory(nullptr)
	, OwnsMemory(false)
{
}

FStructOnScope::FStructOnScope(const UStruct* InScriptStruct)
	: ScriptStruct(InScriptStruct)
	, SampleStructMemory(nullptr)
	, OwnsMemory(false)
{
	Initialize(); //-V1053
}

FStructOnScope::FStructOnScope(const UStruct* InScriptStruct, uint8* InData)
	: ScriptStruct(InScriptStruct)
	, SampleStructMemory(InData)
	, OwnsMemory(false)
{
}

FStructOnScope::FStructOnScope(FStructOnScope&& InOther)
{
	ScriptStruct = InOther.ScriptStruct;
	SampleStructMemory = InOther.SampleStructMemory;
	OwnsMemory = InOther.OwnsMemory;

	InOther.OwnsMemory = false;
	InOther.Reset();
}

FStructOnScope::~FStructOnScope()
{
	Destroy(); //-V1053
}


UPackage* FStructOnScope::GetPackage() const
{
	return Package.Get();
}

void FStructOnScope::SetPackage(UPackage* InPackage)
{
	Package = InPackage;
}
