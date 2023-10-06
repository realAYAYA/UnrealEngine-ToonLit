// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadata.h"

FPCGMetadataAttributeBase::FPCGMetadataAttributeBase(UPCGMetadata* InMetadata, FName InName, const FPCGMetadataAttributeBase* InParent, bool bInAllowsInterpolation)
	: Metadata(InMetadata)
	, Parent(InParent)
	, bAllowsInterpolation(bInAllowsInterpolation)
	, Name(InName)
{
}

void FPCGMetadataAttributeBase::Serialize(UPCGMetadata* InMetadata, FArchive& InArchive)
{
	InArchive << EntryToValueKeyMap;
	Metadata = InMetadata;

	int32 ParentAttributeId = (Parent ? Parent->AttributeId : -1);
	InArchive << ParentAttributeId;

	if (InArchive.IsLoading())
	{
		ensure(ParentAttributeId < 0 || Metadata->GetParent());
		if (ParentAttributeId >= 0 && Metadata->GetParent())
		{
			Parent = Metadata->GetParent()->GetConstAttributeById(ParentAttributeId);
			check(Parent);
		}
	}

	//Type id should already be known by then, so no need to serialize it
	InArchive << Name;
	InArchive << AttributeId;
}

void FPCGMetadataAttributeBase::SetValueFromValueKey(PCGMetadataEntryKey EntryKey, PCGMetadataValueKey ValueKey)
{
	check(EntryKey != PCGInvalidEntryKey);
	FWriteScopeLock ScopeLock(EntryMapLock);
	EntryToValueKeyMap.FindOrAdd(EntryKey) = ValueKey;
}

void FPCGMetadataAttributeBase::SetValuesFromValueKeys(const TArray<TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>>& EntryValuePairs, bool bResetValueOnDefaultValueKey)
{
	if (EntryValuePairs.IsEmpty())
	{
		return;
	}

	FWriteScopeLock ScopeLock(EntryMapLock);
	for (const TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>& EntryValuePair : EntryValuePairs)
	{
		check(EntryValuePair.Key != PCGInvalidEntryKey);
		if (EntryValuePair.Value == PCGDefaultValueKey)
		{
			if (bResetValueOnDefaultValueKey)
			{
				EntryToValueKeyMap.Remove(EntryValuePair.Key);
			}
		}
		else
		{
			EntryToValueKeyMap.FindOrAdd(EntryValuePair.Key, EntryValuePair.Value);
		}
	}
}

PCGMetadataValueKey FPCGMetadataAttributeBase::GetValueKey(PCGMetadataEntryKey EntryKey) const
{
	if (EntryKey == PCGInvalidEntryKey)
	{
		return PCGDefaultValueKey;
	}

	PCGMetadataValueKey ValueKey = PCGDefaultValueKey;
	bool bFoundKey = false;

	EntryMapLock.ReadLock();
	if (const PCGMetadataValueKey* FoundLocalKey = EntryToValueKeyMap.Find(EntryKey))
	{
		ValueKey = *FoundLocalKey;
		bFoundKey = true;
	}
	EntryMapLock.ReadUnlock();

	if (!bFoundKey && Parent)
	{
		return Parent->GetValueKey(Metadata->GetParentKey(EntryKey));
	}
	else
	{
		return ValueKey;
	}
}

bool FPCGMetadataAttributeBase::HasNonDefaultValue(PCGMetadataEntryKey EntryKey) const
{
	return GetValueKey(EntryKey) != PCGDefaultValueKey;
}

void FPCGMetadataAttributeBase::ClearEntries()
{
	EntryToValueKeyMap.Reset();
}

bool FPCGMetadataAttributeBase::IsValidName(const FString& Name)
{
	// A valid name is alphanumeric with some special characters allowed.
	static const FString AllowedSpecialCharacters = TEXT(" _-/");

	for (int32 i = 0; i < Name.Len(); ++i)
	{
		if (FChar::IsAlpha(Name[i]) || FChar::IsDigit(Name[i]))
		{
			continue;
		}

		bool bAllowedSpecialCharacterFound = false;

		for (int32 j = 0; j < AllowedSpecialCharacters.Len(); ++j)
		{
			if (Name[i] == AllowedSpecialCharacters[j])
			{
				bAllowedSpecialCharacterFound = true;
				break;
			}
		}

		if (!bAllowedSpecialCharacterFound)
		{
			return false;
		}
	}

	return true;
}

bool FPCGMetadataAttributeBase::IsValidName(const FName& Name)
{
	// Early out on None
	return (Name == NAME_None) || IsValidName(Name.ToString());
}