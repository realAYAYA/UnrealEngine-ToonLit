// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "PlayerTime.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Containers/Array.h"
#include "Misc/Variant.h"


namespace Electra
{

class ELECTRABASE_API FVariantValue
{
	class FSharedPtrHolderBase
	{
	public:
		virtual ~FSharedPtrHolderBase() {}
		virtual void SetValueOn(FSharedPtrHolderBase* Dst) const = 0;
	};

	template<typename T> class TSharedPtrHolder : public FSharedPtrHolderBase
	{
		using SharedPtrType = TSharedPtr<T, ESPMode::ThreadSafe>;

	public:
		TSharedPtrHolder(const SharedPtrType & InPointer)
			: Pointer(InPointer)
		{}

		SharedPtrType	Pointer;

		virtual void SetValueOn(FSharedPtrHolderBase* Dst) const override
		{
			new(Dst) TSharedPtrHolder<T>(reinterpret_cast<const SharedPtrType&>(Pointer));
		}
	};

public:
	FVariantValue();
	~FVariantValue();
	FVariantValue(const FVariantValue& rhs);
	FVariantValue& operator=(const FVariantValue& rhs);

	explicit FVariantValue(const FString& StringValue);
	explicit FVariantValue(double DoubleValue);
	explicit FVariantValue(int64 Int64Value);
	explicit FVariantValue(bool BoolValue);
	explicit FVariantValue(const FTimeValue& TimeValue);
	explicit FVariantValue(void* PointerValue);
	template<typename T> explicit FVariantValue(const TSharedPtr<T, ESPMode::ThreadSafe>& PointerValue)
	: DataType(EDataType::TypeUninitialized)
	{
		Set(PointerValue);
	}
	explicit FVariantValue(const TArray<uint8>& ArrayValue);

	FVariantValue& Set(const FString& StringValue);
	FVariantValue& Set(double DoubleValue);
	FVariantValue& Set(int64 Int64Value);
	FVariantValue& Set(bool BoolValue);
	FVariantValue& Set(const FTimeValue& TimeValue);
	FVariantValue& Set(void* PointerValue);
	template <typename T> FVariantValue& Set(const TSharedPtr<T, ESPMode::ThreadSafe>& PointerValue)
	{
		Clear();
		new(&DataBuffer) TSharedPtrHolder<T>(PointerValue);
		DataType = EDataType::TypeSharedPointer;
		return *this;
	}
	FVariantValue& Set(const TArray<uint8>& ArrayValue);

	// Returns variant value. Type *must* match. Otherwise an empty/zero value is returned.
	const FString& GetFString() const;
	const double& GetDouble() const;
	const int64& GetInt64() const;
	const bool& GetBool() const;
	const FTimeValue& GetTimeValue() const;
	void* const & GetPointer() const;
	template<typename T> TSharedPtr<T, ESPMode::ThreadSafe> GetSharedPointer() const
	{
		check(DataType == EDataType::TypeSharedPointer);
		if (DataType == EDataType::TypeSharedPointer)
		{
			const TSharedPtrHolder<T>* Pointer = reinterpret_cast<const TSharedPtrHolder<T>*>(&DataBuffer);
			return Pointer->Pointer;
		}
		else
		{
			return TSharedPtr<T, ESPMode::ThreadSafe>();
		}
	}
	const TArray<uint8>& GetArray() const;

	// Returns variant value. If type does not match the specified default will be returned.
	const FString& SafeGetFString(const FString& Default = FString()) const;
	double SafeGetDouble(double Default=0.0) const;
	int64 SafeGetInt64(int64 Default=0) const;
	bool SafeGetBool(bool Default=false) const;
	FTimeValue SafeGetTimeValue(const FTimeValue& Default=FTimeValue()) const;
	void* SafeGetPointer(void* Default=nullptr) const;
	const TArray<uint8>& SafeGetArray() const;

	enum class EDataType
	{
		TypeUninitialized,
		TypeFString,
		TypeDouble,
		TypeInt64,
		TypeBoolean,
		TypeTimeValue,
		TypeVoidPointer,
		TypeSharedPointer,
		TypeU8Array,
	};
	EDataType GetDataType() const
	{
		return DataType;
	}

	bool IsValid() const
	{
		return DataType != EDataType::TypeUninitialized;
	}

	bool IsType(EDataType type) const
	{
		return DataType == type;
	}

private:

	union FUnionLayout
	{
		uint8 MemSizeFString[sizeof(FString)];
		uint8 MemSizeDouble[sizeof(double)];
		uint8 MemSizeInt64[sizeof(int64)];
		uint8 MemSizeVoidPtr[sizeof(void*)];
		uint8 MemSizeBoolean[sizeof(bool)];
		uint8 MemSizeTimeValue[sizeof(FTimeValue)];
		uint8 MemSizeSharedPtrValue[sizeof(TSharedPtrHolder<uint8>)];
		uint8 MemSizeTArray[sizeof(TArray<uint8>)];
	};

	void Clear();
	void CopyInternal(const FVariantValue& FromOther);

	TAlignedBytes<sizeof(FUnionLayout), 16> DataBuffer;
	EDataType	DataType;
};



class ELECTRABASE_API FParamDict
{
public:
	FParamDict() {}
	FParamDict(const FParamDict& Other);
	FParamDict& operator=(const FParamDict& Other);
	~FParamDict() = default;
	void Clear();
	void Set(const FName& Key, const FVariantValue& Value);
	void GetKeys(TArray<FName>& OutKeys) const;
	bool HaveKey(const FName& Key) const;
	FVariantValue GetValue(const FName& Key) const;
	void Remove(const FName& Key);

	void ConvertTo(TMap<FString, FVariant>& OutVariantMap, const FString& InAddPrefixToKey) const;
	void ConvertKeysStartingWithTo(TMap<FString, FVariant>& OutVariantMap, const FString& InKeyStartsWith, const FString& InAddPrefixToKey) const;
private:
	void InternalCopy(const FParamDict& Other);
	TMap<FName, FVariantValue> Dictionary;
};

class ELECTRABASE_API FParamDictTS
{
public:
	FParamDictTS() {}
	FParamDictTS(const FParamDictTS& Other)
	{
		Dictionary = Other.Dictionary;
	}
	FParamDictTS& operator=(const FParamDictTS& Other)
	{
		if (&Other != this)
		{
			FScopeLock lock(&Lock);
			Dictionary = Other.Dictionary;
		}
		return *this;
	}
	FParamDictTS& operator=(const FParamDict& Other)
	{
		FScopeLock lock(&Lock);
		Dictionary = Other;
		return *this;
	}

	~FParamDictTS() = default;

	FParamDict GetDictionary() const
	{
		FScopeLock lock(&Lock);
		return Dictionary;
	}

	void Clear()
	{
		FScopeLock lock(&Lock);
		Dictionary.Clear();
	}
	void Set(const FName& Key, const FVariantValue& Value)
	{
		FScopeLock lock(&Lock);
		Dictionary.Set(Key, Value);
	}
	void GetKeys(TArray<FName>& OutKeys) const
	{
		FScopeLock lock(&Lock);
		Dictionary.GetKeys(OutKeys);
	}
	bool HaveKey(const FName& Key) const
	{
		FScopeLock lock(&Lock);
		return Dictionary.HaveKey(Key);
	}
	FVariantValue GetValue(const FName& Key) const
	{
		FScopeLock lock(&Lock);
		return Dictionary.GetValue(Key);
	}
	void Remove(const FName& Key)
	{
		FScopeLock lock(&Lock);
		Dictionary.Remove(Key);
	}
	void ConvertTo(TMap<FString, FVariant>& OutVariantMap, const FString& InAddPrefixToKey) const
	{
		FScopeLock lock(&Lock);
		Dictionary.ConvertTo(OutVariantMap, InAddPrefixToKey);
	}
	void ConvertKeysStartingWithTo(TMap<FString, FVariant>& OutVariantMap, const FString& InKeyStartsWith, const FString& InAddPrefixToKey) const
	{
		FScopeLock lock(&Lock);
		Dictionary.ConvertKeysStartingWithTo(OutVariantMap, InKeyStartsWith, InAddPrefixToKey);
	}
private:
	mutable FCriticalSection Lock;
	FParamDict Dictionary;
};


} // namespace Electra


