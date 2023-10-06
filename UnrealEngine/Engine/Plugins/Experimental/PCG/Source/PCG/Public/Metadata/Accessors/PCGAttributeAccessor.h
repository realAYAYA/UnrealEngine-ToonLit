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
	FPCGAttributeAccessor(FPCGMetadataAttribute<T>* InAttribute, UPCGMetadata* InMetadata)
		: Super(/*bInReadOnly=*/ InMetadata == nullptr)
		, Attribute(InAttribute)
		, Metadata(InMetadata)
	{
		check(InAttribute);
	}

	FPCGAttributeAccessor(const FPCGMetadataAttribute<T>* InAttribute, const UPCGMetadata* InMetadata)
		: Super(/*bInReadOnly=*/ true)
		, Attribute(const_cast<FPCGMetadataAttribute<T>*>(InAttribute))
		, Metadata(const_cast<UPCGMetadata*>(InMetadata))
	{
		check(InAttribute);
	}

	bool GetRangeImpl(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		TArray<const PCGMetadataEntryKey*> EntryKeys;
		EntryKeys.SetNum(OutValues.Num());
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
		TArray<PCGMetadataEntryKey*> EntryKeys;
		EntryKeys.SetNum(InValues.Num());
		TArrayView<PCGMetadataEntryKey*> EntryKeysView(EntryKeys);
		if (!Keys.GetKeys<PCGMetadataEntryKey>(Index, EntryKeysView))
		{
			return false;
		}

		// TODO: Same than above (avoid locking too many times), but perhaps will be a bit more complex, because of
		// the added logic with PCGInvalidEntryKey, AddEntry and SetDefaultValue.
		for (int32 i = 0; i < InValues.Num(); ++i)
		{
			PCGMetadataEntryKey& EntryKey = *EntryKeys[i];
			if (EntryKey != PCGInvalidEntryKey || !(Flags & EPCGAttributeAccessorFlags::AllowSetDefaultValue))
			{
				// TODO: This part seems costly for a lot of entries. Maybe there are some optimizations to do.
				if (EntryKey == PCGInvalidEntryKey)
				{
					EntryKey = Metadata->AddEntry();
				}

				Attribute->SetValue(EntryKey, InValues[i]);
			}
			else
			{
				Attribute->SetDefaultValue(InValues[i]);
			}
		}

		return true;
	}

private:
	FPCGMetadataAttribute<T>* Attribute = nullptr;
	UPCGMetadata* Metadata = nullptr;
};
