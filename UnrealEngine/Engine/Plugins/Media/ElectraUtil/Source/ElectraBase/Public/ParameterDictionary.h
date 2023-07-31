// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "PlayerTime.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Containers/Array.h"


namespace Electra
{

class ELECTRABASE_API FVariantValue
{
	class FSharedPtrHolderBase
	{
	public:
		virtual ~FSharedPtrHolderBase() {}
		virtual void SetValueOn(FSharedPtrHolderBase *Dst) const = 0;
	};

	template<typename T> class TSharedPtrHolder : public FSharedPtrHolderBase
	{
		using SharedPtrType = TSharedPtr<T, ESPMode::ThreadSafe>;

	public:
		TSharedPtrHolder(const SharedPtrType & InPointer)
			: Pointer(InPointer)
		{}

		SharedPtrType	Pointer;

		virtual void SetValueOn(FSharedPtrHolderBase *Dst) const override
		{
			new(Dst) TSharedPtrHolder<T>(reinterpret_cast<const SharedPtrType&>(Pointer));
		}
	};

public:
	FVariantValue();
	~FVariantValue();
	FVariantValue(const FVariantValue& rhs);
	FVariantValue& operator=(const FVariantValue&rhs);

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

	// Returns variant value. If type does not match the specified default will be returned.
	const FString& SafeGetFString(const FString& Default = FString()) const;
	double SafeGetDouble(double Default=0.0) const;
	int64 SafeGetInt64(int64 Default=0) const;
	bool SafeGetBool(bool Default=false) const;
	FTimeValue SafeGetTimeValue(const FTimeValue& Default=FTimeValue()) const;
	void* SafeGetPointer(void* Default=nullptr) const;

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
	};

	void Clear();
	void CopyInternal(const FVariantValue& FromOther);

	TAlignedBytes<sizeof(FUnionLayout), 16> DataBuffer;
	EDataType	DataType;
};



class ELECTRABASE_API FParamDict
{
public:
	FParamDict();
	FParamDict(const FParamDict& Other);
	FParamDict& operator=(const FParamDict& Other);
	~FParamDict();
	void Clear();
	void Set(const FString& Key, const FVariantValue& Value);
	bool HaveKey(const FString& Key) const;
	FVariantValue GetValue(const FString& Key) const;
	void Remove(const FString& Key)
	{
		FScopeLock lock(&Lock);
		Dictionary.Remove(Key); 
	}
	void SetOrUpdate(const FString& Key, const FVariantValue& Value)
	{ 
		FScopeLock lock(&Lock);
		Dictionary.Add(Key, Value); 
	}

	void GetKeysStartingWith(const FString& StartsWith, TArray<FString>& Keys) const;
private:
	void InternalCopy(const FParamDict& Other);
	mutable FCriticalSection Lock;
	TMap<FString, FVariantValue> Dictionary;
};


} // namespace Electra


