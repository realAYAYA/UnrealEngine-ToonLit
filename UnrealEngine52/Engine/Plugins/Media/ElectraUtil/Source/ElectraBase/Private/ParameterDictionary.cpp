// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParameterDictionary.h"

namespace Electra
{


FVariantValue::FVariantValue()
	: DataType(EDataType::TypeUninitialized)
{
}

FVariantValue::~FVariantValue()
{
	Clear();
}

FVariantValue::FVariantValue(const FVariantValue& rhs)
	: DataType(EDataType::TypeUninitialized)
{
	CopyInternal(rhs);
}

FVariantValue& FVariantValue::operator=(const FVariantValue& rhs)
{
	if (this != &rhs)
	{
		CopyInternal(rhs);
	}
	return *this;
}


FVariantValue::FVariantValue(const FString& StringValue)
	: DataType(EDataType::TypeUninitialized)
{
	Set(StringValue);
}

FVariantValue::FVariantValue(const double DoubleValue)
	: DataType(EDataType::TypeUninitialized)
{
	Set(DoubleValue);
}

FVariantValue::FVariantValue(const int64 Int64Value)
	: DataType(EDataType::TypeUninitialized)
{
	Set(Int64Value);
}

FVariantValue::FVariantValue(const bool BoolValue)
	: DataType(EDataType::TypeUninitialized)
{
	Set(BoolValue);
}

FVariantValue::FVariantValue(const FTimeValue& TimeValue)
	: DataType(EDataType::TypeUninitialized)
{
	Set(TimeValue);
}

FVariantValue::FVariantValue(void* PointerValue)
	: DataType(EDataType::TypeUninitialized)
{
	Set(PointerValue);
}



void FVariantValue::CopyInternal(const FVariantValue& FromOther)
{
	switch(FromOther.DataType)
	{
		case EDataType::TypeUninitialized:
			Clear();
			break;
		case EDataType::TypeFString:
			Set(FromOther.GetFString());
			break;
		case EDataType::TypeDouble:
			Set(FromOther.GetDouble());
			break;
		case EDataType::TypeInt64:
			Set(FromOther.GetInt64());
			break;
		case EDataType::TypeBoolean:
			Set(FromOther.GetBool());
			break;
		case EDataType::TypeTimeValue:
			Set(FromOther.GetTimeValue());
			break;
		case EDataType::TypeVoidPointer:
			Set(FromOther.GetPointer());
			break;
		case EDataType::TypeSharedPointer:
		{
			Clear();
			FSharedPtrHolderBase* Pointer = reinterpret_cast<FSharedPtrHolderBase*>(&DataBuffer);
			const FSharedPtrHolderBase* OtherPointer = reinterpret_cast<const FSharedPtrHolderBase*>(&FromOther.DataBuffer);
			OtherPointer->SetValueOn(Pointer);
			DataType = FromOther.DataType;
			break;
		}
		default:
			Clear();
			check(!"Whoops");
			break;
	}
}


void FVariantValue::Clear()
{
	switch(DataType)
	{
		case EDataType::TypeFString:
		{
			FString* Str = reinterpret_cast<FString*>(&DataBuffer);
			Str->~FString();
			break;
		}
		case EDataType::TypeTimeValue:
		{
			FTimeValue* Time = reinterpret_cast<FTimeValue*>(&DataBuffer);
			Time->~FTimeValue();
			break;
		}
		case EDataType::TypeUninitialized:
		case EDataType::TypeDouble:
		case EDataType::TypeInt64:
		case EDataType::TypeBoolean:
		case EDataType::TypeVoidPointer:
			break;
		case EDataType::TypeSharedPointer:
		{
			FSharedPtrHolderBase* Pointer = reinterpret_cast<FSharedPtrHolderBase*>(&DataBuffer);
			Pointer->~FSharedPtrHolderBase();
			break;
		}
		default:
		{
			check(!"Whoops");
			break;
		}
	}
	DataType = EDataType::TypeUninitialized;
}


FVariantValue& FVariantValue::Set(const FString& StringValue)
{
	Clear();
	FString* Str = reinterpret_cast<FString*>(&DataBuffer);
	new ((void *)Str) FString(StringValue);
	DataType = EDataType::TypeFString;
	return *this;
}

FVariantValue& FVariantValue::Set(const double DoubleValue)
{
	Clear();
	double* ValuePtr = reinterpret_cast<double*>(&DataBuffer);
	*ValuePtr = DoubleValue;
	DataType = EDataType::TypeDouble;
	return *this;
}

FVariantValue& FVariantValue::Set(const int64 Int64Value)
{
	Clear();
	int64* ValuePtr = reinterpret_cast<int64*>(&DataBuffer);
	*ValuePtr = Int64Value;
	DataType = EDataType::TypeInt64;
	return *this;
}

FVariantValue& FVariantValue::Set(const bool BoolValue)
{
	Clear();
	bool* ValuePtr = reinterpret_cast<bool*>(&DataBuffer);
	*ValuePtr = BoolValue;
	DataType = EDataType::TypeBoolean;
	return *this;
}

FVariantValue& FVariantValue::Set(const FTimeValue& TimeValue)
{
	Clear();
	FTimeValue* ValuePtr = reinterpret_cast<FTimeValue*>(&DataBuffer);
	*ValuePtr = TimeValue;
	DataType = EDataType::TypeTimeValue;
	return *this;
}

FVariantValue& FVariantValue::Set(void* PointerValue)
{
	Clear();
	void** ValuePtr = reinterpret_cast<void**>(&DataBuffer);
	*ValuePtr = PointerValue;
	DataType = EDataType::TypeVoidPointer;
	return *this;
}

const FString& FVariantValue::GetFString() const
{
	check(DataType == EDataType::TypeFString);
	if (DataType == EDataType::TypeFString)
	{
		const FString* Str = reinterpret_cast<const FString*>(&DataBuffer);
		return *Str;
	}
	else
	{
		static FString Empty;
		return Empty;
	}
}

const double& FVariantValue::GetDouble() const
{
	check(DataType == EDataType::TypeDouble);
	if (DataType == EDataType::TypeDouble)
	{
		const double* Dbl = reinterpret_cast<const double*>(&DataBuffer);
		return *Dbl;
	}
	else
	{
		static double Empty = 0.0;
		return Empty;
	}
}

const int64& FVariantValue::GetInt64() const
{
	check(DataType == EDataType::TypeInt64);
	if (DataType == EDataType::TypeInt64)
	{
		const int64* Int = reinterpret_cast<const int64*>(&DataBuffer);
		return *Int;
	}
	else
	{
		static int64 Empty = 0;
		return Empty;
	}
}

const bool& FVariantValue::GetBool() const
{
	check(DataType == EDataType::TypeBoolean);
	if (DataType == EDataType::TypeBoolean)
	{
		const bool* Bool = reinterpret_cast<const bool*>(&DataBuffer);
		return *Bool;
	}
	else
	{
		static bool Empty = false;
		return Empty;
	}
}

const FTimeValue& FVariantValue::GetTimeValue() const
{
	check(DataType == EDataType::TypeTimeValue);
	if (DataType == EDataType::TypeTimeValue)
	{
		const FTimeValue* Time = reinterpret_cast<const FTimeValue*>(&DataBuffer);
		return *Time;
	}
	else
	{
		static FTimeValue Empty;
		return Empty;
	}
}

void* const & FVariantValue::GetPointer() const
{
	check(DataType == EDataType::TypeVoidPointer);
	if (DataType == EDataType::TypeVoidPointer)
	{
		void** Pointer = (void**)&DataBuffer;
		return *Pointer;
	}
	else
	{
		static void* Empty = nullptr;
		return Empty;
	}
}

const FString& FVariantValue::SafeGetFString(const FString& Default) const
{
	if (DataType == EDataType::TypeFString)
	{
		const FString* Str = reinterpret_cast<const FString*>(&DataBuffer);
		return *Str;
	}
	return Default;
}

double FVariantValue::SafeGetDouble(double Default) const
{
	if (DataType == EDataType::TypeDouble)
	{
		const double* Dbl = reinterpret_cast<const double*>(&DataBuffer);
		return *Dbl;
	}
	return Default;
}

int64 FVariantValue::SafeGetInt64(int64 Default) const
{
	if (DataType == EDataType::TypeInt64)
	{
		const int64* Int = reinterpret_cast<const int64*>(&DataBuffer);
		return *Int;
	}
	return Default;
}

bool FVariantValue::SafeGetBool(bool Default) const
{
	if (DataType == EDataType::TypeBoolean)
	{
		const bool* Bool = reinterpret_cast<const bool*>(&DataBuffer);
		return *Bool;
	}
	return Default;
}

FTimeValue FVariantValue::SafeGetTimeValue(const FTimeValue& Default) const
{
	if (DataType == EDataType::TypeTimeValue)
	{
		const FTimeValue* Time = reinterpret_cast<const FTimeValue*>(&DataBuffer);
		return *Time;
	}
	return Default;
}

void* FVariantValue::SafeGetPointer(void* Default) const
{
	if (DataType == EDataType::TypeVoidPointer)
	{
		void** Pointer = (void**)&DataBuffer;
		return *Pointer;
	}
	return Default;
}







FParamDict::FParamDict()
{
}

FParamDict::FParamDict(const FParamDict& Other)
{
	InternalCopy(Other);
}

FParamDict& FParamDict::operator=(const FParamDict& Other)
{
	if (&Other != this)
	{
		FScopeLock lock(&Lock);
		InternalCopy(Other);
	}
	return *this;
}

FParamDict::~FParamDict()
{
}

void FParamDict::InternalCopy(const FParamDict& Other)
{
	FScopeLock lock(&Other.Lock);
	Dictionary = Other.Dictionary;
}



void FParamDict::Clear()
{
	FScopeLock lock(&Lock);
	Dictionary.Empty();
}

void FParamDict::Set(const FString& Key, const FVariantValue& Value)
{
	FScopeLock lock(&Lock);
	Dictionary.FindOrAdd(Key, Value);
}

bool FParamDict::HaveKey(const FString& Key) const
{
	FScopeLock lock(&Lock);
	return Dictionary.Find(Key) != nullptr;
}

FVariantValue FParamDict::GetValue(const FString& Key) const
{
	static FVariantValue Empty;
	FScopeLock lock(&Lock);
	const FVariantValue* VariantValue = Dictionary.Find(Key);
	return VariantValue ? *VariantValue : Empty;
}

void FParamDict::GetKeysStartingWith(const FString& StartsWith, TArray<FString>& Keys) const
{
	FScopeLock lock(&Lock);
	Keys.Empty();
	if (StartsWith.IsEmpty())
	{
		Dictionary.GenerateKeyArray(Keys);
	}
	else
	{
		for(const TPair<FString, FVariantValue>& Pair : Dictionary)
		{
			if (Pair.Key.StartsWith(StartsWith, ESearchCase::CaseSensitive))
			{
				Keys.Emplace(Pair.Key);
			}
		}
	}
}

} // namespace Electra


