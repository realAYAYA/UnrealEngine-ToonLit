// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "Formatters/BinaryArchiveFormatter.h"
#include "Misc/Build.h"
#include "Misc/Optional.h"
#include "Serialization/Archive.h"
#include "Serialization/StructuredArchiveFwd.h"
#include "Serialization/StructuredArchiveNameHelpers.h"
#include "Serialization/StructuredArchiveSlotBase.h"
#include "Templates/EnableIf.h"
#include "Templates/IsEnumClass.h"

class FName;
class FString;
class FStructuredArchive;
class FStructuredArchiveArray;
class FStructuredArchiveChildReader;
class FStructuredArchiveMap;
class FStructuredArchiveRecord;
class FStructuredArchiveSlot;
class FStructuredArchiveStream;
class FText;
class UObject;
struct FLazyObjectPtr;
struct FObjectPtr;
struct FSoftObjectPath;
struct FSoftObjectPtr;
struct FWeakObjectPtr;
template <class TEnum> class TEnumAsByte;

namespace UE::StructuredArchive::Private
{
	FElementId GetCurrentSlotElementIdImpl(FStructuredArchive& StructuredArchive);
	FArchiveFormatterType& GetFormatterImpl(FStructuredArchive& StructuredArchive);
}

/**
 * Contains a value in the archive; either a field or array/map element. A slot does not know it's name or location,
 * and can merely have a value serialized into it. That value may be a literal (eg. int, float) or compound object
 * (eg. object, array, map).
 */
class CORE_API FStructuredArchiveSlot final : public UE::StructuredArchive::Private::FSlotBase
{
public:
	FStructuredArchiveRecord EnterRecord();
	FStructuredArchiveArray EnterArray(int32& Num);
	FStructuredArchiveStream EnterStream();
	FStructuredArchiveMap EnterMap(int32& Num);
	FStructuredArchiveSlot EnterAttribute(FArchiveFieldName AttributeName);
	TOptional<FStructuredArchiveSlot> TryEnterAttribute(FArchiveFieldName AttributeName, bool bEnterWhenWriting);

	// We don't support chaining writes to a single slot, so this returns void.
	void operator << (uint8& Value);
	void operator << (uint16& Value);
	void operator << (uint32& Value);
	void operator << (uint64& Value);
	void operator << (int8& Value);
	void operator << (int16& Value);
	void operator << (int32& Value);
	void operator << (int64& Value);
	void operator << (float& Value);
	void operator << (double& Value);
	void operator << (bool& Value);
	void operator << (FString& Value);
	void operator << (FName& Value);
	void operator << (UObject*& Value);
	void operator << (FText& Value);
	void operator << (FWeakObjectPtr& Value);
	void operator << (FSoftObjectPtr& Value);
	void operator << (FSoftObjectPath& Value);
	void operator << (FLazyObjectPtr& Value);
	void operator << (FObjectPtr& Value);

	template <typename T>
	FORCEINLINE void operator<<(TEnumAsByte<T>& Value)
	{
		uint8 Tmp = (uint8)Value.GetValue();
		*this << Tmp;
		Value = (T)Tmp;
	}

	template <
		typename EnumType,
		typename = typename TEnableIf<TIsEnumClass<EnumType>::Value>::Type
	>
	FORCEINLINE void operator<<(EnumType& Value)
	{
		*this << (__underlying_type(EnumType)&)Value;
	}

	template <typename T>
	FORCEINLINE void operator<<(UE::StructuredArchive::Private::TNamedAttribute<T> Item)
	{
		EnterAttribute(Item.Name) << Item.Value;
	}

	template <typename T>
	FORCEINLINE void operator<<(UE::StructuredArchive::Private::TOptionalNamedAttribute<T> Item)
	{
		if (TOptional<FStructuredArchiveSlot> Attribute = TryEnterAttribute(Item.Name, Item.Value != Item.Default))
		{
			Attribute.GetValue() << Item.Value;
		}
		else
		{
			Item.Value = Item.Default;
		}
	}

	void Serialize(TArray<uint8>& Data);
	void Serialize(void* Data, uint64 DataSize);

	FORCEINLINE bool IsFilled() const
	{
#if WITH_TEXT_ARCHIVE_SUPPORT
		return UE::StructuredArchive::Private::GetCurrentSlotElementIdImpl(StructuredArchive) != ElementId;
#else
		return true;
#endif
	}

private:
	friend FStructuredArchive;
	friend FStructuredArchiveChildReader;
	friend FStructuredArchiveSlot;
	friend FStructuredArchiveRecord;
	friend FStructuredArchiveArray;
	friend FStructuredArchiveStream;
	friend FStructuredArchiveMap;

	using UE::StructuredArchive::Private::FSlotBase::FSlotBase;
};

/**
 * Represents a record in the structured archive. An object contains slots that are identified by FArchiveName,
 * which may be compiled out with binary-only archives.
 */
class CORE_API FStructuredArchiveRecord final : public UE::StructuredArchive::Private::FSlotBase
{
public:
	FStructuredArchiveSlot EnterField(FArchiveFieldName Name);
	FStructuredArchiveRecord EnterRecord(FArchiveFieldName Name);
	FStructuredArchiveArray EnterArray(FArchiveFieldName Name, int32& Num);
	FStructuredArchiveStream EnterStream(FArchiveFieldName Name);
	FStructuredArchiveMap EnterMap(FArchiveFieldName Name, int32& Num);

	TOptional<FStructuredArchiveSlot> TryEnterField(FArchiveFieldName Name, bool bEnterForSaving);

	template<typename T> FORCEINLINE FStructuredArchiveRecord& operator<<(UE::StructuredArchive::Private::TNamedValue<T> Item)
	{
		EnterField(Item.Name) << Item.Value;
		return *this;
	}

private:
	friend FStructuredArchive;
	friend FStructuredArchiveSlot;

	using UE::StructuredArchive::Private::FSlotBase::FSlotBase;
};

/**
 * Represents an array in the structured archive. An object contains slots that are identified by a FArchiveFieldName,
 * which may be compiled out with binary-only archives.
 */
class CORE_API FStructuredArchiveArray final : public UE::StructuredArchive::Private::FSlotBase
{
public:
	FStructuredArchiveSlot EnterElement();

	template<typename T> FORCEINLINE FStructuredArchiveArray& operator<<(T& Item)
	{
		EnterElement() << Item;
		return *this;
	}

private:
	friend FStructuredArchive;
	friend FStructuredArchiveSlot;

	using UE::StructuredArchive::Private::FSlotBase::FSlotBase;
};

/**
 * Represents an unsized sequence of slots in the structured archive (similar to an array, but without a known size).
 */
class CORE_API FStructuredArchiveStream final : public UE::StructuredArchive::Private::FSlotBase
{
public:
	FStructuredArchiveSlot EnterElement();

	template<typename T> FORCEINLINE FStructuredArchiveStream& operator<<(T& Item)
	{
		EnterElement() << Item;
		return *this;
	}

private:
	friend FStructuredArchive;
	friend FStructuredArchiveSlot;

	using UE::StructuredArchive::Private::FSlotBase::FSlotBase;
};

/**
 * Represents a map in the structured archive. A map is similar to a record, but keys can be read back out from an archive.
 * (This is an important distinction for binary archives).
 */
class CORE_API FStructuredArchiveMap final : public UE::StructuredArchive::Private::FSlotBase
{
public:
	FStructuredArchiveSlot EnterElement(FString& Name);

private:
	friend FStructuredArchive;
	friend FStructuredArchiveSlot;

	using UE::StructuredArchive::Private::FSlotBase::FSlotBase;
};

template <typename T>
FORCEINLINE_DEBUGGABLE void operator<<(FStructuredArchiveSlot Slot, TArray<T>& InArray)
{
	int32 NumElements = InArray.Num();
	FStructuredArchiveArray Array = Slot.EnterArray(NumElements);

	if (Slot.GetArchiveState().IsLoading())
	{
		InArray.SetNum(NumElements);
	}

	for (int32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
	{
		FStructuredArchiveSlot ElementSlot = Array.EnterElement();
		ElementSlot << InArray[ElementIndex];
	}
}

template <>
FORCEINLINE_DEBUGGABLE void operator<<(FStructuredArchiveSlot Slot, TArray<uint8>& InArray)
{
	Slot.Serialize(InArray);
}

#if !WITH_TEXT_ARCHIVE_SUPPORT
	//////////// FStructuredArchiveSlot ////////////
	FORCEINLINE FStructuredArchiveRecord FStructuredArchiveSlot::EnterRecord()
	{
		return FStructuredArchiveRecord(FStructuredArchiveRecord::EPrivateToken{}, StructuredArchive);
	}

	FORCEINLINE FStructuredArchiveArray FStructuredArchiveSlot::EnterArray(int32& Num)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).EnterArray(Num);
		return FStructuredArchiveArray(FStructuredArchiveArray::EPrivateToken{}, StructuredArchive);
	}

	FORCEINLINE FStructuredArchiveStream FStructuredArchiveSlot::EnterStream()
	{
		return FStructuredArchiveStream(FStructuredArchiveStream::EPrivateToken{}, StructuredArchive);
	}

	FORCEINLINE FStructuredArchiveMap FStructuredArchiveSlot::EnterMap(int32& Num)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).EnterMap(Num);
		return FStructuredArchiveMap(FStructuredArchiveMap::EPrivateToken{}, StructuredArchive);
	}

	FORCEINLINE FStructuredArchiveSlot FStructuredArchiveSlot::EnterAttribute(FArchiveFieldName FieldName)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).EnterAttribute(FieldName);
		return FStructuredArchiveSlot(FStructuredArchiveSlot::EPrivateToken{}, StructuredArchive);
	}

	FORCEINLINE TOptional<FStructuredArchiveSlot> FStructuredArchiveSlot::TryEnterAttribute(FArchiveFieldName FieldName, bool bEnterWhenWriting)
	{
		if (UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).TryEnterAttribute(FieldName, bEnterWhenWriting))
		{
			return TOptional<FStructuredArchiveSlot>(InPlace, FStructuredArchiveSlot::EPrivateToken{}, StructuredArchive);
		}
		else
		{
			return TOptional<FStructuredArchiveSlot>();
		}
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (uint8& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (uint16& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (uint32& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (uint64& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (int8& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (int16& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (int32& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (int64& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (float& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (double& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (bool& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (FString& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (FName& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (UObject*& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (FText& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (FWeakObjectPtr& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (FSoftObjectPath& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (FSoftObjectPtr& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (FLazyObjectPtr& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::operator<< (FObjectPtr& Value)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Value);
	}

	FORCEINLINE void FStructuredArchiveSlot::Serialize(TArray<uint8>& Data)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Data);
	}

	FORCEINLINE void FStructuredArchiveSlot::Serialize(void* Data, uint64 DataSize)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).Serialize(Data, DataSize);
	}

	//////////// FStructuredArchiveRecord ////////////

	FORCEINLINE FStructuredArchiveSlot FStructuredArchiveRecord::EnterField(FArchiveFieldName Name)
	{
		return FStructuredArchiveSlot(FStructuredArchiveSlot::EPrivateToken{}, StructuredArchive);
	}

	FORCEINLINE FStructuredArchiveRecord FStructuredArchiveRecord::EnterRecord(FArchiveFieldName Name)
	{
		return EnterField(Name).EnterRecord();
	}

	FORCEINLINE FStructuredArchiveArray FStructuredArchiveRecord::EnterArray(FArchiveFieldName Name, int32& Num)
	{
		return EnterField(Name).EnterArray(Num);
	}

	FORCEINLINE FStructuredArchiveStream FStructuredArchiveRecord::EnterStream(FArchiveFieldName Name)
	{
		return EnterField(Name).EnterStream();
	}

	FORCEINLINE FStructuredArchiveMap FStructuredArchiveRecord::EnterMap(FArchiveFieldName Name, int32& Num)
	{
		return EnterField(Name).EnterMap(Num);
	}

	FORCEINLINE TOptional<FStructuredArchiveSlot> FStructuredArchiveRecord::TryEnterField(FArchiveFieldName Name, bool bEnterWhenWriting)
	{
		if (UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).TryEnterField(Name, bEnterWhenWriting))
		{
			return TOptional<FStructuredArchiveSlot>(InPlace, FStructuredArchiveSlot(FStructuredArchiveSlot::EPrivateToken{}, StructuredArchive));
		}
		else
		{
			return TOptional<FStructuredArchiveSlot>();
		}
	}

	//////////// FStructuredArchiveArray ////////////

	FORCEINLINE FStructuredArchiveSlot FStructuredArchiveArray::EnterElement()
	{
		return FStructuredArchiveSlot(FStructuredArchiveSlot::EPrivateToken{}, StructuredArchive);
	}

	//////////// FStructuredArchiveStream ////////////

	FORCEINLINE FStructuredArchiveSlot FStructuredArchiveStream::EnterElement()
	{
		return FStructuredArchiveSlot(FStructuredArchiveSlot::EPrivateToken{}, StructuredArchive);
	}

	//////////// FStructuredArchiveMap ////////////

	FORCEINLINE FStructuredArchiveSlot FStructuredArchiveMap::EnterElement(FString& Name)
	{
		UE::StructuredArchive::Private::GetFormatterImpl(StructuredArchive).EnterMapElement(Name);
		return FStructuredArchiveSlot(FStructuredArchiveSlot::EPrivateToken{}, StructuredArchive);
	}

#endif
