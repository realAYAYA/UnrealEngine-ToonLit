// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransformableRegistry.h"

#include "UObject/Class.h"

FTransformableRegistry::~FTransformableRegistry() = default;

FTransformableRegistry& FTransformableRegistry::Get()
{
	static FTransformableRegistry Singleton;
	return Singleton;
}

void FTransformableRegistry::Register(
	UClass* InClass,
	CreateHandleFuncT InHandleFunc,
	GetHashFuncT InHashFunc)
{
	const FTransformableInfo Info { InHandleFunc, InHashFunc};
	Transformables.Emplace(InClass, Info);	
}

FTransformableRegistry::GetHashFuncT FTransformableRegistry::GetHashFunction(const UClass* InClass) const
{
	if (InClass)
	{
		if (const FTransformableInfo* Info = FindInfo(InClass))
		{
			return Info->GetHashFunc; 
		}
	}
	return {};
}

FTransformableRegistry::CreateHandleFuncT FTransformableRegistry::GetCreateFunction(const UClass* InClass) const
{
	if (InClass)
	{
		if(const FTransformableInfo* Info = FindInfo(InClass))
		{
			return Info->CreateHandleFunc; 
		}
	}
	return {};
}

const FTransformableRegistry::FTransformableInfo* FTransformableRegistry::FindInfo(const UClass* InClass) const
{
	if (const FTransformableInfo* Info = Transformables.Find(InClass))
	{
		return Info;
	}

	return nullptr;
}