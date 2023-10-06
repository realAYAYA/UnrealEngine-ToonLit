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

	virtual void Flatten() = 0;

	const UPCGMetadata* GetMetadata() const { return Metadata; }
	int16 GetTypeId() const { return TypeId; }

	virtual FPCGMetadataAttributeBase* Copy(FName NewName, UPCGMetadata* InMetadata, bool bKeepParent, bool bCopyEntries = true, bool bCopyValues = true) const = 0;

	virtual PCGMetadataValueKey GetValueKeyOffsetForChild() const = 0;
	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey) = 0;
	virtual void SetZeroValue(PCGMetadataEntryKey ItemKey) = 0;
	virtual void AccumulateValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, float Weight) = 0;
	virtual void SetWeightedValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<TPair<PCGMetadataEntryKey, float>> InWeightedKeys) = 0;
	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB, EPCGMetadataOp Op) = 0;
	virtual bool IsEqualToDefaultValue(PCGMetadataValueKey ValueKey) const = 0;

	virtual bool UsesValueKeys() const = 0;
	virtual bool AreValuesEqualForEntryKeys(PCGMetadataEntryKey EntryKey1, PCGMetadataEntryKey EntryKey2) const = 0;
	virtual bool AreValuesEqual(PCGMetadataValueKey ValueKey1, PCGMetadataValueKey ValueKey2) const = 0;

	void SetValueFromValueKey(PCGMetadataEntryKey EntryKey, PCGMetadataValueKey ValueKey);
	void SetValuesFromValueKeys(const TArray<TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>>& EntryValuePairs, bool bResetValueOnDefaultValueKey = true);
	PCGMetadataValueKey GetValueKey(PCGMetadataEntryKey EntryKey) const;
	bool HasNonDefaultValue(PCGMetadataEntryKey EntryKey) const;
	void ClearEntries();

	bool AllowsInterpolation() const { return bAllowsInterpolation; }

	int32 GetNumberOfEntries() const { return EntryToValueKeyMap.Num(); }
	int32 GetNumberOfEntriesWithParents() const { return EntryToValueKeyMap.Num() + (Parent ? Parent->GetNumberOfEntries() : 0); }

	// This call is not thread safe
	const TMap<PCGMetadataEntryKey, PCGMetadataValueKey>& GetEntryToValueKeyMap_NotThreadSafe() const { return EntryToValueKeyMap; }

	const FPCGMetadataAttributeBase* GetParent() const { return Parent; }

	static bool IsValidName(const FString& Name);
	static bool IsValidName(const FName& Name);

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
