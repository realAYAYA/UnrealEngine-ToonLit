// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEMetaData.h"

#include "Containers/Array.h"
#include "Serialization/CustomVersion.h"
#include "NNE.h"

namespace UE::NNERuntimeIREE::ModuleMetaData::Private
{
	enum Version : uint32
	{
		V0 = 0, // Initial
		// New versions can be added above this line
		VersionPlusOne,
		Latest = VersionPlusOne - 1
	};
	const FGuid GUID(0x2f9ffd31, 0x12b817cd, 0x627855bf, 0x5e405720);
	FCustomVersionRegistration Version(GUID, Version::Latest, TEXT("NNERuntimeIREEModuleMetaDataVersion"));// Always save with the latest version

	ENNETensorDataType ConvertTypeString(const FString& TypeString)
	{
		if (TypeString.StartsWith("char"))
		{
			return ENNETensorDataType::Char;
		}
		if (TypeString.StartsWith("bool") || TypeString.StartsWith("i1"))
		{
			return ENNETensorDataType::Boolean;
		}
		else if (TypeString.StartsWith("half"))
		{
			return ENNETensorDataType::Half;
		}
		else if (TypeString.StartsWith("f"))
		{
			if (TypeString.StartsWith("f16"))
			{
				return ENNETensorDataType::Half;
			}
			else if (TypeString.StartsWith("float") || TypeString.StartsWith("f32"))
			{
				return ENNETensorDataType::Float;
			}
			else if (TypeString.StartsWith("f64"))
			{
				return ENNETensorDataType::Double;
			}
		}
		else if (TypeString.StartsWith("double"))
		{
			return ENNETensorDataType::Double;
		}
		else if (TypeString.StartsWith("i") || TypeString.StartsWith("si"))
		{
			if (TypeString.EndsWith("i8"))
			{
				return ENNETensorDataType::Int8;
			}
			else if (TypeString.EndsWith("i16"))
			{
				return ENNETensorDataType::Int16;
			}
			else if (TypeString.EndsWith("i32") || TypeString.EndsWith("int"))
			{
				return ENNETensorDataType::Int32;
			}
			else if (TypeString.EndsWith("i64"))
			{
				return ENNETensorDataType::Int64;
			}
		}
		else if (TypeString.StartsWith("ui"))
		{
			if (TypeString.EndsWith("i8"))
			{
				return ENNETensorDataType::UInt8;
			}
			else if (TypeString.EndsWith("i16"))
			{
				return ENNETensorDataType::UInt16;
			}
			else if (TypeString.EndsWith("i32"))
			{
				return ENNETensorDataType::UInt32;
			}
			else if (TypeString.EndsWith("i64"))
			{
				return ENNETensorDataType::UInt64;
			}
		}
		return ENNETensorDataType::None;
	}

	bool ParseTensorDescFromString(const FString& ArgumentString, FString& Name, TArray<int32>& Shape, ENNETensorDataType& Type)
	{
		FString Argument = ArgumentString;
		int32 AttributeStartIndex = Argument.Find("{");
		if (AttributeStartIndex > 0)
		{
			Argument = Argument.Mid(0, AttributeStartIndex).TrimStartAndEnd();
		}

		Name = "";
		int32 NameEnd = Argument.Find(":");
		if (NameEnd > 0)
		{
			Name = Argument.Mid(0, NameEnd).TrimStartAndEnd();
			Argument = Argument.Mid(NameEnd + 1).TrimStartAndEnd();
		}
		else
		{
			Argument = Argument.TrimStartAndEnd();
		}

		Shape.Empty();
		if (Argument.StartsWith("tensor"))
		{
			int32 ShapeOpenBracket = Argument.Find("<");
			int32 ShapeCloseBracket = Argument.Find(">");
			if (ShapeOpenBracket < 1 || ShapeCloseBracket <= ShapeOpenBracket)
			{
				return false;
			}
			FString ShapeTypeString = Argument.Mid(ShapeOpenBracket + 1, ShapeCloseBracket - (ShapeOpenBracket + 1)).TrimStartAndEnd();

			TArray<FString> Dims;
			FString Dim;
			while (ShapeTypeString.Split("x", &Dim, &ShapeTypeString))
			{
				Dims.Add(Dim);
			}
			Type = ConvertTypeString(ShapeTypeString);

			for (int32 i = 0; i < Dims.Num(); i++)
			{
				if (Dims[i].Contains("?"))
				{
					Shape.Add(-1);
				}
				else
				{
					int32 DimVal = FCString::Atoi(*Dims[i]);
					Shape.Add(DimVal > 0 ? DimVal : -1);
				}
			}
		}
		else
		{
			Shape.Add(1);
			Type = ConvertTypeString(Argument);
		}

		return Type != ENNETensorDataType::None;
	}

	bool ParseFunctionMetaDataFromString(const FString& FunctionString, UE::NNERuntimeIREE::FFunctionMetaData& FunctionMetaData)
	{
		FString ResultPattern = "->";
		int32 ArgumentsOpenBracket = FunctionString.Find("(");
		int32 ArgumentsCloseBracket = FunctionString.Find(")");
		int32 ResultStart = FunctionString.Find(ResultPattern);
		if (ArgumentsOpenBracket < 1 || ArgumentsCloseBracket <= ArgumentsOpenBracket)
		{
			return false;
		}

		FunctionMetaData.Name = FunctionString.Mid(0, ArgumentsOpenBracket).TrimStartAndEnd();
		if (FunctionMetaData.Name.IsEmpty())
		{
			return false;
		}
		FunctionMetaData.InputDescs.Empty();
		FunctionMetaData.OutputDescs.Empty();

		FString ArgumentsString = FunctionString.Mid(ArgumentsOpenBracket + 1, ArgumentsCloseBracket - (ArgumentsOpenBracket + 1)).TrimStartAndEnd();
		while (ArgumentsString.Len() > 0)
		{
			int32 SeparatorIndex = ArgumentsString.Find(",");
			FString ArgumentString;
			if (SeparatorIndex > 0)
			{
				ArgumentString = ArgumentsString.Mid(0, SeparatorIndex).TrimStartAndEnd();
				ArgumentsString = ArgumentsString.Mid(SeparatorIndex + 1).TrimStartAndEnd();
			}
			else
			{
				ArgumentString = ArgumentsString;
				ArgumentsString = "";
			}

			FString Name;
			TArray<int32> Shape;
			ENNETensorDataType Type;
			if (ParseTensorDescFromString(ArgumentString, Name, Shape, Type))
			{
				FunctionMetaData.InputDescs.Add(UE::NNE::FTensorDesc::Make(Name, UE::NNE::FSymbolicTensorShape::Make(Shape), Type));
			}
			else
			{
				return false;
			}
		}

		if (ResultStart > ArgumentsCloseBracket)
		{
			FString ResultsString = FunctionString.Mid(ResultStart + ResultPattern.Len()).TrimStartAndEnd();
			ResultsString.RemoveFromStart("(");
			ResultsString.RemoveFromEnd(")");
			ResultsString = ResultsString.TrimStartAndEnd();
			while (ResultsString.Len() > 0)
			{
				int32 SeparatorIndex = ResultsString.Find(",");
				FString ResultString;
				if (SeparatorIndex > 0)
				{
					ResultString = ResultsString.Mid(0, SeparatorIndex).TrimStartAndEnd();
					ResultsString = ResultsString.Mid(SeparatorIndex + 1).TrimStartAndEnd();
				}
				else
				{
					ResultString = ResultsString;
					ResultsString = "";
				}

				FString Name;
				TArray<int32> Shape;
				ENNETensorDataType Type;
				if (ParseTensorDescFromString(ResultString, Name, Shape, Type))
				{
					FunctionMetaData.OutputDescs.Add(UE::NNE::FTensorDesc::Make(Name, UE::NNE::FSymbolicTensorShape::Make(Shape), Type));
				}
				else
				{
					return false;
				}
			}
		}

		return true;
	}
} // UE::NNERuntimeIREE::ModuleMetaData::Private

void UNNERuntimeIREEModuleMetaData::Serialize(FArchive& Ar)
{
	// Store the asset version (no effect in load)
	Ar.UsingCustomVersion(UE::NNERuntimeIREE::ModuleMetaData::Private::GUID);

	if (Ar.IsSaving() || Ar.IsCountingMemory()) 
	{
		int32 NumItems = FunctionMetaData.Num();
		Ar << NumItems;
		for (int32 i = 0; i < NumItems; i++)
		{
			Ar << FunctionMetaData[i].Name;

			int32 NumInputs = FunctionMetaData[i].InputDescs.Num();
			Ar << NumInputs;
			for (int32 j = 0; j < NumInputs; j++)
			{
				FString Name = FunctionMetaData[i].InputDescs[j].GetName();
				Ar << Name;

				ENNETensorDataType Type = FunctionMetaData[i].InputDescs[j].GetDataType();
				Ar << Type;

				TArray<int32> Shape = (TArray<int32>)FunctionMetaData[i].InputDescs[j].GetShape().GetData();
				Ar << Shape;
			}

			int32 NumOutputs = FunctionMetaData[i].OutputDescs.Num();
			Ar << NumOutputs;
			for (int32 j = 0; j < NumOutputs; j++)
			{
				FString Name = FunctionMetaData[i].OutputDescs[j].GetName();
				Ar << Name;

				ENNETensorDataType Type = FunctionMetaData[i].OutputDescs[j].GetDataType();
				Ar << Type;

				TArray<int32> Shape = (TArray<int32>)FunctionMetaData[i].OutputDescs[j].GetShape().GetData();
				Ar << Shape;
			}
		}
	}
	else
	{
		int32 NumItems = 0;
		int32 NumInputs = 0;
		int32 NumOutputs = 0;
		UE::NNERuntimeIREE::FFunctionMetaData MetaData;
		FString Name;
		ENNETensorDataType Type;
		TArray<int32> Shape;

		switch (Ar.CustomVer(UE::NNERuntimeIREE::ModuleMetaData::Private::GUID))
		{
		case UE::NNERuntimeIREE::ModuleMetaData::Private::Version::V0:
			Ar << NumItems;
			FunctionMetaData.SetNum(NumItems, EAllowShrinking::Yes);
			for (int32 i = 0; i < NumItems; i++)
			{
				Ar << MetaData.Name;

				Ar << NumInputs;
				MetaData.InputDescs.Empty();
				for (int32 j = 0; j < NumInputs; j++)
				{
					Ar << Name;
					Ar << Type;
					Ar << Shape;
					MetaData.InputDescs.Add(UE::NNE::FTensorDesc::Make(Name, UE::NNE::FSymbolicTensorShape::Make(Shape), Type));
				}

				Ar << NumOutputs;
				MetaData.OutputDescs.Empty();
				for (int32 j = 0; j < NumOutputs; j++)
				{
					Ar << Name;
					Ar << Type;
					Ar << Shape;
					MetaData.OutputDescs.Add(UE::NNE::FTensorDesc::Make(Name, UE::NNE::FSymbolicTensorShape::Make(Shape), Type));
				}

				FunctionMetaData[i] = MetaData;
			}
			break;
		default:
			UE_LOG(LogNNE, Error, TEXT("UNNERuntimeIREEModuleMetaData: Unknown asset version %d: Deserialisation failed, please reimport the original model."), Ar.CustomVer(UE::NNERuntimeIREE::ModuleMetaData::Private::GUID));
			break;
		}
	}
}

bool UNNERuntimeIREEModuleMetaData::ParseFromString(const FString& ModuleString)
{
	using namespace UE::NNERuntimeIREE::ModuleMetaData::Private;

	TArray<UE::NNERuntimeIREE::FFunctionMetaData> Result;
	FString SearchString = ModuleString;
	FString Pattern = "func.func";
	int32 MatchStart = -1;
	while ((MatchStart = SearchString.Find(Pattern)) > 0)
	{
		MatchStart += Pattern.Len();
		SearchString = SearchString.Mid(MatchStart).TrimStartAndEnd();
		if (SearchString.StartsWith("@"))
		{
			SearchString = SearchString.Mid(1);

			int32 ArgumentsEnd = SearchString.Find(")");
			FString TempString = SearchString.Mid(ArgumentsEnd + 1).TrimStartAndEnd();
			if (TempString.StartsWith("->"))
			{
				TempString = TempString.Mid(3).TrimStartAndEnd();
				if (TempString.StartsWith("("))
				{
					MatchStart = SearchString.Find(")", ESearchCase::IgnoreCase, ESearchDir::FromStart, ArgumentsEnd + 1);
				}
				else
				{
					int32 AttribtueStart = SearchString.Find("attributes", ESearchCase::IgnoreCase, ESearchDir::FromStart, ArgumentsEnd + 1);
					int32 BracketStart = SearchString.Find("{", ESearchCase::IgnoreCase, ESearchDir::FromStart, ArgumentsEnd + 1);
					MatchStart = AttribtueStart <= ArgumentsEnd ? BracketStart : FMath::Min(AttribtueStart, BracketStart);
				}
			}
			else
			{
				MatchStart = ArgumentsEnd;
			}

			UE::NNERuntimeIREE::FFunctionMetaData MetaData;
			if (!ParseFunctionMetaDataFromString(SearchString.Mid(0, MatchStart), MetaData))
			{
				return false;
			}
			Result.Add(MetaData);
		}
	}
	if (!Result.IsEmpty())
	{
		FunctionMetaData = Result;
		return true;
	}
	return false;
}