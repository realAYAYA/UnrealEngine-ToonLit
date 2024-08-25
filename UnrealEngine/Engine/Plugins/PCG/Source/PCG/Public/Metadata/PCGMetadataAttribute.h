// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGMetadataCommon.h"

#include "UObject/ObjectPtr.h" // IWYU pragma: keep

class UPCGMetadata;

namespace PCGMetadataAttributeConstants
{
	const FName LastAttributeName = TEXT("@Last");
	const FName LastCreatedAttributeName = TEXT("@LastCreated");
	const FName SourceAttributeName = TEXT("@Source");
	const FName SourceNameAttributeName = TEXT("@SourceName");
}

class PCG_API FPCGMetadataAttributeBase
{
public:
	FPCGMetadataAttributeBase() = default;
	FPCGMetadataAttributeBase(UPCGMetadata* InMetadata, FName InName, const FPCGMetadataAttributeBase* InParent, bool bInAllowsInterpolation);

	virtual ~FPCGMetadataAttributeBase() = default;
	virtual void Serialize(UPCGMetadata* InMetadata, FArchive& InArchive);

	/** Unparents current attribute by flattening the values, entries, etc. */
	virtual void Flatten() = 0;
	/** Unparents current attribute by flattening the values, entries, etc while only keeping the entries referenced in InEntryKeysToKeep. There must be NO invalid entry keys. */
	virtual void FlattenAndCompress(const TArray<PCGMetadataEntryKey>& InEntryKeysToKeep) = 0;

	/** Remove all entries, values and parenting. */
	virtual void Reset() = 0;

	const UPCGMetadata* GetMetadata() const { return Metadata; }
	int16 GetTypeId() const { return TypeId; }

	virtual FPCGMetadataAttributeBase* Copy(FName NewName, UPCGMetadata* InMetadata, bool bKeepParent, bool bCopyEntries = true, bool bCopyValues = true) const = 0;
	virtual FPCGMetadataAttributeBase* CopyToAnotherType(int16 Type) const = 0;

	virtual PCGMetadataValueKey GetValueKeyOffsetForChild() const = 0;
	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey) = 0;
	virtual void SetZeroValue(PCGMetadataEntryKey ItemKey) = 0;
	virtual void AccumulateValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, float Weight) = 0;
	virtual void SetWeightedValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<const TPair<PCGMetadataEntryKey, float>>& InWeightedKeys) = 0;
	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB, EPCGMetadataOp Op) = 0;
	virtual bool IsEqualToDefaultValue(PCGMetadataValueKey ValueKey) const = 0;
	/** In the case of multi entry attribute and after some operations, we might have a single entry attribute with a default value that is different than the first entry. Use this function to fix that. Only valid if there is one and only one value. */
	virtual void SetDefaultValueToFirstEntry() = 0;

	virtual bool UsesValueKeys() const = 0;
	virtual bool AreValuesEqualForEntryKeys(PCGMetadataEntryKey EntryKey1, PCGMetadataEntryKey EntryKey2) const = 0;
	virtual bool AreValuesEqual(PCGMetadataValueKey ValueKey1, PCGMetadataValueKey ValueKey2) const = 0;

	void SetValueFromValueKey(PCGMetadataEntryKey EntryKey, PCGMetadataValueKey ValueKey, bool bResetValueOnDefaultValueKey = false);
	PCGMetadataValueKey GetValueKey(PCGMetadataEntryKey EntryKey) const;
	bool HasNonDefaultValue(PCGMetadataEntryKey EntryKey) const;
	void ClearEntries();

	/** Bulk getter, to lock in read only once per parent. */
	void GetValueKeys(const TArrayView<const PCGMetadataEntryKey>& EntryKeys, TArray<PCGMetadataValueKey>& OutValueKeys) const;

	/** Bulk setter to lock in write only once. */
	void SetValuesFromValueKeys(const TArrayView<const TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>>& EntryValuePairs, bool bResetValueOnDefaultValueKey = true);

	/** Two arrays version of bulk setter to lock in write only once. Both arrays must be the same size. */
	void SetValuesFromValueKeys(const TArrayView<const PCGMetadataEntryKey>& EntryKeys, const TArrayView<const PCGMetadataValueKey>& ValueKeys, bool bResetValueOnDefaultValueKey = true);
	void SetValuesFromValueKeys(const TArrayView<const PCGMetadataEntryKey* const>& EntryKeys, const TArrayView<const PCGMetadataValueKey>& ValueKeys, bool bResetValueOnDefaultValueKey = true);

	bool AllowsInterpolation() const { return bAllowsInterpolation; }

	int32 GetNumberOfEntries() const { return EntryToValueKeyMap.Num(); }
	int32 GetNumberOfEntriesWithParents() const { return EntryToValueKeyMap.Num() + (Parent ? Parent->GetNumberOfEntries() : 0); }

	// This call is not thread safe
	const TMap<PCGMetadataEntryKey, PCGMetadataValueKey>& GetEntryToValueKeyMap_NotThreadSafe() const { return EntryToValueKeyMap; }

	const FPCGMetadataAttributeBase* GetParent() const { return Parent; }

	/** Returns true if for valid attribute names, which are alphanumeric with some special characters allowed. */
	static bool IsValidName(const FString& Name);
	static bool IsValidName(const FName& Name);

	/** Replaces any invalid characters in name with underscores. Returns true if Name was changed. */
	static bool SanitizeName(FString& InOutName);

private:
	// Unsafe version, needs to be write lock protected.
	void SetValueFromValueKey_Unsafe(PCGMetadataEntryKey EntryKey, PCGMetadataValueKey ValueKey, bool bResetValueOnDefaultValueKey, bool bAllowInvalidEntries = false);

	void GetValueKeys_Internal(const TArrayView<const PCGMetadataEntryKey>& EntryKeys, TArrayView<PCGMetadataValueKey> OutValueKeys, TBitArray<>& UnsetValues) const;

protected:
	TMap<PCGMetadataEntryKey, PCGMetadataValueKey> EntryToValueKeyMap;
	mutable FRWLock EntryMapLock;

	TObjectPtr<UPCGMetadata> Metadata = nullptr;
	const FPCGMetadataAttributeBase* Parent = nullptr;
	int16 TypeId = 0;
	bool bAllowsInterpolation = false;

public:
	FName Name = NAME_None;
	PCGMetadataAttributeKey AttributeId = -1;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Misc/ScopeRWLock.h"
#endif
