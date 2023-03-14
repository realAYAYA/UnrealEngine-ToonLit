// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStruct.h"
#include "StructView.h"
#include "InstancedStructArray.generated.h"

/**
 * Array of heterogeneous structs. Can be used as a property, supports serialization,
 * but does not have type customizations (no editing in the UI).
 *
 * If you need UI editable array of heterogeneous structs, use TArray<FInstancedStruct> instead.
 *
 * The array item values and the index to an item are stored in one contiguous block of memory.
 * The size required to specific layout of structs is larger than the sum of their sizes due to alignment,
 * and because the index to the structs is stored along with the value memory. Each item takes extra 16 bytes
 * for index. If your items are roughly same size, a TArray<TVariant<>> might be more performant.
 *
 * Adding new items is more expensive than on regular TArray<>, layout of the structs needs to be updated,
 * and initialization is done via UScriptStruct. Adding and removing items should be done in chunks if possible.
 *
 * The allocation of new items does not allocate extra space as most array implementations do.
 * Use Reserve() to reserve certain sized buffer in bytes if that is applicable to your use case.
 */

USTRUCT()
struct STRUCTUTILS_API FInstancedStructArray
{
	GENERATED_BODY()

	FInstancedStructArray();
	FInstancedStructArray(const FInstancedStructArray& InOther);
	FInstancedStructArray(FInstancedStructArray&& InOther);
	
	~FInstancedStructArray() { Empty(); }

	FInstancedStructArray& operator=(const FInstancedStructArray& InOther);
	FInstancedStructArray& operator=(FInstancedStructArray&& InOther);
	FInstancedStructArray& operator=(TConstArrayView<FInstancedStruct> InItems);
	FInstancedStructArray& operator=(TConstArrayView<FConstStructView> InItems);

	/** Appends items to the array. */
	void Append(const FInstancedStructArray& Other);
	void Append(TConstArrayView<FInstancedStruct> NewItemValues);
	void Append(TConstArrayView<FConstStructView> NewItemValues);

	/** Insert new items at specified location. */
	void InsertAt(const int32 InsertAtIndex, const FInstancedStructArray& Other);
	void InsertAt(const int32 InsertAtIndex, TConstArrayView<FInstancedStruct> ValuesToInsert);
	void InsertAt(const int32 InsertAtIndex, TConstArrayView<FConstStructView> ValuesToInsert);

	/** Remove items at specific location. Does not change memory allocation. */
	void RemoveAt(const int32 RemoveAtIndex, const int32 Count);

	/** Reserves at least 'NumBytes' for internal storage. */
	void ReserveBytes(const int32 NumBytes, const int32 MinAlignment = DefaultMinAlignment);
	
	/** Sets the number of items in the array. Note: this can only shrink the array. Does not change memory allocation. */
	void SetNum(const int32 NewNum);

	/** Returns number of bytes allocated for the array  */
	int32 GetAllocatedMemory() const { return AllocatedSize; }
	
	/** Empties the array, destroys entities. Does not change memory allocation. */
	void Reset();

	/** Empties the array, destroys entities. Frees memory. */
	void Empty();

	/** @return true if the instance is correctly initialized. */
	bool IsValid() const { return NumItems > 0; }

	/** @return Number of items in the instance data. */
	int32 Num() const { return NumItems; }

	/** @return true of the index is in valid range to the array. */
	bool IsValidIndex(const int32 Index) const { return Index >= 0 && Index < NumItems; }
	
	/** @return view to the struct at specified index. */
	FStructView operator[](const int32 Index) const
	{
		check(IsValid() && IsValidIndex(Index));
		const FItem& Item = GetItem(Index);
		return FStructView(Item.ScriptStruct, Memory + Item.Offset);
	}

	/** Type traits */
	void AddStructReferencedObjects(class FReferenceCollector& Collector) const;
	bool Identical(const FInstancedStructArray* Other, uint32 PortFlags) const;
	bool Serialize(FArchive& Ar);
	void GetPreloadDependencies(TArray<UObject*>& OutDeps) const;

private:

	/** Struct describing an item in the array. */
	struct FItem
	{
		FItem() = default;
		FItem(const UScriptStruct* InScriptStruct, const int32 InOffset) : ScriptStruct(InScriptStruct), Offset(InOffset) {}

		int32 GetStructureSize() const
		{
			return ScriptStruct != nullptr ? ScriptStruct->GetStructureSize() : 0;
		}

		int32 GetMinAlignment() const
		{
			return ScriptStruct != nullptr ? ScriptStruct->GetMinAlignment() : 1;
		}

		int32 GetEndOffset() const
		{
			return Offset + GetStructureSize();
		}

		const UScriptStruct* ScriptStruct = nullptr;
		int32 Offset = 0;
	};

	/** Default minimum alignment for any allocation. Pointer sized to prevent trivial allocations due to alignment. */
	static constexpr int32 DefaultMinAlignment = alignof(void*);

	/** Each FItem struct is allocated at the end of the buffer (which is aligned to FItem alignment), and they are stride apart. */
	static constexpr int32 ItemStride = Align(sizeof(FItem), alignof(FItem));

	/** @returns offset of the item in the allocated memory.  */
	int32 GetItemOffset(const int32 Index) const
	{
		check(NumItems > 0 && Index < NumItems);
		return AllocatedSize - ((Index + 1) * ItemStride);
	}

	/** @returns item based on index.  */
	FItem& GetItem(const int32 Index) const
	{
		check(NumItems > 0 && Index < NumItems);
		const int32 Offset = AllocatedSize - ((Index + 1) * ItemStride);
		return *(FItem*)(Memory + Offset);
	}

	/** Memory holding all structs values and items. Values are stored front, and items back. */
	uint8* Memory = nullptr;

	/** Number of bytes allocated for the array. */
	int32 AllocatedSize = 0;

	/** Number of items in the array. */
	int32 NumItems = 0;
};

template<>
struct TStructOpsTypeTraits<FInstancedStructArray> : public TStructOpsTypeTraitsBase2<FInstancedStructArray>
{
	enum
	{
		WithSerializer = true,
		WithIdentical = true,
		WithAddStructReferencedObjects = true,
		WithGetPreloadDependencies = true,
	};
};
