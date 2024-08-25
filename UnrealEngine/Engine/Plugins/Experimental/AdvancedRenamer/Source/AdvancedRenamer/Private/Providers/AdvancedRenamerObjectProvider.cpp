// Copyright Epic Games, Inc. All Rights Reserved.

#include "Providers/AdvancedRenamerObjectProvider.h"
#include "UObject/Object.h"

FAdvancedRenamerObjectProvider::FAdvancedRenamerObjectProvider()
{
}

FAdvancedRenamerObjectProvider::~FAdvancedRenamerObjectProvider()
{
}

void FAdvancedRenamerObjectProvider::SetObjectList(const TArray<TWeakObjectPtr<UObject>>& InObjectList)
{
	ObjectList.Empty();
	ObjectList.Append(InObjectList);
}

void FAdvancedRenamerObjectProvider::AddObjectList(const TArray<TWeakObjectPtr<UObject>>& InObjectList)
{
	ObjectList.Append(InObjectList);
}

void FAdvancedRenamerObjectProvider::AddObjectData(UObject* InObject)
{
	ObjectList.Add(InObject);
}

UObject* FAdvancedRenamerObjectProvider::GetObject(int32 Index) const
{
	if (!ObjectList.IsValidIndex(Index))
	{
		return nullptr;
	}

	return ObjectList[Index].Get();
}

int32 FAdvancedRenamerObjectProvider::Num() const
{
	return ObjectList.Num();
}

bool FAdvancedRenamerObjectProvider::IsValidIndex(int32 Index) const
{
	UObject* Object = GetObject(Index);

	return IsValid(Object);
}

FString FAdvancedRenamerObjectProvider::GetOriginalName(int32 Index) const
{
	UObject* Object = GetObject(Index);

	if (!IsValid(Object))
	{
		return "";
	}

	return Object->GetName();
}

uint32 FAdvancedRenamerObjectProvider::GetHash(int32 Index) const
{
	UObject* Object = GetObject(Index);

	if (!IsValid(Object))
	{
		return 0;
	}

	return GetTypeHash(Object);
}

bool FAdvancedRenamerObjectProvider::RemoveIndex(int32 Index)
{
	if (!ObjectList.IsValidIndex(Index))
	{
		return false;
	}

	ObjectList.RemoveAt(Index);
	return true;
}

bool FAdvancedRenamerObjectProvider::CanRename(int32 Index) const
{
	UObject* Object = GetObject(Index);

	if (!IsValid(Object))
	{
		return false;
	}

	return true;
}

bool FAdvancedRenamerObjectProvider::ExecuteRename(int32 Index, const FString& NewName)
{
	UObject* Object = GetObject(Index);

	if (!IsValid(Object))
	{
		return false;
	}

	if (Object->Rename(*NewName, nullptr, REN_Test))
	{
		Object->Rename(*NewName);
		return true;
	}

	return false;
}
