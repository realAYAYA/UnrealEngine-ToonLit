// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Metadata/PCGMetadataCommon.h"
#include "PCGModule.h"

#include "Helpers/PCGMetadataHelpers.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Misc/ScopeRWLock.h"

class UPCGMetadata;

template<typename T>
class FPCGMetadataAttribute : public FPCGMetadataAttributeBase
{
	template<typename U>
	friend class FPCGMetadataAttribute;

public:
	FPCGMetadataAttribute(UPCGMetadata* InMetadata, FName InName, const FPCGMetadataAttributeBase* InParent, const T& InDefaultValue, bool bInAllowsInterpolation)
		: FPCGMetadataAttributeBase(InMetadata, InName, InParent, bInAllowsInterpolation)
		, DefaultValue(InDefaultValue)
	{
		TypeId = PCG::Private::MetadataTypes<T>::Id;

		// Make sure we don't parent with the wrong type id
		check(!Parent || Parent->GetTypeId() == TypeId)

		if (GetParent())
		{
			ValueKeyOffset = GetParent()->GetValueKeyOffsetForChild();
		}
	}

	// This constructor is used only during serialization
	FPCGMetadataAttribute()
	{
		TypeId = PCG::Private::MetadataTypes<T>::Id;
	}

	virtual void Serialize(UPCGMetadata* InMetadata, FArchive& InArchive) override
	{
		FPCGMetadataAttributeBase::Serialize(InMetadata, InArchive);

		InArchive << Values;
		InArchive << DefaultValue;
		
		// Initialize non-serialized members
		if (InArchive.IsLoading())
		{
			ValueKeyOffset = GetParent() ? GetParent()->GetValueKeyOffsetForChild() : 0;
		}
	}

	virtual void Flatten() override
	{
		// Implementation notes:
		// We don't need to flatten the EntryToValueKeyMap - this will have been taken care of in the metadata flatten

		// Flatten values, from root to current attribute
		if (Parent)
		{
			FWriteScopeLock ScopeLock(ValueLock);

			TArray<const TArray<T>*> OriginalValues;
			int32 ValueCount = 0;

			const FPCGMetadataAttribute<T>* Current = static_cast<const FPCGMetadataAttribute<T>*>(this);
			while (Current)
			{
				ValueCount += Current->Values.Num();
				OriginalValues.Add(&Current->Values);
				Current = static_cast<const FPCGMetadataAttribute<T>*>(Current->Parent);
			}

			TArray<T> FlattenedValues;
			FlattenedValues.Reserve(ValueCount);

			for (int32 ValuesIndex = OriginalValues.Num() - 1; ValuesIndex >= 0; --ValuesIndex)
			{
				FlattenedValues.Append(*OriginalValues[ValuesIndex]);
			}

			Values = MoveTemp(FlattenedValues);
		}
		
		// Reset value offset, and lose parent
		ValueKeyOffset = 0;
		Parent = nullptr;
	}

	virtual void FlattenAndCompress(const TArray<PCGMetadataEntryKey>& InEntryKeysToKeep) override
	{
		// No entries, we can just delete everything in the attribute.
		if (InEntryKeysToKeep.IsEmpty())
		{
			Reset();
			return;
		}

		TArray<PCGMetadataValueKey> AllValueKeys;
		TArray<PCGMetadataValueKey> AllUniqueValueKeys;
		AllValueKeys.Reserve(InEntryKeysToKeep.Num());
		constexpr bool bUseValueKeys = PCG::Private::MetadataTraits<T>::CompressData;
		if constexpr (bUseValueKeys)
		{
			AllUniqueValueKeys.Reserve(InEntryKeysToKeep.Num());
		}

		// First gather all value keys associated with the entry keys to keep.
		// If we compress data, we also store the unique value keys used (that is not default).
		for (const PCGMetadataEntryKey& EntryKey : InEntryKeysToKeep)
		{
			AllValueKeys.Add(GetValueKey(EntryKey));
			if constexpr (bUseValueKeys)
			{
				if (AllValueKeys.Last() != PCGDefaultValueKey)
				{
					AllUniqueValueKeys.AddUnique(AllValueKeys.Last());
				}
			}
		}

		// Then for each value key (or unique values keys), gather the value in a NewValues array
		// and also keep a mapping between old value key and new value key.
		// Only done if the old value key is not the default one.
		TMap<PCGMetadataValueKey, PCGMetadataValueKey> ValueKeyMapping;
		TArray<T> NewValues;
		const TArray<PCGMetadataValueKey>& AllValueKeysRef = !bUseValueKeys ? AllValueKeys : AllUniqueValueKeys;
		NewValues.Reserve(AllValueKeysRef.Num());
		ValueKeyMapping.Reserve(AllValueKeysRef.Num());
		for (PCGMetadataValueKey ValueKey : AllValueKeysRef)
		{
			if (ValueKey != PCGDefaultValueKey)
			{
				ValueKeyMapping.Add(ValueKey, NewValues.Num());
				NewValues.Add(GetValue(ValueKey));
			}
		}

		// Move the new values in place of the old values.
		ValueLock.WriteLock();
		Values = std::move(NewValues);
		ValueLock.WriteUnlock();

		// And finally, create a new entry to value mapping.
		// Logic is that each entry to keep will have their "index" as new entry key
		// (like if the entries to keep are [25, 47, 54], the new entries would be [0, 1, 2]).
		// So the operation is:
		// All pairs Old EK -> Old VK transform to New EK -> New VK.
		TMap<PCGMetadataEntryKey, PCGMetadataValueKey> NewMap;
		for (int32 i = 0; i < InEntryKeysToKeep.Num(); ++i)
		{
			PCGMetadataValueKey ValueKey = AllValueKeys[i];
			if (ValueKey != PCGDefaultValueKey && ensure(InEntryKeysToKeep[i] != PCGInvalidEntryKey))
			{
				NewMap.Add(i, ValueKeyMapping[ValueKey]);
			}
		}

		// And move the map.
		EntryMapLock.WriteLock();
		EntryToValueKeyMap = std::move(NewMap);
		EntryMapLock.WriteUnlock();

		// At the end, reset value offset, and lose parent.
		ValueKeyOffset = 0;
		Parent = nullptr;
	}

	virtual void Reset() override
	{
		ValueKeyOffset = 0;
		Parent = nullptr;

		EntryMapLock.WriteLock();
		EntryToValueKeyMap.Empty();
		EntryMapLock.WriteUnlock();

		ValueLock.WriteLock();
		Values.Empty();
		ValueLock.WriteUnlock();
	}

	const FPCGMetadataAttribute* GetParent() const { return static_cast<const FPCGMetadataAttribute*>(Parent); }

	FPCGMetadataAttribute* TypedCopy(FName NewName, UPCGMetadata* InMetadata, bool bKeepParent, bool bCopyEntries = true, bool bCopyValues = true)
	{
		return static_cast<FPCGMetadataAttribute*>(Copy(NewName, InMetadata, bKeepParent, bCopyEntries, bCopyValues));
	}

	virtual FPCGMetadataAttributeBase* Copy(FName NewName, UPCGMetadata* InMetadata, bool bKeepParent, bool bCopyEntries = true, bool bCopyValues = true) const override
	{
		return CopyInternal<T>(NewName, InMetadata, bKeepParent, bCopyEntries, bCopyValues);
	}

	virtual FPCGMetadataAttributeBase* CopyToAnotherType(int16 TargetType) const override;

	// TODO: add enable if only on compatible types, but this has some repercussion on using metadata on types that aren't normally supported.
	template<typename U>
	FPCGMetadataAttributeBase* CopyInternal(FName NewName, UPCGMetadata* InMetadata, bool bKeepParent, bool bCopyEntries, bool bCopyValues) const
	{
		// If we copy an attribute where we don't want to keep the parent, while copying entries and/or values, we'll lose data.
		// In that case, we will copy all the data from this attribute and all its ancestors.

		// We can't keep the parent if we don't have the same root.
		check(!bKeepParent || PCGMetadataHelpers::HasSameRoot(Metadata, InMetadata));

		// Validate that the new name is valid
		if (!IsValidName(NewName))
		{
			UE_LOG(LogPCG, Error, TEXT("Try to create a new attribute with an invalid name: %s"), *NewName.ToString());
			return nullptr;
		}

		U NewDefaultValue{};
		PCG::Private::GetValueWithBroadcastAndConstructible(DefaultValue, NewDefaultValue);
		
		// This copies to a new attribute.
		FPCGMetadataAttribute<U>* AttributeCopy = new FPCGMetadataAttribute<U>(InMetadata, NewName, bKeepParent ? this : nullptr, NewDefaultValue, bAllowsInterpolation);

		// Gather the chain of parents if we don't keep the parent and we want to copy entries/values.
		// We always have at least one item, "this".
		TArray<const FPCGMetadataAttribute<T>*, TInlineAllocator<2>> Parents = { this };
		if (!bKeepParent && (bCopyEntries || bCopyValues))
		{
			const UPCGMetadata* CurrentMetadata = Metadata.Get();
			const FPCGMetadataAttribute<T>* Current = this;

			const UPCGMetadata* ParentMetadata = PCGMetadataHelpers::GetParentMetadata(CurrentMetadata);
			while(ParentMetadata && Current->Parent)
			{
				CurrentMetadata = ParentMetadata;
				Current = static_cast<const FPCGMetadataAttribute<T>*>(Current->Parent);
				Parents.Add(Current);

				ParentMetadata = PCGMetadataHelpers::GetParentMetadata(CurrentMetadata);
			}
		}

		if (bCopyEntries)
		{
			// We go backwards, since we need to preserve order (root -> this)
			// Latest entry in our Parents array is the root.
			for (int32 i = Parents.Num() - 1; i >= 0; --i)
			{
				const FPCGMetadataAttribute<T>* Current = Parents[i];
				
				Current->EntryMapLock.ReadLock();
				AttributeCopy->EntryToValueKeyMap.Append(Current->EntryToValueKeyMap);
				Current->EntryMapLock.ReadUnlock();
			}
		}

		if (bCopyValues)
		{
			// We go backwards, since we need to preserve order (root -> this)
			// Latest entry in our Parents array is the root.
			for (int32 i = Parents.Num() - 1; i >= 0; --i)
			{
				const FPCGMetadataAttribute<T>* Current = Parents[i];
				
				Current->ValueLock.ReadLock();

				if constexpr (std::is_same_v<T, U>)
				{
					AttributeCopy->Values.Append(Current->Values);
				}
				else
				{
					U NewValue{};

					for (const T& Value : Current->Values)
					{
						PCG::Private::GetValueWithBroadcastAndConstructible(Value, NewValue);
						AttributeCopy->Values.Add(NewValue);
					}
				}

				// The expected value key offset is the one for this attribute (i == 0), and only if we
				// keep the parent. Otherwise we don't have any parent, so offset should be kept at 0.
				if (i == 0 && bKeepParent)
				{
					AttributeCopy->ValueKeyOffset = Current->ValueKeyOffset;
				}
				Current->ValueLock.ReadUnlock();
			}
		}

		return AttributeCopy;
	}

	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		if (InAttribute == this)
		{
			SetValueFromValueKey(ItemKey, GetValueKey(InEntryKey));
		}
		else if (InAttribute)
		{
			SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttribute)->GetValueFromItemKey(InEntryKey));
		}
	}

	virtual void SetZeroValue(PCGMetadataEntryKey ItemKey) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		ZeroValue(ItemKey);
	}

	virtual void AccumulateValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey, float Weight) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		Accumulate(ItemKey, InAttribute, InEntryKey, Weight);
	}

	virtual void SetWeightedValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<const TPair<PCGMetadataEntryKey, float>>& InWeightedKeys) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		Accumulate(ItemKey, InAttribute, InWeightedKeys);
	}

	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB, EPCGMetadataOp Op) override
	{
		check(ItemKey != PCGInvalidEntryKey);
		bool bAppliedValue = false;

		if (Op == EPCGMetadataOp::TargetValue && InAttributeB)
		{
			// Take value of second attribute.
			if (InAttributeB == this)
			{
				SetValueFromValueKey(ItemKey, GetValueKey(InEntryKeyB));
			}
			else
			{
				SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB));
			}

			bAppliedValue = true;
		}
		else if (Op == EPCGMetadataOp::SourceValue && InAttributeA)
		{
			// Take value of first attribute.
			if (InAttributeA == this)
			{
				SetValueFromValueKey(ItemKey, GetValueKey(InEntryKeyA));
			}
			else
			{
				SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA));
			}

			bAppliedValue = true;
		}
		else if (InAttributeA && InAttributeB && bAllowsInterpolation)
		{
			// Combine attributes using specified operation.
			if (Op == EPCGMetadataOp::Min)
			{
				bAppliedValue = SetMin(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Max)
			{
				bAppliedValue = SetMax(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Sub)
			{
				bAppliedValue = SetSub(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Add)
			{
				bAppliedValue = SetAdd(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Mul)
			{
				bAppliedValue = SetMul(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
			else if (Op == EPCGMetadataOp::Div)
			{
				bAppliedValue = SetDiv(ItemKey, InAttributeA, InEntryKeyA, InAttributeB, InEntryKeyB);
			}
		}
		else if (InAttributeA && InAttributeB && HasNonDefaultValue(ItemKey))
		{
			// In this case, the current already has a value, in which case we should not update it
			bAppliedValue = true;
		}

		if (bAppliedValue)
		{
			// Nothing to do
		}
		else if (InAttributeA)
		{
			if (InAttributeA == this)
			{
				SetValueFromValueKey(ItemKey, GetValueKey(InEntryKeyA));
			}
			else
			{
				SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA));
			}
		}
		else if (InAttributeB)
		{
			if (InAttributeB == this)
			{
				SetValueFromValueKey(ItemKey, GetValueKey(InEntryKeyB));
			}
			else
			{
				SetValue(ItemKey, static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB));
			}
		}
	}

	virtual bool AreValuesEqualForEntryKeys(PCGMetadataEntryKey EntryKey1, PCGMetadataEntryKey EntryKey2) const override
	{
		return AreValuesEqual(GetValueKey(EntryKey1), GetValueKey(EntryKey2));
	}

	virtual bool AreValuesEqual(PCGMetadataValueKey ValueKey1, PCGMetadataValueKey ValueKey2) const override
	{
		if constexpr (PCG::Private::MetadataTraits<T>::CompressData)
		{
			if (ValueKey1 == PCGInvalidEntryKey)
			{
				return PCG::Private::MetadataTraits<T>::Equal(DefaultValue, GetValue(ValueKey2));
			}
			else if (ValueKey2 == PCGInvalidEntryKey)
			{
				return PCG::Private::MetadataTraits<T>::Equal(GetValue(ValueKey1), DefaultValue);
			}
			else
			{
				return ValueKey1 == ValueKey2;
			}
		}
		else
		{
			return ValueKey1 == ValueKey2 || PCG::Private::MetadataTraits<T>::Equal(GetValue(ValueKey1), GetValue(ValueKey2));
		}
	}

	virtual bool IsEqualToDefaultValue(PCGMetadataValueKey ValueKey) const override
	{
		return PCG::Private::MetadataTraits<T>::Equal(GetValue(ValueKey), DefaultValue);
	}

	virtual void SetDefaultValueToFirstEntry() override
	{
		if (Values.Num() != 1)
		{
			return;
		}

		DefaultValue = Values[0];
	}

	PCGMetadataValueKey GetValueKeyOffsetForChild() const
	{
		FReadScopeLock ScopeLock(ValueLock);
		return Values.Num() + ValueKeyOffset;
	}

	/** Adds the value, returns the value key for the given value */
	PCGMetadataValueKey AddValue(const T& InValue)
	{
		PCGMetadataValueKey FoundValueKey = FindValue(InValue);

		if (FoundValueKey == PCGNotFoundValueKey)
		{
			FWriteScopeLock ScopeLock(ValueLock);
			return Values.Add(InValue) + ValueKeyOffset;
		}
		else
		{
			return FoundValueKey;
		}
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	TArray<PCGMetadataValueKey> AddValues(const TArrayView<const T>& InValues)
	{
		// Since we're getting raw values here, we might have duplicates
		// so we should aim to remove duplicates here so we preserve our 'compress data' idea, otherwise it will break other foundational blocks (e.g. partition)
		TArray<T, TInlineAllocator<256>> UniqueValues;

		// Initially, fill with mapping to unique values so we can remap them at the end if needed
		TArray<PCGMetadataValueKey> FoundValueKeys;
		FoundValueKeys.Reserve(InValues.Num());

		for (int ValueIndex = 0; ValueIndex < InValues.Num(); ++ValueIndex)
		{
			FoundValueKeys.Emplace(UniqueValues.AddUnique(InValues[ValueIndex]));
		}

		const bool bHasDuplicateValues = (UniqueValues.Num() != InValues.Num());

		TArray<PCGMetadataValueKey> FoundUniqueValueKeys;
		TArray<PCGMetadataValueKey>& FoundKeys = (bHasDuplicateValues ? FoundUniqueValueKeys : FoundValueKeys);

		// Implementation note: when we don't have any duplicate values, the previously-set values in FoundValueKeys will be wiped out - this is intended
		const bool bAtLeastOneValueNotFound = !FindValues(UniqueValues, FoundKeys);

		if (bAtLeastOneValueNotFound)
		{
			FWriteScopeLock ScopeLock(ValueLock);
			for (int ValueIndex = 0; ValueIndex < UniqueValues.Num(); ++ValueIndex)
			{
				if (FoundKeys[ValueIndex] == PCGNotFoundValueKey)
				{
					FoundKeys[ValueIndex] = Values.Add(UniqueValues[ValueIndex]) + ValueKeyOffset;
				}
			}
		}

		// Remap to full array if needed
		if (bHasDuplicateValues)
		{
			for (PCGMetadataValueKey& ValueToRemap : FoundValueKeys)
			{
				ValueToRemap = FoundUniqueValueKeys[ValueToRemap];
			}
		}

		return FoundValueKeys;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	TArray<PCGMetadataValueKey> AddValues(const TArrayView<const T>& InValues)
	{
		TArray<PCGMetadataValueKey> ValueKeys;

		int FirstValueIndex = 0;

		ValueLock.WriteLock();
		FirstValueIndex = Values.Num() + ValueKeyOffset;
		Values.Append(InValues);
		ValueLock.WriteUnlock();

		ValueKeys.SetNum(InValues.Num());
		for (int ValueIndex = 0; ValueIndex < InValues.Num(); ++ValueIndex)
		{
			ValueKeys[ValueIndex] = FirstValueIndex + ValueIndex;
		}

		return ValueKeys;
	}

	void SetValue(PCGMetadataEntryKey ItemKey, const T& InValue)
	{
		check(ItemKey != PCGInvalidEntryKey);
		SetValueFromValueKey(ItemKey, AddValue(InValue));
	}

	void SetValues(const TArrayView<const PCGMetadataEntryKey>& ItemKeys, const TArrayView<const T>& InValues)
	{
		SetValuesFromValueKeys(ItemKeys, AddValues(InValues));
	}

	void SetValues(const TArrayView<const PCGMetadataEntryKey * const>& ItemKeys, const TArrayView<const T>& InValues)
	{
		SetValuesFromValueKeys(ItemKeys, AddValues(InValues));
	}

	template<typename U>
	void SetValue(PCGMetadataEntryKey ItemKey, const U& InValue)
	{
		check(ItemKey != PCGInvalidEntryKey);
		SetValueFromValueKey(ItemKey, AddValue(T(InValue)));
	}

	T GetValueFromItemKey(PCGMetadataEntryKey ItemKey) const
	{
		return GetValue(GetValueKey(ItemKey));
	}

	T GetValue(PCGMetadataValueKey ValueKey) const
	{
		if (ValueKey == PCGDefaultValueKey)
		{
			return DefaultValue;
		}
		else if (ValueKey >= ValueKeyOffset)
		{
			int32 Index = ValueKey - ValueKeyOffset;
			FReadScopeLock ScopeLock(ValueLock);
			return Index < Values.Num() ? Values[Index] : DefaultValue;
		}
		else if (GetParent())
		{
			return GetParent()->GetValue(ValueKey);
		}
		else
		{
			return DefaultValue;
		}
	}

	/** Code related to finding values / compressing data */
	virtual bool UsesValueKeys() const override
	{
		return PCG::Private::MetadataTraits<T>::CompressData;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	PCGMetadataValueKey FindValue(const T& InValue) const
	{
		if (InValue == DefaultValue)
		{
			return PCGDefaultValueKey;
		}

		PCGMetadataValueKey ParentValueKey = (GetParent() ? GetParent()->FindValue(InValue) : PCGNotFoundValueKey);
		if (ParentValueKey != PCGNotFoundValueKey)
		{
			return ParentValueKey;
		}
		else
		{
			ValueLock.ReadLock();
			const int32 ValueIndex = Values.FindLast(InValue);
			ValueLock.ReadUnlock();

			if (ValueIndex != INDEX_NONE)
			{
				return ValueIndex + ValueKeyOffset;
			}
			else
			{
				return PCGNotFoundValueKey;
			}
		}
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	bool FindValues(const TArrayView<const T>& InValues, TArray<PCGMetadataValueKey>& OutValueKeys) const
	{
		OutValueKeys.Init(PCGNotFoundValueKey, InValues.Num());

		int ValueKeysSet = 0;
		FindValuesInternal(InValues, OutValueKeys, ValueKeysSet, /*bIsRoot=*/true);

		return (ValueKeysSet == InValues.Num());
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	void FindValuesInternal(const TArrayView<const T>& InValues, TArray<PCGMetadataValueKey>& ValueKeys, int& ValueKeysSet, bool bIsRoot) const
	{
		check(InValues.Num() == ValueKeys.Num());

		if (bIsRoot)
		{
			for (int ValueIndex = 0; ValueIndex < InValues.Num(); ++ValueIndex)
			{
				if (InValues[ValueIndex] == DefaultValue)
				{
					ValueKeys[ValueIndex] = PCGDefaultValueKey;
					++ValueKeysSet;
				}
			}
		}

		if (ValueKeysSet != InValues.Num() && GetParent())
		{
			GetParent()->FindValuesInternal(InValues, ValueKeys, ValueKeysSet, /*bRoot=*/false);
		}

		if (ValueKeysSet != InValues.Num())
		{
			ValueLock.ReadLock();
			for (int ValueIndex = 0; ValueIndex < InValues.Num(); ++ValueIndex)
			{
				if (ValueKeys[ValueIndex] != PCGNotFoundValueKey)
				{
					continue;
				}

				const int32 FoundValueIndex = Values.FindLast(InValues[ValueIndex]);
				if (FoundValueIndex != INDEX_NONE)
				{
					ValueKeys[ValueIndex] = FoundValueIndex + ValueKeyOffset;
					++ValueKeysSet;
				}
			}
			ValueLock.ReadUnlock();
		}
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	PCGMetadataValueKey FindValue(const T& InValue) const
	{
		return PCGNotFoundValueKey;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CompressData>::Type* = nullptr>
	bool FindValues(const TArrayView<const T>& InValues, TArray<PCGMetadataValueKey>& OutValueKeys) const
	{
		OutValueKeys.Init(PCGNotFoundValueKey, InValues.Num());
		return false;
	}

	void SetDefaultValue(const T& Value)
	{
		DefaultValue = Value;
	}

protected:
	/** Code related to computing compared values (min, max, sub, add) */
	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanMinMax>::Type* = nullptr>
	bool SetMin(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey, 
			PCG::Private::MetadataTraits<IT>::Min(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanMinMax>::Type* = nullptr>
	bool SetMin(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanMinMax>::Type* = nullptr>
	bool SetMax(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Max(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanMinMax>::Type* = nullptr>
	bool SetMax(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanSubAdd>::Type* = nullptr>
	bool SetAdd(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Add(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanSubAdd>::Type* = nullptr>
	bool SetAdd(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanSubAdd>::Type* = nullptr>
	bool SetSub(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Sub(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanSubAdd>::Type* = nullptr>
	bool SetSub(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanMulDiv>::Type* = nullptr>
	bool SetMul(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Mul(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanMulDiv>::Type* = nullptr>
	bool SetMul(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanMulDiv>::Type* = nullptr>
	bool SetDiv(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::Div(
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeA)->GetValueFromItemKey(InEntryKeyA),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttributeB)->GetValueFromItemKey(InEntryKeyB)));

		return true;
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanMulDiv>::Type* = nullptr>
	bool SetDiv(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB)
	{
		return false;
	}

	/** Weighted/interpolated values related code */
	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void ZeroValue(PCGMetadataEntryKey ItemKey)
	{
		SetValue(ItemKey, PCG::Private::MetadataTraits<IT>::ZeroValue());
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void ZeroValue(PCGMetadataEntryKey ItemKey)
	{
		// Intentionally empty
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void Accumulate(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey, float Weight)
	{
		SetValue(ItemKey,
			PCG::Private::MetadataTraits<IT>::WeightedSum(
				GetValueFromItemKey(ItemKey),
				static_cast<const FPCGMetadataAttribute<T>*>(InAttribute)->GetValueFromItemKey(InEntryKey),
				Weight));
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void Accumulate(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey, float Weight)
	{
		// Empty on purpose
	}

	template<typename IT = T, typename TEnableIf<PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void Accumulate(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<const TPair<PCGMetadataEntryKey, float>>& InWeightedKeys)
	{
		IT Value = PCG::Private::MetadataTraits<IT>::ZeroValueForWeightedSum();
		for (const TPair<PCGMetadataEntryKey, float>& WeightedEntry : InWeightedKeys)
		{
			Value = PCG::Private::MetadataTraits<IT>::WeightedSum(
				Value,
				static_cast<const FPCGMetadataAttribute<T>*>(InAttribute)->GetValueFromItemKey(WeightedEntry.Key),
				WeightedEntry.Value);
		}

		SetValue(ItemKey, Value);
	}

	template<typename IT = T, typename TEnableIf<!PCG::Private::MetadataTraits<IT>::CanInterpolate>::Type* = nullptr>
	void Accumulate(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<const TPair<PCGMetadataEntryKey, float>>& InWeightedKeys)
	{
		// Empty on purpose
	}

protected:
	mutable FRWLock ValueLock;
	TArray<T> Values;
	T DefaultValue = T{};
	PCGMetadataValueKey ValueKeyOffset = 0;
};

namespace PCGMetadataAttribute
{
	inline FPCGMetadataAttributeBase* AllocateEmptyAttributeFromType(int16 TypeId)
	{

		switch (TypeId)
		{

#define PCG_ALLOCATEEMPTY_DECL(T) case PCG::Private::MetadataTypes<T>::Id: return new FPCGMetadataAttribute<T>();
		PCG_FOREACH_SUPPORTEDTYPES(PCG_ALLOCATEEMPTY_DECL)
#undef PCG_ALLOCATEEMPTY_DECL

		default:
			return nullptr;
		}

#undef AllocatePCGMetadataAttributeOnType
	}

	template <typename Func, typename... Args>
	inline decltype(auto) CallbackWithRightType(uint16 TypeId, Func Callback, Args&& ...InArgs)
	{
		using ReturnType = decltype(Callback(double{}, std::forward<Args>(InArgs)...));

		switch (TypeId)
		{

#define PCG_CALLBACKWITHRIGHTTYPE_DECL(T) case (uint16)(PCG::Private::MetadataTypes<T>::Id): return Callback(T{}, std::forward<Args>(InArgs)...);
		PCG_FOREACH_SUPPORTEDTYPES(PCG_CALLBACKWITHRIGHTTYPE_DECL)
#undef PCG_CALLBACKWITHRIGHTTYPE_DECL

		default:
		{
			// ReturnType{} is invalid if ReturnType is void
			if constexpr (std::is_same_v<ReturnType, void>)
			{
				return;
			}
			else
			{
				return ReturnType{};
			}
		}
		}
	}
}

template<typename T>
FPCGMetadataAttributeBase* FPCGMetadataAttribute<T>::CopyToAnotherType(int16 TargetType) const
{
	return PCGMetadataAttribute::CallbackWithRightType(TargetType, [this](auto Dummy) -> FPCGMetadataAttributeBase*
	{
		using U = decltype(Dummy);

		if constexpr (PCG::Private::IsBroadcastableOrConstructible(PCG::Private::MetadataTypes<T>::Id, PCG::Private::MetadataTypes<U>::Id))
		{
			return CopyInternal<U>(Name, Metadata, /*bKeepParent=*/false, /*bCopyEntries=*/true, /*bCopyValues=*/true);
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Metadata attribute '%s' cannot change its type - delete and create instead"), *Name.ToString());
			return nullptr;
		}
	});
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
