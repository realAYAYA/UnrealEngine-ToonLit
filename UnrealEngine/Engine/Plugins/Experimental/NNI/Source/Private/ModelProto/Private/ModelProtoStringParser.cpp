// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelProtoStringParser.h"
#include "ModelProtoUtils.h"



/* FModelProtoStringParser static variables
 *****************************************************************************/

FString FModelProtoStringParser::LineSeparator = TEXT("  ");
FString FModelProtoStringParser::ToStringSeparator = TEXT("    ");



/* FModelProtoStringParser public functions
 *****************************************************************************/

TMap<FString, TArray<FString>> FModelProtoStringParser::ModelProtoStringToMap(const FString& InString, const int32 InLevel)
{
	TMap<FString, TArray<FString>> ProtoMap;

	// Parse FString
	FString RightSubString = (InString.Find(TEXT("\r\n")) == INDEX_NONE
		// Windows uses "/r/n" for end-of-line, most others just "/n"
		? InString.Replace(TEXT("\n"), TEXT("\r\n"))
		: InString);
	int32 EnterIndex;
	while (RightSubString.FindChar(TCHAR('\n'), EnterIndex) || RightSubString.Len() > 0)
	{
		if (EnterIndex == INDEX_NONE)
		{
			EnterIndex = RightSubString.Len() + 1;
		}
		const FString SubString = RightSubString.Left(EnterIndex);
		int32 SeparatorCharIndex;
		FString Key;
		FString Value;
		// Colon
		if (SubString.FindChar(TCHAR(':'), SeparatorCharIndex))
		{
			Key = SubString.Mid(LineSeparator.Len() * InLevel, SeparatorCharIndex - LineSeparator.Len() * InLevel);
			Value = SubString.Mid(SeparatorCharIndex + 2, EnterIndex - SeparatorCharIndex - 3);
			// Update RightSubString
			RightSubString.RightChopInline(EnterIndex + 1);
		}
		// Braces
		else if (SubString.FindChar(TCHAR('{'), SeparatorCharIndex))
		{
			// Key
			Key = SubString.Mid(LineSeparator.Len() * InLevel, SeparatorCharIndex - LineSeparator.Len() * InLevel - 1);
			// Get right ClosingBrace
			FString ClosingBrace = TEXT("\r\n");
			for (int32 Index = 0; Index < InLevel; ++Index)
			{
				ClosingBrace += LineSeparator;
			}
			ClosingBrace += TEXT("}");
			// Value
			const int32 ClosingBraceIndex = RightSubString.Find(ClosingBrace);
			const int32 StartPoint = SeparatorCharIndex + 3;
			if (ClosingBraceIndex != INDEX_NONE)
			{
				Value = RightSubString.Mid(StartPoint, ClosingBraceIndex - StartPoint);
			}
			else
			{
				UE_LOG(LogModelProto, Warning, TEXT("ModelProtoStringToMap(): Expected closing brace missing: \"}\"."));
				return TMap<FString, TArray<FString>>();
			}
			// Update RightSubString
			RightSubString.RightChopInline(ClosingBraceIndex + ClosingBrace.Len() + 2);
		}
		// Unknown
		else
		{
			UE_LOG(LogModelProto, Warning, TEXT("ModelProtoStringToMap(): InLevel: %d\n - InString: %s\n - RightSubString: %s."),
				InLevel, *InString, *RightSubString);
			break;
		}
		// Already exist --> Emplace to TArray
		if (TArray<FString>* Values = ProtoMap.Find(Key))
		{
			Values->Emplace(Value);
		}
		// First time --> Add
		else
		{
			ProtoMap.Add(Key, TArray<FString>({Value}));
		}
	}

	return ProtoMap;
}

FString FModelProtoStringParser::GetModelProtoStringOrEmpty(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	if (const TArray<FString>* const String = InProtoMap.Find(InKey))
	{
		if (String->Num() != 1)
		{
			UE_LOG(LogModelProto, Warning,
				TEXT("GetModelProtoStringOrEmpty(): InKey: %s, InProtoMap.Num() = %d, String->Num() = %d != 1."),
				*InKey, InProtoMap.Num(), String->Num());
		}
		FString OutputString = (*String)[0]; // Copy so RemoveQuotesFromProtoString does not edit it
		// If string is something like: "gemm", it changes it into gemm
		RemoveQuotesFromProtoString(OutputString);
		return OutputString;
	}
	return TEXT("");
}

TArray<FString> FModelProtoStringParser::GetModelProtoStringArray(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	if (const TArray<FString>* const StringArray = InProtoMap.Find(InKey))
	{
		if (StringArray->Num() < 1)
		{
			UE_LOG(LogModelProto, Warning,
				TEXT("GetModelProtoStringArray(): InKey: %s, InProtoMap.Num() = %d, StringArray->Num() = %d (should be >1)."),
				*InKey, InProtoMap.Num(), StringArray->Num());
		}
		// If string is something like: "gemm", it changes it into gemm
		TArray<FString> CleanedStringArray = *StringArray;
		for (FString& String : CleanedStringArray)
		{
			RemoveQuotesFromProtoString(String);
		}
		return CleanedStringArray;
	}
	return TArray<FString>({});
}

int32 FModelProtoStringParser::GetModelProtoInt32(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const FString ModelProtoNumberString = GetModelProtoStringOrEmpty(InProtoMap, InKey);
	if (ModelProtoNumberString.Len() > 0)
	{
		return FCString::Atoi(*ModelProtoNumberString);
	}
	return int32(-1);
}

TArray<uint8> FModelProtoStringParser::GetModelProtoCharAsUInt8Array(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	TArray<uint8> ProtoCharAsUint8Array;
	const FString RawDataString = FModelProtoStringParser::GetModelProtoStringOrEmpty(InProtoMap, InKey);
	if (!RawDataString.IsEmpty())
	{
		const auto RawDataAnsiString = StringCast<ANSICHAR>(*RawDataString);
		// Allocate memory
		ProtoCharAsUint8Array.SetNumUninitialized(RawDataString.Len());
		// Copy memory
		FMemory::Memcpy(ProtoCharAsUint8Array.GetData(), RawDataAnsiString.Get(), ProtoCharAsUint8Array.Num());
	}
	// Return array
	return ProtoCharAsUint8Array;
}

TArray<int32> FModelProtoStringParser::GetModelProtoInt32Array(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const TArray<FString> ModelProtoNumberStringArray = GetModelProtoStringArray(InProtoMap, InKey);
	TArray<int32> ProtoNumberArray;
	for (const FString& ModelProtoNumberString : ModelProtoNumberStringArray)
	{
		if (ModelProtoNumberString.Len() > 0)
		{
			ProtoNumberArray.Emplace(FCString::Atoi(*ModelProtoNumberString));
		}
	}
	return ProtoNumberArray;
}

int64 FModelProtoStringParser::GetModelProtoInt64(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const FString ModelProtoNumberString = GetModelProtoStringOrEmpty(InProtoMap, InKey);
	if (ModelProtoNumberString.Len() > 0)
	{
		return FCString::Atoi64(*ModelProtoNumberString);
	}
	return int64(-1);
}

TArray<int64> FModelProtoStringParser::GetModelProtoInt64Array(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const TArray<FString> ModelProtoNumberStringArray = GetModelProtoStringArray(InProtoMap, InKey);
	TArray<int64> ProtoNumberArray;
	for (const FString& ModelProtoNumberString : ModelProtoNumberStringArray)
	{
		if (ModelProtoNumberString.Len() > 0)
		{
			ProtoNumberArray.Emplace(FCString::Atoi64(*ModelProtoNumberString));
		}
	}
	return ProtoNumberArray;
}

float FModelProtoStringParser::GetModelProtoFloat(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const FString ModelProtoNumberString = GetModelProtoStringOrEmpty(InProtoMap, InKey);
	if (ModelProtoNumberString.Len() > 0)
	{
		return FCString::Atof(*ModelProtoNumberString);
	}
	return int64(-1);
}

TArray<float> FModelProtoStringParser::GetModelProtoFloatArray(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const TArray<FString> ModelProtoNumberStringArray = GetModelProtoStringArray(InProtoMap, InKey);
	TArray<float> ProtoNumberArray;
	for (const FString& ModelProtoNumberString : ModelProtoNumberStringArray)
	{
		if (ModelProtoNumberString.Len() > 0)
		{
			ProtoNumberArray.Emplace(FCString::Atof(*ModelProtoNumberString));
		}
	}
	return ProtoNumberArray;
}

double FModelProtoStringParser::GetModelProtoDouble(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const FString ModelProtoNumberString = GetModelProtoStringOrEmpty(InProtoMap, InKey);
	if (ModelProtoNumberString.Len() > 0)
	{
		return FCString::Atod(*ModelProtoNumberString);
	}
	return int64(-1);
}

TArray<double> FModelProtoStringParser::GetModelProtoDoubleArray(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const TArray<FString> ModelProtoNumberStringArray = GetModelProtoStringArray(InProtoMap, InKey);
	TArray<double> ProtoNumberArray;
	for (const FString& ModelProtoNumberString : ModelProtoNumberStringArray)
	{
		if (ModelProtoNumberString.Len() > 0)
		{
			ProtoNumberArray.Emplace(FCString::Atod(*ModelProtoNumberString));
		}
	}
	return ProtoNumberArray;
}

TArray<uint64> FModelProtoStringParser::GetModelProtoUInt64Array(const TMap<FString, TArray<FString>>& InProtoMap, const FString& InKey)
{
	const TArray<FString> ModelProtoNumberStringArray = GetModelProtoStringArray(InProtoMap, InKey);
	TArray<uint64> ProtoNumberArray;
	for (const FString& ModelProtoNumberString : ModelProtoNumberStringArray)
	{
		if (ModelProtoNumberString.Len() > 0)
		{
			ProtoNumberArray.Emplace(FCString::Strtoui64(*ModelProtoNumberString, nullptr, 10));
		}
	}
	return ProtoNumberArray;
}

void FModelProtoStringParser::StringArrayToStringAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const TArray<FString>& InArrayString)
{
	if (InArrayString.Num() > 0)
	{
		FString FinalString = TEXT(" ");
		for (const FString& String : InArrayString)
		{
			FinalString += String + TEXT(" ");
		}
		StringToStringIfNotEmptyAuxiliary(InOutString, InText, InLineStarted, FinalString);
	}
}

void FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(FString& InOutString, const TCHAR* const InText, const FString& InLineStarted, const FString& InString)
{
	if (!InString.IsEmpty())
	{
		InOutString += FString::Format(InText, { InLineStarted, InString });
	}
}

void FModelProtoStringParser::RemoveQuotesFromProtoString(FString& InOutProtoString)
{
	if (InOutProtoString.Len() > 1 && InOutProtoString.Left(1) == TEXT("\"") && InOutProtoString.Right(1) == TEXT("\""))
	{
		InOutProtoString.MidInline(1, InOutProtoString.Len() - 2);
	}
}
