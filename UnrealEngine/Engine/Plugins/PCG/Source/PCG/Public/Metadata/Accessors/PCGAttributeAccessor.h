// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPCGAttributeAccessorTpl.h"

#include "Metadata/PCGMetadata.h"

template <typename T> class FPCGMetadataAttribute;


/**
* Templated accessor class for attributes. Will wrap around a typed attribute.
* Key supported: MetadataEntryKey and Points
*/
template <typename T>
class FPCGAttributeAccessor : public IPCGAttributeAccessorT<FPCGAttributeAccessor<T>>
{
public:
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGAttributeAccessor<T>>;

	// Can't write if metadata is null
	FPCGAttributeAccessor(FPCGMetadataAttribute<T>* InAttribute, UPCGMetadata* InMetadata, bool bForceReadOnly = false)
		: Super(/*bInReadOnly=*/ InMetadata == nullptr || bForceReadOnly)
		, Attribute(InAttribute)
		, Metadata(InMetadata)
	{
		check(InAttribute);
	}

	FPCGAttributeAccessor(const FPCGMetadataAttribute<T>* InAttribute, const UPCGMetadata* InMetadata, bool bForceReadOnly = false)
		: Super(/*bInReadOnly=*/ true)
		, Attribute(const_cast<FPCGMetadataAttribute<T>*>(InAttribute))
		, Metadata(const_cast<UPCGMetadata*>(InMetadata))
	{
		check(InAttribute);
	}

	bool GetRangeImpl(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		TArray<const PCGMetadataEntryKey*, TInlineAllocator<256>> EntryKeys;
		EntryKeys.SetNumUninitialized(OutValues.Num());

		TArrayView<const PCGMetadataEntryKey*> EntryKeysView(EntryKeys);
		if (!Keys.GetKeys<PCGMetadataEntryKey>(Index, EntryKeysView))
		{
			return false;
		}

		// TODO: Might be good to hase a "GetValuesFromItemKeys" to try locking less often.
		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			OutValues[i] = Attribute->GetValueFromItemKey(*EntryKeys[i]);
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags)
	{
		TArray<PCGMetadataEntryKey*, TInlineAllocator<256>> EntryKeys;
		EntryKeys.SetNumUninitialized(InValues.Num());
		TArrayView<PCGMetadataEntryKey*> EntryKeysView(EntryKeys);
		if (!Keys.GetKeys<PCGMetadataEntryKey>(Index, EntryKeysView))
		{
			return false;
		}

		int LastDefaultKeyIndex = INDEX_NONE;

		// Implementation note: this is a stripped down version of UPCGMetadata::InitializeOnSet
		for(int EntryIndex = 0; EntryIndex < EntryKeys.Num(); ++EntryIndex)
		{
			PCGMetadataEntryKey& EntryKey = *EntryKeys[EntryIndex];
			if (EntryKey == PCGInvalidEntryKey)
			{
				if (!(Flags & EPCGAttributeAccessorFlags::AllowSetDefaultValue))
				{
					EntryKey = Metadata->AddEntry(); // TODO - replace by AddEntryPlaceholder ?
				}
				else
				{
					LastDefaultKeyIndex = EntryIndex;
				}
			}
			else if (EntryKey < Metadata->GetItemKeyCountForParent())
			{
				EntryKey = Metadata->AddEntry(EntryKey);
			}
		}

		Attribute->SetValues(EntryKeys, InValues);

		if (LastDefaultKeyIndex != INDEX_NONE)
		{
			Attribute->SetDefaultValue(InValues[LastDefaultKeyIndex]);
		}

		return true;
	}

private:
	FPCGMetadataAttribute<T>* Attribute = nullptr;
	UPCGMetadata* Metadata = nullptr;
};
