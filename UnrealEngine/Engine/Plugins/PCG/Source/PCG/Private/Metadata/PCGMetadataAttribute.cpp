// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadata.h"

namespace PCGMetadataAttributeBase
{
	static constexpr TCHAR AllowedSpecialCharacters[4] = {' ', '_', '-', '/'};

	bool IsValidNameCharacter(TCHAR Character)
	{
		if (FChar::IsAlpha(Character) || FChar::IsDigit(Character))
		{
			return true;
		}

		for (const TCHAR AllowedSpecialCharacter : AllowedSpecialCharacters)
		{
			if (AllowedSpecialCharacter == Character)
			{
				return true;
			}
		}

		return false;
	}
}

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

void FPCGMetadataAttributeBase::SetValueFromValueKey(PCGMetadataEntryKey EntryKey, PCGMetadataValueKey ValueKey, bool bResetValueOnDefaultValueKey)
{
	FWriteScopeLock ScopeLock(EntryMapLock);
	SetValueFromValueKey_Unsafe(EntryKey, ValueKey, bResetValueOnDefaultValueKey);
}

void FPCGMetadataAttributeBase::SetValueFromValueKey_Unsafe(PCGMetadataEntryKey EntryKey, PCGMetadataValueKey ValueKey, bool bResetValueOnDefaultValueKey, bool bAllowInvalidEntries)
{
	if (EntryKey == PCGInvalidEntryKey)
	{
		check(bAllowInvalidEntries);
		return;
	}

	if (ValueKey == PCGDefaultValueKey && bResetValueOnDefaultValueKey)
	{
		EntryToValueKeyMap.Remove(EntryKey);
	}
	else
	{
		EntryToValueKeyMap.FindOrAdd(EntryKey) = ValueKey;
	}
}

void FPCGMetadataAttributeBase::SetValuesFromValueKeys(const TArrayView<const TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>>& EntryValuePairs, bool bResetValueOnDefaultValueKey)
{
	if (EntryValuePairs.IsEmpty())
	{
		return;
	}

	FWriteScopeLock ScopeLock(EntryMapLock);
	for (const TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>& EntryValuePair : EntryValuePairs)
	{
		SetValueFromValueKey_Unsafe(EntryValuePair.Key, EntryValuePair.Value, bResetValueOnDefaultValueKey, /*bAllowInvalidEntries=*/true);
	}
}

void FPCGMetadataAttributeBase::SetValuesFromValueKeys(const TArrayView<const PCGMetadataEntryKey>& EntryKeys, const TArrayView<const PCGMetadataValueKey>& ValueKeys, bool bResetValueOnDefaultValueKey)
{
	if (EntryKeys.IsEmpty() || EntryKeys.Num() != ValueKeys.Num())
	{
		return;
	}

	FWriteScopeLock ScopeLock(EntryMapLock);
	for (int32 i = 0; i < EntryKeys.Num(); ++i)
	{
		SetValueFromValueKey_Unsafe(EntryKeys[i], ValueKeys[i], bResetValueOnDefaultValueKey, /*bAllowInvalidEntries=*/true);
	}
}

void FPCGMetadataAttributeBase::SetValuesFromValueKeys(const TArrayView<const PCGMetadataEntryKey * const>& EntryKeys, const TArrayView<const PCGMetadataValueKey>& ValueKeys, bool bResetValueOnDefaultValueKey)
{
	if (EntryKeys.IsEmpty() || EntryKeys.Num() != ValueKeys.Num())
	{
		return;
	}

	FWriteScopeLock ScopeLock(EntryMapLock);
	for (int32 i = 0; i < EntryKeys.Num(); ++i)
	{
		SetValueFromValueKey_Unsafe(*EntryKeys[i], ValueKeys[i], bResetValueOnDefaultValueKey, /*bAllowInvalidEntries=*/true);
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

void FPCGMetadataAttributeBase::GetValueKeys(const TArrayView<const PCGMetadataEntryKey>& EntryKeys, TArray<PCGMetadataValueKey>& OutValueKeys) const
{
	if (EntryKeys.IsEmpty())
	{
		return;
	}

	OutValueKeys.SetNumUninitialized(EntryKeys.Num());
	// Bitset with all unset values. If we have any unset value, we will ask the parent for those.
	TBitArray<> UnsetValues(true, EntryKeys.Num());

	GetValueKeys_Internal(EntryKeys, OutValueKeys, UnsetValues);
}

void FPCGMetadataAttributeBase::GetValueKeys_Internal(const TArrayView<const PCGMetadataEntryKey>& EntryKeys, TArrayView<PCGMetadataValueKey> OutValueKeys, TBitArray<>& UnsetValues) const
{
	check(EntryKeys.Num() == OutValueKeys.Num() && OutValueKeys.Num() == UnsetValues.Num());

	bool bFoundAllKeys = true;
	TConstSetBitIterator<> It(UnsetValues);
	if (!It)
	{
		return;
	}

	EntryMapLock.ReadLock();

	for (; It; ++It)
	{
		const int32 Index = It.GetIndex();
		const PCGMetadataEntryKey EntryKey = EntryKeys[Index];

		auto SetValueKey = [Index, &OutValueKeys, &UnsetValues](PCGMetadataValueKey ValueKey)
		{
			OutValueKeys[Index] = ValueKey;
			UnsetValues[Index] = false;
		};
		
		if (EntryKey == PCGInvalidEntryKey)
		{
			SetValueKey(PCGDefaultValueKey);
		}
		else if (const PCGMetadataValueKey* FoundLocalKey = EntryToValueKeyMap.Find(EntryKey))
		{
			SetValueKey(*FoundLocalKey);
		}
		else if (!Parent)
		{
			SetValueKey(PCGDefaultValueKey);
		}
		else
		{
			bFoundAllKeys = false;
		}
	}

	EntryMapLock.ReadUnlock();

	ensure(Parent || bFoundAllKeys);

	if (Parent && !bFoundAllKeys)
	{
		Parent->GetValueKeys_Internal(EntryKeys, OutValueKeys, UnsetValues);
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
	for (int32 i = 0; i < Name.Len(); ++i)
	{
		if (!PCGMetadataAttributeBase::IsValidNameCharacter(Name[i]))
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

bool FPCGMetadataAttributeBase::SanitizeName(FString& InOutName)
{
	bool bAnyCharactersSanitized = false;

	for (int32 i = 0; i < InOutName.Len(); ++i)
	{
		if (!PCGMetadataAttributeBase::IsValidNameCharacter(InOutName[i]))
		{
			InOutName[i] = '_';
			bAnyCharactersSanitized = true;
		}
	}

	return bAnyCharactersSanitized;
}
