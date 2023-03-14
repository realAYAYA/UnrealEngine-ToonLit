// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineMeta.h"
#include "Containers/UnrealString.h"

namespace UE::Online {

template <typename T> FString ToLogString(const TArray<T>& Array);
template <typename T> FString ToLogString(const TSet<T>& Set);
template <typename K, typename V> FString ToLogString(const TMap<K, V>& Map);
template <typename T, typename U> FString ToLogString(const TPair<T, U>& Pair);
template <typename T, ESPMode Mode> FString ToLogString(const TSharedPtr<T, Mode>& Ptr);
template <typename T, ESPMode Mode> FString ToLogString(const TSharedRef<T, Mode>& Ref);
template <typename T> FString ToLogString(const TOptional<T> Optional);
template <typename... Ts> FString ToLogString(const TVariant<Ts...>& Variant);
template <typename T, ESPMode Mode> FString ToLogString(const TSharedRef<T, Mode>& Ref);
inline FString ToLogString(const FString& String);
inline FString ToLogString(const FName& Name);
inline FString ToLogString(const FText& Text);
inline FString ToLogString(const FDateTime& Time);
inline FString ToLogString(uint8 Value);
inline FString ToLogString(int8 Value);
inline FString ToLogString(uint16 Value);
inline FString ToLogString(int16 Value);
inline FString ToLogString(uint32 Value);
inline FString ToLogString(int32 Value);
inline FString ToLogString(uint64 Value);
inline FString ToLogString(int64 Value);
inline FString ToLogString(bool Value);
inline FString ToLogString(float Value);
inline FString ToLogString(double Value);
template <typename T> std::enable_if_t<!TModels<Meta::COnlineMetadataAvailable, T>::Value, FString> ToLogString(const T& Value);
template <typename T> std::enable_if_t<TModels<Meta::COnlineMetadataAvailable, T>::Value, FString> ToLogString(const T& Value);

template <typename T>
FString ToLogString(const TArray<T>& Array)
{
	return FString::Printf(TEXT("[%s]"), *FString::JoinBy(Array, TEXT(", "), [](const T& Value) { return ToLogString(Value); }));
}

template <typename T>
FString ToLogString(const TSet<T>& Set)
{
	return FString::Printf(TEXT("{%s}"), *FString::JoinBy(Set, TEXT(", "), [](const T& Value) { return ToLogString(Value); }));
}

template <typename K, typename V>
FString ToLogString(const TMap<K, V>& Map)
{
	return FString::Printf(TEXT("{%s}"), *FString::JoinBy(Map, TEXT(", "), [](const TPair<K, V>& Pair) { return ToLogString(Pair); }));
}

template <typename T, typename U>
FString ToLogString(const TPair<T, U>& Pair)
{
	return FString::Printf(TEXT("%s:%s"), *ToLogString(Pair.template Get<0>()), *ToLogString(Pair.template Get<1>()));
}

template <typename T, ESPMode Mode>
FString ToLogString(const TSharedPtr<T, Mode>& Ptr)
{
	if (Ptr.IsValid())
	{
		return ToLogString(*Ptr);
	}
	else
	{
		return TEXT("null");
	}
}

template <typename T, ESPMode Mode>
FString ToLogString(const TSharedRef<T, Mode>& Ref)
{
	return ToLogString(*Ref);
}

template <typename T>
FString ToLogString(const TOptional<T> Optional)
{
	if (Optional.IsSet())
	{
		return ToLogString(Optional.GetValue());
	}
	else
	{
		return TEXT("unset");
	}
}

template <typename... Ts>
FString ToLogString(const TVariant<Ts...>& Variant)
{
	return Visit([](const auto& Value)
	{
		return ToLogString(Value);
	}, Variant);
}

inline FString ToLogString(const FString& String)
{
	return String;
}

inline FString ToLogString(const FName& Name)
{
	return Name.ToString();
}

inline FString ToLogString(const FText& Text)
{
	return Text.ToString();
}

inline FString ToLogString(const FDateTime& Time)
{
	return Time.ToString();
}


inline FString ToLogString(uint8 Value)
{
	return FString::Printf(TEXT("%hhu"), Value);
}

inline FString ToLogString(int8 Value)
{
	return FString::Printf(TEXT("%hhi"), Value);
}

inline FString ToLogString(uint16 Value)
{
	return FString::Printf(TEXT("%hu"), Value);
}

inline FString ToLogString(int16 Value)
{
	return FString::Printf(TEXT("%hi"), Value);
}

inline FString ToLogString(uint32 Value)
{
	return FString::Printf(TEXT("%u"), Value);
}

inline FString ToLogString(int32 Value)
{
	return FString::Printf(TEXT("%i"), Value);
}

inline FString ToLogString(uint64 Value)
{
	return FString::Printf(TEXT("%llu"), Value);
}

inline FString ToLogString(int64 Value)
{
	return FString::Printf(TEXT("%lli"), Value);
}

inline FString ToLogString(float Value)
{
	return FString::Printf(TEXT("%.2f"), Value);
}

inline FString ToLogString(double Value)
{
	return FString::Printf(TEXT("%.2f"), Value);
}

inline FString ToLogString(bool Value)
{
	return ::LexToString(Value);
}

inline FString ToLogString(FPlatformUserId PlatformUserId)
{
	return ToLogString(PlatformUserId.GetInternalId());
}

template<typename T>
std::enable_if_t<!TModels<Meta::COnlineMetadataAvailable, T>::Value, FString> ToLogString(const T& Value)
{
	return LexToString(Value);
}

template<typename T>
std::enable_if_t<TModels<Meta::COnlineMetadataAvailable, T>::Value, FString> ToLogString(const T& Value)
{
	FString LogString;
	LogString += TEXT("{ ");
	bool bFirst = true;
	Meta::VisitFields(Value, [&LogString, &bFirst](const TCHAR* Name, auto& Field)
		{
			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				LogString += TEXT(", ");
			}
			LogString += Name;
			LogString += TEXT(": ");
			LogString += ToLogString(Field);
		});
	LogString += TEXT(" }");

	return LogString;
}

template <typename DataType, typename OpType>
const DataType& GetOpDataChecked(const TOnlineAsyncOp<OpType>& Op, const FString& Key)
{
	const DataType* Data = Op.Data.template Get<DataType>(Key);
	check(Data);
	return *Data;
}

/* UE::Online */ }