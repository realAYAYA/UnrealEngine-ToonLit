// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"
#include "Math/Vector.h"
#include <cinttypes>

namespace UE::Net
{

class FTestMessage
{
public:
	inline const TCHAR* C_Str() const { return *String; }
	inline const TCHAR* operator*() const { return *String; }

	inline FTestMessage& operator<<(const FString& InString) { String.Append(InString); return *this; }
	inline FTestMessage& operator<<(const TCHAR* InString) { String.Append(InString); return *this; }
	inline FTestMessage& operator<<(TCHAR Char) { String.AppendChar(Char); return *this; }

	inline FTestMessage& operator<<(const FTestMessage& Message) { String.Append(Message.String); return *this; }

private:
	FString String;
};

inline FTestMessage& operator<<(FTestMessage& Message, int Value)
{
	return Message << FString::FromInt(Value);
}

inline FTestMessage& operator<<(FTestMessage& Message, unsigned Value)
{
	return Message << TStringBuilder<32>().Appendf(TEXT("%u"), Value).ToString();
}

inline FTestMessage& operator<<(FTestMessage& Message, int8 Value)
{
	return Message << FString::FromInt(Value);
}

inline FTestMessage& operator<<(FTestMessage& Message, int16 Value)
{
	return Message << FString::FromInt(Value);
}

inline FTestMessage& operator<<(FTestMessage& Message, int64 Value)
{
	return Message << TStringBuilder<32>().Appendf(TEXT("%" INT64_FMT), Value).ToString();
}

inline FTestMessage& operator<<(FTestMessage& Message, uint8 Value)
{
	return Message << FString::FromInt(int(Value));
}

inline FTestMessage& operator<<(FTestMessage& Message, uint16 Value)
{
	return Message << FString::FromInt(int(Value));
}

inline FTestMessage& operator<<(FTestMessage& Message, uint64 Value)
{
	return Message << TStringBuilder<32>().Appendf(TEXT("%" UINT64_FMT), Value).ToString();
}

inline FTestMessage& operator<<(FTestMessage& Message, const void* Value)
{
	return Message << TStringBuilder<32>().Appendf(TEXT("0x%" UPTRINT_X_FMT), UPTRINT(Value)).ToString();
}

// uintptr_t streaming
template<typename T, typename = typename TEnableIf<TIsSame<T, uintptr_t>::Value && !TIsSame<uintptr_t, unsigned>::Value && !TIsSame<uintptr_t, uint64>::Value, uintptr_t>::Type>
inline FTestMessage& operator<<(FTestMessage& Message, T Value)
{
	return Message << TStringBuilder<32>().Appendf(TEXT("0x%" UPTRINT_X_FMT), UPTRINT(Value)).ToString();
}

// ptrdiff_t streaming
template<typename T, typename = typename TEnableIf<TIsSame<T, ptrdiff_t>::Value, ptrdiff_t>::Type, typename = typename TEnableIf<TIsSame<T, ptrdiff_t>::Value && !TIsSame<ptrdiff_t, int32>::Value && !TIsSame<ptrdiff_t, int64>::Value, ptrdiff_t>::Type>
inline FTestMessage& operator<<(FTestMessage& Message, T Value)
{
	return Message << TStringBuilder<32>().Appendf(TEXT("%" SSIZE_T_FMT), SSIZE_T(Value)).ToString();
}

inline FTestMessage& operator<<(FTestMessage& Message, float Value)
{
	return Message << TStringBuilder<384>().Appendf(TEXT("%f"), Value).ToString();
}

inline FTestMessage& operator<<(FTestMessage& Message, double Value)
{
	return Message << TStringBuilder<384>().Appendf(TEXT("%lf"), Value).ToString();
}

inline FTestMessage& operator<<(FTestMessage& Message, char Char)
{
	return Message << CharCast<TCHAR>(Char);
}

inline FTestMessage& operator<<(FTestMessage& Message, bool Value)
{
	return Message << (Value ? TEXT("true") : TEXT("false"));
}

inline FTestMessage& operator<<(FTestMessage& Message, nullptr_t)
{
	return Message << TEXT("nullptr");
}

inline FTestMessage& operator<<(FTestMessage& Message, const char* String)
{
	return Message << (String != nullptr ? StringCast<TCHAR>(String).Get() : TEXT("(null)"));
}

inline FTestMessage& operator<<(FTestMessage& Message, const FName& Name)
{
	return Message << Name.ToString();
}

inline FTestMessage& operator<<(FTestMessage& Message, const FVector& Vector)
{
	return Message << Vector.ToString();
}

#if defined(_WIN32) && !defined(_WIN64)
inline FTestMessage& operator<<(FTestMessage& Message, SIZE_T Value)
{
	return Message << TStringBuilder<32>().Appendf(TEXT("%" SIZE_T_FMT), Value).ToString();
}
#endif

}
