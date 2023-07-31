// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FModelProtoStringParser
{
public:
	static FString LineSeparator;

	static FString ToStringSeparator;

	// Note: InString is considerably big, avoiding the first InString copy with a move would be faster but would require
	// InString not to be const
	static TMap<FString, TArray<FString>> ModelProtoStringToMap(const FString& InString, const int32 InLevel);

	static FString GetModelProtoStringOrEmpty(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static TArray<FString> GetModelProtoStringArray(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	template<typename T>
	static bool GetModelProtoArray(TArray<T>& OutArray, const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey, const int32 InLevel);

	template<typename T>
	static bool GetModelProtoMap(TMap<FString, T>& OutMap, const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey, const int32 InLevel);

	static int32 GetModelProtoInt32(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static TArray<uint8> GetModelProtoCharAsUInt8Array(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static TArray<int32> GetModelProtoInt32Array(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static int64 GetModelProtoInt64(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static TArray<int64> GetModelProtoInt64Array(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static float GetModelProtoFloat(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static TArray<float> GetModelProtoFloatArray(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static double GetModelProtoDouble(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static TArray<double> GetModelProtoDoubleArray(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey);

	static TArray<uint64> GetModelProtoUInt64Array(const TMap<FString, TArray<FString>>& InProtoMap, const FString& inKey);

	/**
	 * Functions meant for ::ToString() debugging
	 */
	static void StringArrayToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const TArray<FString>& InArrayString);

	template<typename T>
	static void ArrayTToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const TArray<T>& InArray, const FString& InLineStartedNext);

	template<typename T>
	static void MapTToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const TMap<FString, T>& InMap, const FString& InLineStartedNext);

	template<typename T>
	static void PositiveInt32Or64ToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const T InNumberInt32Or64);

	template<typename T>
	static void GreaterThan0Int32Or64ToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const T InNumberInt32Or64);

	static void StringToStringIfNotEmptyAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const FString& InString);

private:
	static void RemoveQuotesFromProtoString(FString& InOutProtoString);
};



/* FModelProtoStringParser template functions
 *****************************************************************************/

template<typename T>
bool FModelProtoStringParser::GetModelProtoArray(TArray<T>& OutArray, const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey, const int32 InLevel)
{
	OutArray.Empty();
	const TArray<FString> StringArray = FModelProtoStringParser::GetModelProtoStringArray(InProtoMap, InKey);
	for (const FString& String : StringArray)
	{
		OutArray.Emplace();
		if (!OutArray.Last().LoadFromString(String, InLevel))
		{
			// If invalid --> Remove it and return false
			OutArray.Pop();
			return false;
		}
	}
	return true;
}

template<typename T>
bool FModelProtoStringParser::GetModelProtoMap(TMap<FString, T>& OutMap, const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey, const int32 InLevel)
{
	OutMap.Empty();
	const TArray<FString> StringArray = FModelProtoStringParser::GetModelProtoStringArray(InProtoMap, InKey);
	for (const FString& String : StringArray)
	{
		T TemporaryObject;
		if (TemporaryObject.LoadFromString(String, InLevel))
		{
			OutMap.Add(TemporaryObject.Name, MoveTemp(TemporaryObject));
		}
		else
		{
			return false;
		}
	}
	return true;
}

template<typename T>
void FModelProtoStringParser::ArrayTToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const TArray<T>& InArray, const FString& InLineStartedNext)
{
	int32 Index = -1;
	for (const T& Value : InArray)
	{
		InOutString += FString::Format(InText, { InLineStarted, ++Index, Value.ToString(InLineStartedNext) });
	}
}

template<typename T>
void FModelProtoStringParser::MapTToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const TMap<FString, T>& InMap, const FString& InLineStartedNext)
{
	int32 Index = -1;
	for (const TTuple<FString, T>& Tuple : InMap)
	{
		InOutString += FString::Format(InText, { InLineStarted, ++Index, Tuple.Value.ToString(InLineStartedNext) });
	}
}

template<typename T>
void FModelProtoStringParser::PositiveInt32Or64ToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const T InNumberInt32Or64)
{
	if (InNumberInt32Or64 > -1)
	{
		InOutString += FString::Format(InText, { InLineStarted, FString::FromInt(InNumberInt32Or64) });
	}
}

template<typename T>
void FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const T InNumberInt32Or64)
{
	if (InNumberInt32Or64 > 0)
	{
		InOutString += FString::Format(InText, { InLineStarted, FString::FromInt(InNumberInt32Or64) });
	}
}
