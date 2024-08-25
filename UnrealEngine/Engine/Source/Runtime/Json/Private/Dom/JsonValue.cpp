// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

double FJsonValue::AsNumber() const
{
	double Number = 0.0;

	if (!TryGetNumber(Number))
	{
		ErrorMessage(TEXT("Number"));
	}

	return Number;
}


FString FJsonValue::AsString() const 
{
	FString String;

	if (!TryGetString(String))
	{
		ErrorMessage(TEXT("String"));
	}

	return String;
}


bool FJsonValue::AsBool() const 
{
	bool Bool = false;

	if (!TryGetBool(Bool))
	{
		ErrorMessage(TEXT("Boolean")); 
	}

	return Bool;
}


const TArray< TSharedPtr<FJsonValue> >& FJsonValue::AsArray() const
{
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;

	if (!TryGetArray(Array))
	{
		static const TArray< TSharedPtr<FJsonValue> > EmptyArray;
		Array = &EmptyArray;
		ErrorMessage(TEXT("Array"));
	}

	return *Array;
}


const TSharedPtr<FJsonObject>& FJsonValue::AsObject() const
{
	const TSharedPtr<FJsonObject>* Object = nullptr;

	if (!TryGetObject(Object))
	{
		static const TSharedPtr<FJsonObject> EmptyObject = MakeShared<FJsonObject>();
		Object = &EmptyObject;
		ErrorMessage(TEXT("Object"));
	}

	return *Object;
}

// -----------------------------------

template <typename T>
bool TryConvertNumber(const FJsonValue& InValue, T& OutNumber)
{
	double Double;

	if (InValue.TryGetNumber(Double) && (Double >= TNumericLimits<T>::Min()) && (Double <= static_cast<double>(TNumericLimits<T>::Max())))
	{
		OutNumber = static_cast<T>(FMath::RoundHalfFromZero(Double));

		return true;
	}

	return false;
}

// Need special handling for int64/uint64, due to overflow in the numeric limits.
// 2^63-1 and 2^64-1 cannot be exactly represented as a double, so TNumericLimits<>::Max() gets rounded up to exactly 2^63 or 2^64 by the compiler's implicit cast to double.
// This breaks the overflow check in TryConvertNumber. We use "<" rather than "<=" along with the exact power-of-two double literal to fix this.
template <> bool TryConvertNumber<uint64>(const FJsonValue& InValue, uint64& OutNumber)
{
	double Double;
	if (InValue.TryGetNumber(Double) && Double >= 0.0 && Double < 18446744073709551616.0)
	{
		OutNumber = static_cast<uint64>(FMath::RoundHalfFromZero(Double));
		return true;
	}

	return false;
}

template <> bool TryConvertNumber<int64>(const FJsonValue& InValue, int64& OutNumber)
{
	double Double;
	if (InValue.TryGetNumber(Double) && Double >= -9223372036854775808.0 && Double < 9223372036854775808.0)
	{
		OutNumber = static_cast<int64>(FMath::RoundHalfFromZero(Double));
		return true;
	}

	return false;
}

// -----------------------------------

bool FJsonValue::TryGetNumber(float& OutNumber) const
{
	double Double;

	if (TryGetNumber(Double))
	{
		OutNumber = static_cast<float>(Double);
		return true;
	}

	return false;
}

bool FJsonValue::TryGetNumber(uint8& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(uint16& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(uint32& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(uint64& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(int8& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(int16& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(int32& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(int64& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

//static 
bool FJsonValue::CompareEqual( const FJsonValue& Lhs, const FJsonValue& Rhs )
{
	if (Lhs.Type != Rhs.Type)
	{
		return false;
	}

	switch (Lhs.Type)
	{
	case EJson::None:
	case EJson::Null:
		return true;

	case EJson::String:
		return Lhs.AsString() == Rhs.AsString();

	case EJson::Number:
		return Lhs.AsNumber() == Rhs.AsNumber();

	case EJson::Boolean:
		return Lhs.AsBool() == Rhs.AsBool();

	case EJson::Array:
		{
			const TArray< TSharedPtr<FJsonValue> >& LhsArray = Lhs.AsArray();
			const TArray< TSharedPtr<FJsonValue> >& RhsArray = Rhs.AsArray();

			if (LhsArray.Num() != RhsArray.Num())
			{
				return false;
			}

			// compare each element
			for (int32 i = 0; i < LhsArray.Num(); ++i)
			{
				if (!CompareEqual(*LhsArray[i], *RhsArray[i]))
				{
					return false;
				}
			}
		}
		return true;

	case EJson::Object:
		{
			const TSharedPtr<FJsonObject>& LhsObject = Lhs.AsObject();
			const TSharedPtr<FJsonObject>& RhsObject = Rhs.AsObject();

			if (LhsObject.IsValid() != RhsObject.IsValid())
			{
				return false;
			}

			if (LhsObject.IsValid())
			{
				if (LhsObject->Values.Num() != RhsObject->Values.Num())
				{
					return false;
				}

				// compare each element
				for (const auto& It : LhsObject->Values)
				{
					const FString& Key = It.Key;
					const TSharedPtr<FJsonValue>* RhsValue = RhsObject->Values.Find(Key);
					if (RhsValue == NULL)
					{
						// not found in both objects
						return false;
					}

					const TSharedPtr<FJsonValue>& LhsValue = It.Value;

					if (LhsValue.IsValid() != RhsValue->IsValid())
					{
						return false;
					}

					if (LhsValue.IsValid())
					{
						if (!CompareEqual(*LhsValue.Get(), *RhsValue->Get()))
						{
							return false;
						}
					}
				}
			}
		}
		return true;

	default:
		return false;
	}
}

static void DuplicateJsonArray(const TArray<TSharedPtr<FJsonValue>>& Source, TArray<TSharedPtr<FJsonValue>>& Dest)
{
	for (const TSharedPtr<FJsonValue>& Value : Source)
	{
		Dest.Add(FJsonValue::Duplicate(Value));
	} 
}

TSharedPtr<FJsonValue> FJsonValue::Duplicate(const TSharedPtr<const FJsonValue>& Src)
{
	return Duplicate(ConstCastSharedPtr<FJsonValue>(Src));
}

TSharedPtr<FJsonValue> FJsonValue::Duplicate(const TSharedPtr<FJsonValue>& Src)
{
	switch (Src->Type)
	{
		case EJson::Boolean:
		{
			bool BoolValue;
			if (Src->TryGetBool(BoolValue))
			{
				return MakeShared<FJsonValueBoolean>(BoolValue);
			}
		}
		case EJson::Number:
		{
			double NumberValue;
			if (Src->TryGetNumber(NumberValue))
			{
				return MakeShared<FJsonValueNumber>(NumberValue);
			}
		}
		case EJson::String:
		{
			FString StringValue;
			if (Src->TryGetString(StringValue))
			{
				return MakeShared<FJsonValueString>(StringValue);
			}
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject>* ObjectValue;
			if (Src->TryGetObject(ObjectValue))
			{
				TSharedPtr<FJsonObject> NewObject = MakeShared<FJsonObject>();
				FJsonObject::Duplicate(*ObjectValue, NewObject);
				return MakeShared<FJsonValueObject>(NewObject);
			}
		}
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
			if (Src->TryGetArray(ArrayValue))
			{
				TArray<TSharedPtr<FJsonValue>> NewArray;
				DuplicateJsonArray(*ArrayValue, NewArray);

				return MakeShared<FJsonValueArray>(NewArray);
			}
		}
	}

	return TSharedPtr<FJsonValue>();
}

void FJsonValue::ErrorMessage(const FString& InType) const
{
	UE_LOG(LogJson, Error, TEXT("Json Value of type '%s' used as a '%s'."), *GetType(), *InType);
}
