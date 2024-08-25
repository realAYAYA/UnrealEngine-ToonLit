// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/PCGMetadataCommon.h"


struct FPCGPoint;

class FPCGMetadataAttributeBase;
class UPCGMetadata;

///////////////////////////////////////////////////////////////////////

namespace PCGAttributeAccessorKeys
{
	template <typename T, typename Container, typename Func>
	bool GetKeys(Container& InContainer, int32 InStart, TArrayView<T*>& OutItems, Func&& Transform)
	{
		if (InContainer.Num() == 0)
		{
			return false;
		}

		int32 Current = InStart;
		if (Current >= InContainer.Num())
		{
			Current %= InContainer.Num();
		}

		for (int32 i = 0; i < OutItems.Num(); ++i)
		{
			OutItems[i] = Transform(InContainer[Current++]);
			if (Current >= InContainer.Num())
			{
				Current = 0;
			}
		}

		return true;
	}
}

///////////////////////////////////////////////////////////////////////

/**
* Base class to identify keys to use with an accessor.
*/
class PCG_API IPCGAttributeAccessorKeys
{
public:
	explicit IPCGAttributeAccessorKeys(bool bInReadOnly)
		: bIsReadOnly(bInReadOnly)
	{}

	virtual ~IPCGAttributeAccessorKeys() = default;

	/**
	* Retrieve in the given view pointers of the wanted type
	* Need to be a supported type, such as FPCGPoint, PCGMetadataEntryKey or void.
	* It will wrap around if the index/range goes outside the number of keys.
	* @param InStart - Index to start looking in the keys.
	* @param OutKeys - View on the out keys. Its size will indicate the number of elements to get.
	* @return true if it succeeded, false otherwise. (like num == 0,unsupported type or read only)
	*/
	template <typename ObjectType>
	bool GetKeys(int32 InStart, TArrayView<ObjectType*>& OutKeys);

	// Same function but const.
	template <typename ObjectType>
	bool GetKeys(int32 InStart, TArrayView<const ObjectType*>& OutKeys) const;

	/**
	* Retrieve in the given argument pointer of the wanted type.
	* Need to be a supported type, such as FPCGPoint, PCGMetadataEntryKey or void.
	* It will wrap around if the index goes outside the number of keys.
	* @param InStart - Index to start looking in the keys.
	* @param OutObject - Reference on a pointer, where the key will be written.
	* @return true if it succeeded, false otherwise. (like num == 0, unsupported type or read only)
	*/
	template <typename ObjectType>
	bool GetKey(int32 InStart, ObjectType*& OutObject)
	{
		return GetKeys(InStart, TArrayView<ObjectType*>(&OutObject, 1));
	}

	// Same function but const
	template <typename ObjectType>
	bool GetKey(int32 InStart, ObjectType const*& OutObject)
	{
		return GetKeys(InStart, TArrayView<ObjectType*>(&OutObject, 1));
	}

	/**
	* Retrieve in the given argument pointer of the wanted type at the index 0.
	* Need to be a supported type, such as FPCGPoint, PCGMetadataEntryKey or void.
	* @param OutObject - Reference on a pointer, where the key will be written.
	* @return true if it succeeded, false otherwise. (like num == 0, unsupported type or read only)
	*/
	template <typename ObjectType>
	bool GetKey(ObjectType*& OutObject)
	{
		return GetKeys(/*InStart=*/ 0, TArrayView<ObjectType*>(&OutObject, 1));
	}

	// Same function but const
	template <typename ObjectType>
	bool GetKey(ObjectType const*& OutObject) const
	{
		return GetKeys(/*InStart=*/ 0, TArrayView<const ObjectType*>(&OutObject, 1));
	}

	/*
	* Returns the number of keys.
	*/
	virtual int32 GetNum() const = 0;

	bool IsReadOnly() const { return bIsReadOnly; }

protected:
	virtual bool GetPointKeys(int32 InStart, TArrayView<FPCGPoint*>& OutPoints) { return false; }
	virtual bool GetPointKeys(int32 InStart, TArrayView<const FPCGPoint*>& OutPoints) const { return false; }

	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<void*>& OutObjects) { return false; }
	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<const void*>& OutObjects) const { return false; }

	virtual bool GetMetadataEntryKeys(int32 InStart, TArrayView<PCGMetadataEntryKey*>& OutEntryKeys) { return false; }
	virtual bool GetMetadataEntryKeys(int32 InStart, TArrayView<const PCGMetadataEntryKey*>& OutEntryKeys) const { return false; }

	bool bIsReadOnly = false;
};

///////////////////////////////////////////////////////////////////////

/**
* Key around a metadata entry key
*/
class PCG_API FPCGAttributeAccessorKeysEntries : public IPCGAttributeAccessorKeys
{
public:
	UE_DEPRECATED(5.5, "This key accessor is deprecated and replaced by the one taking a const or non-const UPCGMetadata object instead")
	explicit FPCGAttributeAccessorKeysEntries(const FPCGMetadataAttributeBase* Attribute);

	explicit FPCGAttributeAccessorKeysEntries(PCGMetadataEntryKey EntryKey);
	explicit FPCGAttributeAccessorKeysEntries(const TArrayView<PCGMetadataEntryKey>& InEntries);
	explicit FPCGAttributeAccessorKeysEntries(const TArrayView<const PCGMetadataEntryKey>& InEntries);

	// Iterates on all the entries in the metadata.
	explicit FPCGAttributeAccessorKeysEntries(const UPCGMetadata* Metadata);
	explicit FPCGAttributeAccessorKeysEntries(UPCGMetadata* Metadata);

	virtual int32 GetNum() const override { return Entries.Num(); }

protected:
	virtual bool GetMetadataEntryKeys(int32 InStart, TArrayView<PCGMetadataEntryKey*>& OutEntryKeys) override;
	virtual bool GetMetadataEntryKeys(int32 InStart, TArrayView<const PCGMetadataEntryKey*>& OutEntryKeys) const override;

	void InitializeFromMetadata(const UPCGMetadata* Metadata);

	TArrayView<PCGMetadataEntryKey> Entries;
	TArray<PCGMetadataEntryKey> ExtractedEntries;
};

///////////////////////////////////////////////////////////////////////

/**
* Key around points
*/
class PCG_API FPCGAttributeAccessorKeysPoints : public IPCGAttributeAccessorKeys
{
public:
	FPCGAttributeAccessorKeysPoints(const TArrayView<FPCGPoint>& InPoints);
	FPCGAttributeAccessorKeysPoints(const TArrayView<const FPCGPoint>& InPoints);

	explicit FPCGAttributeAccessorKeysPoints(FPCGPoint& InPoint);
	explicit FPCGAttributeAccessorKeysPoints(const FPCGPoint& InPoint);

	virtual int32 GetNum() const override { return Points.Num(); }

protected:
	virtual bool GetPointKeys(int32 InStart, TArrayView<FPCGPoint*>& OutPoints) override;
	virtual bool GetPointKeys(int32 InStart, TArrayView<const FPCGPoint*>& OutPoints) const override;

	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<void*>& OutObjects) override;
	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<const void*>& OutObjects) const override;

	virtual bool GetMetadataEntryKeys(int32 InStart, TArrayView<PCGMetadataEntryKey*>& OutEntryKeys) override;
	virtual bool GetMetadataEntryKeys(int32 InStart, TArrayView<const PCGMetadataEntryKey*>& OutEntryKeys) const override;

	TArrayView<FPCGPoint> Points;
};

/////////////////////////////////////////////////////////////////

/**
* Key around generic objects. 
* Make sure ObjectType is not a pointer nor a reference, since we convert those to void*, it could lead to
* very bad situations if we try to convert a T** to a void*.
*/
template <typename ObjectType, typename = typename std::enable_if_t<!std::is_pointer_v<ObjectType> && !std::is_reference_v<ObjectType>>>
class FPCGAttributeAccessorKeysGeneric : public IPCGAttributeAccessorKeys
{
public:
	FPCGAttributeAccessorKeysGeneric(const TArrayView<ObjectType>& InObjects)
		: IPCGAttributeAccessorKeys(/*bInReadOnly=*/ false)
		, Objects(InObjects)
	{}

	FPCGAttributeAccessorKeysGeneric(const TArrayView<const ObjectType>& InObjects)
		: IPCGAttributeAccessorKeys(/*bInReadOnly=*/ true)
		, Objects(const_cast<ObjectType*>(InObjects.GetData()), InObjects.Num())
	{}

	explicit FPCGAttributeAccessorKeysGeneric(ObjectType& InObject)
		: FPCGAttributeAccessorKeysGeneric(TArrayView<ObjectType>(&InObject, 1))
	{}

	explicit FPCGAttributeAccessorKeysGeneric(const ObjectType& InObject)
		: FPCGAttributeAccessorKeysGeneric(TArrayView<const ObjectType>(&InObject, 1))
	{}

	virtual int32 GetNum() const override { return Objects.Num(); }

protected:
	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<void*>& OutObjects) override
	{
		return PCGAttributeAccessorKeys::GetKeys(Objects, InStart, OutObjects, [](ObjectType& Obj) -> ObjectType* { return &Obj; });
	}

	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<const void*>& OutObjects) const override
	{
		return PCGAttributeAccessorKeys::GetKeys(Objects, InStart, OutObjects, [](const ObjectType& Obj) -> const ObjectType* { return &Obj; });
	}

	TArrayView<ObjectType> Objects;
};

/////////////////////////////////////////////////////////////////

/**
* Unique Key around a single object.
* Necessary if ObjectType is void, but keep a template version for completeness.
* Useful when you want to use the accessors Get/Set methods on a single object.
* Make sure ObjectType is not a pointer nor a reference, since we convert those to void*, it could lead to
* very bad situations if we try to convert a T** to a void*.
*/
template <typename ObjectType, typename = typename std::enable_if_t<!std::is_pointer_v<ObjectType> && !std::is_reference_v<ObjectType>>>
class FPCGAttributeAccessorKeysSingleObjectPtr : public IPCGAttributeAccessorKeys
{
public:

	FPCGAttributeAccessorKeysSingleObjectPtr()
		: IPCGAttributeAccessorKeys(/*bInReadOnly=*/ true)
		, Ptr(nullptr)
	{}

	explicit FPCGAttributeAccessorKeysSingleObjectPtr(ObjectType* InPtr)
		: IPCGAttributeAccessorKeys(/*bInReadOnly=*/ false)
		, Ptr(InPtr)
	{}

	explicit FPCGAttributeAccessorKeysSingleObjectPtr(const ObjectType* InPtr)
		: IPCGAttributeAccessorKeys(/*bInReadOnly=*/ true)
		, Ptr(const_cast<ObjectType*>(InPtr))
	{}

	virtual int32 GetNum() const override { return 1; }

protected:
	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<void*>& OutObjects) override
	{
		if (Ptr == nullptr)
		{
			return false;
		}

		for (int32 i = 0; i < OutObjects.Num(); ++i)
		{
			OutObjects[i] = Ptr;
		}

		return true;
	}

	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<const void*>& OutObjects) const override
	{
		if (Ptr == nullptr)
		{
			return false;
		}

		for (int32 i = 0; i < OutObjects.Num(); ++i)
		{
			OutObjects[i] = Ptr;
		}

		return true;
	}

	ObjectType* Ptr = nullptr;
};

/////////////////////////////////////////////////////////////////

/**
* Type erasing generic keys. Allow to store void* keys, if we are dealing with addresses instead
* of plain objects.
* We can't use FPCGAttributeAccessorKeysGeneric since it has a constructor taking a reference on a object,
* and you can't have void&.
*/
class PCG_API FPCGAttributeAccessorKeysGenericPtrs : public IPCGAttributeAccessorKeys
{
public:
	FPCGAttributeAccessorKeysGenericPtrs(const TArrayView<void*>& InPtrs)
		: IPCGAttributeAccessorKeys(/*bInReadOnly=*/ false)
		, Ptrs(InPtrs)
	{}

	FPCGAttributeAccessorKeysGenericPtrs(const TArrayView<const void*>& InPtrs)
		: IPCGAttributeAccessorKeys(/*bInReadOnly=*/ true)
		, Ptrs(const_cast<void**>(InPtrs.GetData()), InPtrs.Num())
	{}

	virtual int32 GetNum() const override { return Ptrs.Num(); }

protected:
	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<void*>& OutObjects) override
	{
		return PCGAttributeAccessorKeys::GetKeys(Ptrs, InStart, OutObjects, [](void* Ptr) -> void* { return Ptr; });
	}

	virtual bool GetGenericObjectKeys(int32 InStart, TArrayView<const void*>& OutObjects) const override
	{
		return PCGAttributeAccessorKeys::GetKeys(Ptrs, InStart, OutObjects, [](const void* Ptr) -> const void* { return Ptr; });
	}

	TArrayView<void*> Ptrs;
};

template <typename ObjectType>
inline bool IPCGAttributeAccessorKeys::GetKeys(int32 InStart, TArrayView<ObjectType*>& OutKeys)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IPCGAttributeAccessorKeys::GetKeys);

	if (bIsReadOnly)
	{
		return false;
	}

	if constexpr (std::is_same_v<ObjectType, FPCGPoint>)
	{
		return GetPointKeys(InStart, OutKeys);
	}
	else if constexpr (std::is_same_v<ObjectType, PCGMetadataEntryKey>)
	{
		return GetMetadataEntryKeys(InStart, OutKeys);
	}
	else if constexpr (std::is_same_v<ObjectType, void>)
	{
		return GetGenericObjectKeys(InStart, OutKeys);
	}
	else
	{
		return false;
	}
}

template <typename ObjectType>
inline bool IPCGAttributeAccessorKeys::GetKeys(int32 InStart, TArrayView<const ObjectType*>& OutKeys) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IPCGAttributeAccessorKeys::GetKeys);

	if constexpr (std::is_same_v<ObjectType, FPCGPoint>)
	{
		return GetPointKeys(InStart, OutKeys);
	}
	else if constexpr (std::is_same_v<ObjectType, PCGMetadataEntryKey>)
	{
		return GetMetadataEntryKeys(InStart, OutKeys);
	}
	else if constexpr (std::is_same_v<ObjectType, void>)
	{
		return GetGenericObjectKeys(InStart, OutKeys);
	}
	else
	{
		return false;
	}
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "PCGPoint.h"
#endif
