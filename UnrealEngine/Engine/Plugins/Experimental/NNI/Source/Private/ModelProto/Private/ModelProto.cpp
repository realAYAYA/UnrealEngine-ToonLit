// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelProto.h"
#include "ModelProtoUtils.h"
#include "ModelProtoStringParser.h"



/* FTensorProtoSegment structors
 *****************************************************************************/

FTensorProtoSegment::FTensorProtoSegment()
	: Begin(-1)
	, End(-1)
{
}

/* FTensorProtoSegment public functions
 *****************************************************************************/

bool FTensorProtoSegment::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	Begin = FModelProtoStringParser::GetModelProtoInt64(ProtoMap, TEXT("begin"));
	End = FModelProtoStringParser::GetModelProtoInt64(ProtoMap, TEXT("end"));

	const bool bIsLoaded = Begin <= End;
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FTensorProtoSegment::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FTensorProtoSegment::ToString(const FString&) const
{
	return FString::Format(TEXT("FTensorProtoSegment - [Begin, End] = [{0},{1}]:\n"), { FString::FromInt(Begin), FString::FromInt(End) });
}



/* FStringStringEntryProto public functions
 *****************************************************************************/

bool FStringStringEntryProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	Key = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("key"));
	Value = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("value"));

	const bool bIsLoaded = !Key.IsEmpty() && !Value.IsEmpty();
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FStringStringEntryProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FStringStringEntryProto::ToString(const FString&) const
{
	return FString::Format(TEXT("FStringStringEntryProto: [Key, Value] = [{0},{1}]\n"), { Key, Value });
}



/* FTensorShapeProtoDimension structors
 *****************************************************************************/

FTensorShapeProtoDimension::FTensorShapeProtoDimension()
	: DimValue(-1)
{
}

/* FTensorShapeProtoDimension public functions
 *****************************************************************************/

bool FTensorShapeProtoDimension::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	DimValue = FModelProtoStringParser::GetModelProtoInt64(ProtoMap, TEXT("dim_value"));
	DimParam = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("dim_param"));

	const bool bIsLoaded = (DimValue > -1);
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FTensorShapeProtoDimension::LoadFromString(): failed."));
	}
	return bIsLoaded;
}

FString FTensorShapeProtoDimension::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	FModelProtoStringParser::PositiveInt32Or64ToStringAuxiliary(String, TEXT("{0}FTensorShapeProtoDimension.DimValue: {1}\n"), InLineStarted, DimValue);
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FTensorShapeProtoDimension.DimParam: {1}\n"), InLineStarted, DimParam);
	return String;
}



/* FTensorShapeProto public functions
 *****************************************************************************/

bool FTensorShapeProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	const bool bIsLoaded = FModelProtoStringParser::GetModelProtoArray(Dim, ProtoMap, TEXT("dim"), InLevel + 1);

	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FTensorShapeProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FTensorShapeProto::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FTensorShapeProto.Dim[{1}]: {2}"), InLineStarted, Dim, FModelProtoStringParser::ToStringSeparator + InLineStarted);
	return String;
}



/* FTensorProto structors
 *****************************************************************************/

FTensorProto::FTensorProto()
	: DataType(ETensorProtoDataType::UNDEFINED)
	, DataLocation(ETensorProtoDataLocation::DEFAULT)
{
}

/* FTensorProto public functions
 *****************************************************************************/

bool FTensorProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bool bIsLoaded = true;
	Dimensions = FModelProtoStringParser::GetModelProtoInt64Array(ProtoMap, TEXT("dims"));
	DataType = StringToDataType(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("data_type")));
	DataLocation = StringToDataLocation(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("data_location")));
	/*bIsLoaded &= */Segment.LoadFromString(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("segment")), InLevel + 1);
	FloatData = FModelProtoStringParser::GetModelProtoFloatArray(ProtoMap, TEXT("float_data"));
	Int32Data = FModelProtoStringParser::GetModelProtoInt32Array(ProtoMap, TEXT("int32_data"));
	StringData = FModelProtoStringParser::GetModelProtoStringArray(ProtoMap, TEXT("string_data"));
	Int64Data = FModelProtoStringParser::GetModelProtoInt64Array(ProtoMap, TEXT("int64_data"));
	Name = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("name"));
	DocString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("doc_string"));
	RawData = FModelProtoStringParser::GetModelProtoCharAsUInt8Array(ProtoMap, TEXT("raw_data"));
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(ExternalData, ProtoMap, TEXT("external_data"), InLevel + 1);
	DoubleData = FModelProtoStringParser::GetModelProtoDoubleArray(ProtoMap, TEXT("double_data"));
	UInt64Data = FModelProtoStringParser::GetModelProtoUInt64Array(ProtoMap, TEXT("uint64_data"));

	bIsLoaded &= (bIsLoaded && DataType != ETensorProtoDataType::UNDEFINED
		&& ((ExternalData.Num() == 1 && ExternalData[0].Key == TEXT("location") && ExternalData[0].Value.Len() > 0) // Either external data is available...
			|| !FloatData.IsEmpty() || !Int32Data.IsEmpty() || !Int64Data.IsEmpty() || !RawData.IsEmpty() || !DoubleData.IsEmpty())); // ...or RawData is filled
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FTensorProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FTensorProto::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FTensorProto.Dimensions.Num(): {1}\n"), InLineStarted, Dimensions.Num());
	String += FString::Format(TEXT("{0}FTensorProto.DataType: {1}\n"), { InLineStarted, FString::FromInt((int32)DataType) });
	String += FString::Format(TEXT("{0}FTensorProto.DataLocation: {1}\n"), { InLineStarted, FString::FromInt((int32)DataLocation) });
	if (Segment.Begin > -1 || Segment.End > -1)
	{
		String += FString::Format(TEXT("{0}FTensorProto.Segment: {1}"), { InLineStarted, Segment.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
	}
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FTensorProto.FloatData.Num(): {1}\n"), InLineStarted, FloatData.Num());
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FTensorProto.Int32Data.Num(): {1}\n"), InLineStarted, Int32Data.Num());
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FTensorProto.StringData.Num(): {1}\n"), InLineStarted, StringData.Num());
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FTensorProto.Int64Data.Num(): {1}\n"), InLineStarted, Int64Data.Num());
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FTensorProto.Name: {1}\n"), InLineStarted, Name);
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FTensorProto.RawData.Num(): {1}\n"), InLineStarted, RawData.Num());
	FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FTensorProto.ExternalData[{1}]: {2}"), InLineStarted, ExternalData, FModelProtoStringParser::ToStringSeparator + InLineStarted);
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FTensorProto.DoubleData.Num(): {1}\n"), InLineStarted, DoubleData.Num());
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FTensorProto.UInt64Data.Num(): {1}\n"), InLineStarted, UInt64Data.Num());
	return String;
}

/* FTensorProto private functions
 *****************************************************************************/

ETensorProtoDataType FTensorProto::StringToDataType(const FString& InString)
{
	if (InString == TEXT("UNDEFINED") || InString == TEXT("0"))
	{
		return ETensorProtoDataType::UNDEFINED;
	}
	else if (InString == TEXT("FLOAT") || InString == TEXT("1"))
	{
		return ETensorProtoDataType::FLOAT;
	}
	else if (InString == TEXT("UINT8") || InString == TEXT("2"))
	{
		return ETensorProtoDataType::UINT8;
	}
	else if (InString == TEXT("INT8") || InString == TEXT("3"))
	{
		return ETensorProtoDataType::INT8;
	}
	else if (InString == TEXT("UINT16") || InString == TEXT("4"))
	{
		return ETensorProtoDataType::UINT16;
	}
	else if (InString == TEXT("INT16") || InString == TEXT("5"))
	{
		return ETensorProtoDataType::INT16;
	}
	else if (InString == TEXT("INT32") || InString == TEXT("6"))
	{
		return ETensorProtoDataType::INT32;
	}
	else if (InString == TEXT("INT64") || InString == TEXT("7"))
	{
		return ETensorProtoDataType::INT64;
	}
	else if (InString == TEXT("STRING") || InString == TEXT("8"))
	{
		return ETensorProtoDataType::STRING;
	}
	else if (InString == TEXT("BOOL") || InString == TEXT("9"))
	{
		return ETensorProtoDataType::BOOL;
	}
	else if (InString == TEXT("FLOAT16") || InString == TEXT("10"))
	{
		return ETensorProtoDataType::FLOAT16;
	}
	else if (InString == TEXT("DOUBLE") || InString == TEXT("11"))
	{
		return ETensorProtoDataType::DOUBLE;
	}
	else if (InString == TEXT("UINT32") || InString == TEXT("12"))
	{
		return ETensorProtoDataType::UINT32;
	}
	else if (InString == TEXT("UINT64") || InString == TEXT("13"))
	{
		return ETensorProtoDataType::UINT64;
	}
	else if (InString == TEXT("COMPLEX64") || InString == TEXT("14"))
	{
		return ETensorProtoDataType::COMPLEX64;
	}
	else if (InString == TEXT("COMPLEX128") || InString == TEXT("15"))
	{
		return ETensorProtoDataType::COMPLEX128;
	}
	else if (InString == TEXT("BFLOAT16") || InString == TEXT("16"))
	{
		return ETensorProtoDataType::BFLOAT16;
	}
	else
	{
		UE_LOG(LogModelProto, Warning,
			TEXT("FTensorProto::StringToDataType(): Not implemented yet for string %s. Notify us."), *InString);
		return ETensorProtoDataType::UNDEFINED;
	}
}

ETensorProtoDataLocation FTensorProto::StringToDataLocation(const FString& InString) 
{
	if (InString == TEXT("DEFAULT") || InString == TEXT("0") || InString == TEXT(""))
	{
		return ETensorProtoDataLocation::DEFAULT;
	}
	else if (InString == TEXT("EXTERNAL") || InString == TEXT("1"))
	{
		return ETensorProtoDataLocation::EXTERNAL;
	}
	else
	{
		UE_LOG(LogModelProto, Warning,
			TEXT("FTensorProto::StringToDataLocation(): Not implemented yet for string %s. Notify us."), *InString);
		return ETensorProtoDataLocation::DEFAULT;
	}
}



/* FTypeProtoTensor structors
 *****************************************************************************/

FTypeProtoTensor::FTypeProtoTensor()
	: ElemType(-1)
{
}

/* FTypeProtoTensor public functions
 *****************************************************************************/

bool FTypeProtoTensor::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	ElemType = FModelProtoStringParser::GetModelProtoInt64(ProtoMap, TEXT("elem_type"));
	/*bIsLoaded &= */Shape.LoadFromString(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("shape")), InLevel + 1);

	const bool bIsLoaded = ElemType > -1;
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FTypeProtoTensor::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FTypeProtoTensor::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	FModelProtoStringParser::PositiveInt32Or64ToStringAuxiliary(String, TEXT("{0}FTypeProtoTensor.ElemType: {1}\n"), InLineStarted, ElemType);
	String += FString::Format(TEXT("{0}FTypeProtoTensor.Shape: {1}"), { InLineStarted, Shape.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
	return String;
}



/* FTypeProtoMap structors
 *****************************************************************************/

FTypeProtoMap::FTypeProtoMap()
	: KeyType(0)
{
}



/* FSparseTensorProto public functions
 *****************************************************************************/

bool FSparseTensorProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bool bIsLoaded = true;
	const FString ValuesString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("values"));
	if (!ValuesString.IsEmpty())
	{
		bIsLoaded &= Values.LoadFromString(ValuesString, InLevel + 1);
	}
	const FString IndicesString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("indices"));
	if (!IndicesString.IsEmpty())
	{
		bIsLoaded &= Indices.LoadFromString(IndicesString, InLevel + 1);
	}
	Dimensions = FModelProtoStringParser::GetModelProtoInt64Array(ProtoMap, TEXT("dims"));


	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FSparseTensorProto::LoadFromString() failed."));
	}
	return bIsLoaded;

}

FString FSparseTensorProto::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	String += FString::Format(TEXT("{0}FSparseTensorProto.Values: {1}"), { InLineStarted, Values.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
	String += FString::Format(TEXT("{0}FSparseTensorProto.Indices: {1}"), { InLineStarted, Indices.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FSparseTensorProto.Dimensions.Num(): {1}\n"), InLineStarted, Dimensions.Num());
	return String;
}



/* FTypeProto public functions
 *****************************************************************************/

bool FTypeProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	const bool bIsLoaded = TensorType.LoadFromString(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("tensor_type")), InLevel + 1);
	Denotation = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("denotation"));

	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FTypeProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FTypeProto::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	String += FString::Format(TEXT("{0}FTypeProto.TensorType: {1}"), { InLineStarted, TensorType.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FTypeProto.Denotation: {1}\n"), InLineStarted, Denotation);
	return String;
}



/* FAttributeProto structors
 *****************************************************************************/

FAttributeProto::FAttributeProto()
	: Type(EAttributeProtoAttributeType::UNDEFINED)
	, F(-1.f)
	, I(-1)
{
}

/* FAttributeProto public functions
 *****************************************************************************/

bool FAttributeProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bool bIsLoaded = true;
	DocString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("doc_string"));
	F = FModelProtoStringParser::GetModelProtoFloat(ProtoMap, TEXT("f"));
	Floats = FModelProtoStringParser::GetModelProtoFloatArray(ProtoMap, TEXT("floats"));
	if (ProtoMap.Find(TEXT("floats")))
	{
		UE_LOG(LogModelProto, Warning, TEXT("Not implemented for this field (\"floats\") yet. ProtoString = %s."), *InProtoString);
	}
	if (ProtoMap.Find(TEXT("g")))
	{
		UE_LOG(LogModelProto, Warning, TEXT("Not implemented for this field (\"g\") yet. ProtoString = %s."), *InProtoString);
	}
// 	const FString GString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("g"));
// 	if (!GString.IsEmpty())
// 	{
// 		G = MakeShared<FGraphProto>();
// 		/*bIsLoaded &= */G->LoadFromString(
// 		FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("g")), InLevel + 1);
// 	}
// 	if (ProtoMap.Find(TEXT("graphs")))
// 	{
// 		UE_LOG(LogModelProto, Warning, TEXT("Not implemented for this field (\"graphs\") yet. ProtoString = %s."), *InProtoString);
// 	}
// 	/*bIsLoaded &= */Graphs.LoadFromString(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("graphs")), InLevel + 1);
	I = FModelProtoStringParser::GetModelProtoInt64(ProtoMap, TEXT("i"));
	Integers = FModelProtoStringParser::GetModelProtoInt64Array(ProtoMap, TEXT("ints"));
	Name = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("name"));
	S = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("s"));
	Strings = FModelProtoStringParser::GetModelProtoStringArray(ProtoMap, TEXT("strings"));
	const FString TString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("t"));
	if (!TString.IsEmpty())
	{
		bIsLoaded &= T.LoadFromString(TString, InLevel + 1);
	}
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(Tensors, ProtoMap, TEXT("tensors"), InLevel + 1);
	const FString STString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("sparse_tensor"));
	if (!STString.IsEmpty())
	{
		bIsLoaded &= SparseTensor.LoadFromString(STString, InLevel + 1);
	}
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(SparseTensors, ProtoMap, TEXT("sparse_tensors"), InLevel + 1);
	Type = StringToAttributeType(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("type")));

	bIsLoaded &= (!Name.IsEmpty() && Type != EAttributeProtoAttributeType::UNDEFINED);
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FAttributeProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FAttributeProto::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	String += FString::Format(TEXT("{0}FAttributeProto.Type: {1}\n"), { InLineStarted, FString::FromInt((int32)Type) });
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FAttributeProto.Name: {1}\n"), InLineStarted, Name);
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FAttributeProto.DocString: {1}\n"), InLineStarted, DocString);
	if (Type != EAttributeProtoAttributeType::UNDEFINED)
	{
		if (Type == EAttributeProtoAttributeType::FLOAT)
		{
			String += FString::Format(TEXT("{0}FAttributeProto.F: {1}\n"), { InLineStarted, FString::SanitizeFloat(F) });
		}
		else if (Type == EAttributeProtoAttributeType::INT)
		{
			FModelProtoStringParser::PositiveInt32Or64ToStringAuxiliary(String, TEXT("{0}FAttributeProto.I: {1}\n"), InLineStarted, I);
		}
		else if (Type == EAttributeProtoAttributeType::STRING)
		{
			FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FAttributeProto.S: {1}\n"), InLineStarted, S);
		}
		else if (Type == EAttributeProtoAttributeType::TENSOR)
		{
			String += FString::Format(TEXT("{0}FAttributeProto.T: {1}"), { InLineStarted, T.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
		}
		//else if (Type == EAttributeProtoAttributeType::GRAPH)
		//{
		//	if (G.IsValid())
		//	{
		//		String += FString::Format(TEXT("{0}FAttributeProto.G: {1}"), { InLineStarted, G->ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
		//	}
		//}
		else if (Type == EAttributeProtoAttributeType::SPARSE_TENSOR)
		{
			String += FString::Format(TEXT("{0}FAttributeProto.SparseTensor: {1}"), { InLineStarted, SparseTensor.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
		}
		else if (Type == EAttributeProtoAttributeType::FLOATS)
		{
			FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FAttributeProto.Floats.Num(): {1}\n"), InLineStarted, Floats.Num());
		}
		else if (Type == EAttributeProtoAttributeType::INTS)
		{
			FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FAttributeProto.Integers.Num(): {1}\n"), InLineStarted, Integers.Num());
		}
		else if (Type == EAttributeProtoAttributeType::STRINGS)
		{
			FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FAttributeProto.Strings.Num(): {1}\n"), InLineStarted, Strings.Num());
		}
		else if (Type == EAttributeProtoAttributeType::TENSORS)
		{
			FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FAttributeProto.Tensors.Num(): {1}\n"), InLineStarted, Tensors.Num());
		}
		//else if (Type == EAttributeProtoAttributeType::GRAPHS)
		//{
		//	FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FAttributeProto.Graphs.Num(): {1}\n"), InLineStarted, Graphs.Num());
		//}
		else if (Type == EAttributeProtoAttributeType::SPARSE_TENSORS)
		{
			FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FAttributeProto.SparseTensors.Num(): {1}\n"), InLineStarted, SparseTensors.Num());
		}
	}
	return String;
}

/* FAttributeProto private functions
 *****************************************************************************/

EAttributeProtoAttributeType FAttributeProto::StringToAttributeType(const FString& InString)
{
	if (InString == TEXT("UNDEFINED"))
	{
		return EAttributeProtoAttributeType::UNDEFINED;
	}
	else if (InString == TEXT("FLOAT"))
	{
		return EAttributeProtoAttributeType::FLOAT;
	}
	else if (InString == TEXT("INT"))
	{
		return EAttributeProtoAttributeType::INT;
	}
	else if (InString == TEXT("STRING"))
	{
		return EAttributeProtoAttributeType::STRING;
	}
	else if (InString == TEXT("TENSOR"))
	{
		return EAttributeProtoAttributeType::TENSOR;
	}
	else if (InString == TEXT("GRAPH"))
	{
		return EAttributeProtoAttributeType::GRAPH;
	}
	else if (InString == TEXT("SPARSE_TENSOR"))
	{
		return EAttributeProtoAttributeType::SPARSE_TENSOR;
	}
	else if (InString == TEXT("FLOATS"))
	{
		return EAttributeProtoAttributeType::FLOATS;
	}
	else if (InString == TEXT("INTS"))
	{
		return EAttributeProtoAttributeType::INTS;
	}
	else if (InString == TEXT("STRINGS"))
	{
		return EAttributeProtoAttributeType::STRINGS;
	}
	else if (InString == TEXT("TENSORS"))
	{
		return EAttributeProtoAttributeType::TENSORS;
	}
	else if (InString == TEXT("GRAPHS"))
	{
		return EAttributeProtoAttributeType::GRAPHS;
	}
	else if (InString == TEXT("SPARSE_TENSORS"))
	{
		return EAttributeProtoAttributeType::SPARSE_TENSORS;
	}
	else
	{
		UE_LOG(LogModelProto, Warning,
			TEXT("FAttributeProto::StringToAttributeType(): Not implemented yet for string %s. Notify us."), *InString);
		return EAttributeProtoAttributeType::UNDEFINED;
	}
}



/* FValueInfoProto public functions
 *****************************************************************************/

bool FValueInfoProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bool bIsLoaded = true;
	Name = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("name"));
	bIsLoaded &= Type.LoadFromString(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("type")), InLevel + 1);
	DocString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("doc_string"));

	bIsLoaded &= !Name.IsEmpty();
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FValueInfoProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FValueInfoProto::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FValueInfoProto.Name: {1}\n"), InLineStarted, Name);
	String += FString::Format(TEXT("{0}FValueInfoProto.Type: {1}"), { InLineStarted, Type.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FValueInfoProto.DocString: {1}\n"), InLineStarted, DocString);
	return String;
}



/* FTensorAnnotation public functions
 *****************************************************************************/

bool FTensorAnnotation::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bool bIsLoaded = true;
	TensorName = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("tensor_name"));
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(QuantParameterTensorNames, ProtoMap, TEXT("quant_parameter_tensor_names"), InLevel + 1);

	bIsLoaded &= !TensorName.IsEmpty();
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FTensorAnnotation::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FTensorAnnotation::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FTensorAnnotation.TensorName: {1}\n"), InLineStarted, TensorName);
	FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FTensorAnnotation.QuantParmeterTensorNames[{1}]: {2}"), InLineStarted, QuantParameterTensorNames, FModelProtoStringParser::ToStringSeparator + InLineStarted);
	return String;
}



/* FNodeProto public functions
 *****************************************************************************/

bool FNodeProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bool bIsLoaded = true;
	Input = FModelProtoStringParser::GetModelProtoStringArray(ProtoMap, TEXT("input"));
	Output = FModelProtoStringParser::GetModelProtoStringArray(ProtoMap, TEXT("output"));
	Name = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("name"));
	OperatorType = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("op_type"));
	Domain = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("domain"));
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(Attribute, ProtoMap, TEXT("attribute"), InLevel + 1);
	DocString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("doc_string"));

	bIsLoaded &= !OperatorType.IsEmpty();
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FNodeProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FNodeProto::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	FModelProtoStringParser::StringArrayToStringAuxiliary(String, TEXT("{0}FNodeProto.Input: [{1}]\n"), InLineStarted, Input);
	FModelProtoStringParser::StringArrayToStringAuxiliary(String, TEXT("{0}FNodeProto.Output: [{1}]\n"), InLineStarted, Output);
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FNodeProto.Name: {1}\n"), InLineStarted, Name);
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FNodeProto.OperatorType: {1}\n"), InLineStarted, OperatorType);
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FNodeProto.Domain: {1}\n"), InLineStarted, Domain);
	FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FNodeProto.Attribute[{1}]: {2}"), InLineStarted, Attribute, FModelProtoStringParser::ToStringSeparator + InLineStarted);
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FNodeProto.DocString: {1}\n"), InLineStarted, DocString);
	return String;
}



/* FGraphProto structors
 *****************************************************************************/

FGraphProto::FGraphProto()
	: bIsLoaded(false)
{
}

/* FGraphProto public functions
 *****************************************************************************/

bool FGraphProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bIsLoaded = true;
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(Node, ProtoMap, TEXT("node"), InLevel + 1);
	Name = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("name"));
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(Initializer, ProtoMap, TEXT("initializer"), InLevel + 1);
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(SparseInitializer, ProtoMap, TEXT("sparse_initializer"), InLevel + 1);
	DocString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("doc_string"));
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(Input, ProtoMap, TEXT("input"), InLevel + 1);
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(Output, ProtoMap, TEXT("output"), InLevel + 1);
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(ValueInfo, ProtoMap, TEXT("value_info"), InLevel + 1);
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(QuantizationAnnotation, ProtoMap, TEXT("quantization_annotation"), InLevel + 1);

	bIsLoaded &= !Name.IsEmpty();
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FGraphProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FGraphProto::ToString(const FString& InLineStarted) const
{
	if (bIsLoaded)
	{
		FString String = TEXT("\n");
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FGraphProto.Node[{1}]: {2}"), InLineStarted, Node, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FGraphProto.Name: {1}\n"), InLineStarted, Name);
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FGraphProto.Initializer[{1}]: {2}"), InLineStarted, Initializer, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		FModelProtoStringParser::GreaterThan0Int32Or64ToStringAuxiliary(String, TEXT("{0}FGraphProto.SparseInitializer.Num(): {1}\n"), InLineStarted, SparseInitializer.Num());
		FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FGraphProto.DocString: {1}\n"), InLineStarted, DocString);
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FGraphProto.Input[{1}]: {2}"), InLineStarted, Input, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FGraphProto.Output[{1}]: {2}"), InLineStarted, Output, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FGraphProto.ValueInfo[{1}]: {2}"), InLineStarted, ValueInfo, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FGraphProto.QuantizationAnnoation[{1}]: {2}"), InLineStarted, QuantizationAnnotation, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		return String;
	}
	else
	{
		return TEXT("");
	}
}



/* FTrainingProto structors
 *****************************************************************************/

FTrainingInfoProto::FTrainingInfoProto()
	: bIsLoaded(false)
{
}

/* FTrainingInfoProto public functions
 *****************************************************************************/ 

bool FTrainingInfoProto::LoadFromString(const FString & InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bIsLoaded = true;
	bIsLoaded &= Initialization.LoadFromString(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("initialization")), InLevel + 1);
	bIsLoaded &= Algorithm.LoadFromString(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("algorithm")), InLevel + 1);
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(InitializationBinding, ProtoMap, TEXT("initialization_binding"), InLevel + 1);
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(UpdateBinding, ProtoMap, TEXT("update_binding"), InLevel + 1);

	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FTrainingInforProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FTrainingInfoProto::ToString(const FString& InLineStarted) const
{
	if (bIsLoaded)
	{
		FString String = TEXT("\n");
		String += FString::Format(TEXT("{0}FTrainingProto.Initialization: {1}"), { InLineStarted, Initialization.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
		String += FString::Format(TEXT("{0}FTrainingProto.Algorithm: {1}"), { InLineStarted, Algorithm.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FTrainingProto.InitializationBinding[{1}]: {2}"), InLineStarted, InitializationBinding, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FTrainingProto.UpdateBinding[{1}]: {2}"), InLineStarted, UpdateBinding, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		return String;
	}
	else
	{
		return TEXT("");
	}
}



/* FOperatorSetIdProto structors
 *****************************************************************************/

FOperatorSetIdProto::FOperatorSetIdProto()
	: Version(-1)
{
}

/* FOperatorSetIdProto public functions
 *****************************************************************************/

bool FOperatorSetIdProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	Domain = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("domain"));
	Version = FModelProtoStringParser::GetModelProtoInt64(ProtoMap, TEXT("version"));

	const bool bIsLoaded = (Version > -1);
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FOperatorSetIdProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FOperatorSetIdProto::ToString(const FString& InLineStarted) const
{
	FString String = TEXT("\n");
	FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FOperatorSetIdProto.Domain: {1}\n"), InLineStarted, Domain);
	String += FString::Format(TEXT("{0}FOperatorSetIdProto.Version: {1}\n"), { InLineStarted, FString::FromInt(Version) });
	return String;
}



/* FModelProto structors
 *****************************************************************************/

FModelProto::FModelProto()
	: bIsLoaded(false)
	, IRVersion(-1)
	, ModelVersion(-1)
{
}

/* FModelProto public functions
 *****************************************************************************/

bool FModelProto::LoadFromString(const FString& InProtoString, const int32 InLevel)
{
	// Sanity check
	if (InProtoString.Len() < 1)
	{
		return false;
	}

	// FString text into [key, value] maps
	const TMap<FString, TArray<FString>> ProtoMap = FModelProtoStringParser::ModelProtoStringToMap(InProtoString, InLevel);

	// Fill class variables
	bIsLoaded = true;
	IRVersion = FModelProtoStringParser::GetModelProtoInt64(ProtoMap, TEXT("ir_version"));
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(OpsetImport, ProtoMap, TEXT("opset_import"), InLevel + 1);
	ProducerName = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("producer_name"));
	ProducerVersion = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("producer_version"));
	Domain = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("domain"));
	ModelVersion = FModelProtoStringParser::GetModelProtoInt64(ProtoMap, TEXT("model_version"));
	DocString = FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("doc_string"));
	bIsLoaded &= Graph.LoadFromString(FModelProtoStringParser::GetModelProtoStringOrEmpty(ProtoMap, TEXT("graph")), InLevel + 1);
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(MetadataProps, ProtoMap, TEXT("metadata_props"), InLevel + 1);
	bIsLoaded &= FModelProtoStringParser::GetModelProtoArray(TrainingInfo, ProtoMap, TEXT("training_info"), InLevel + 1);

	bIsLoaded &= IRVersion > -1 && OpsetImport.Num() > 0;
	if (!bIsLoaded)
	{
		UE_LOG(LogModelProto, Warning, TEXT("FModelProto::LoadFromString() failed."));
	}
	return bIsLoaded;
}

FString FModelProto::ToString(const FString& InLineStarted) const
{
	if (bIsLoaded)
	{
		FString String;
		FModelProtoStringParser::PositiveInt32Or64ToStringAuxiliary(String, TEXT("{0}FModelProto.IRVersion: {1}\n"), InLineStarted, IRVersion);
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FModelProto.OpsetImport[{1}]: {2}"), InLineStarted, OpsetImport, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FModelProto.ProducerName: {1}\n"), InLineStarted, ProducerName);
		FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FModelProto.ProducerVersion: {1}\n"), InLineStarted, ProducerVersion);
		FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FModelProto.Domain: {1}\n"), InLineStarted, Domain);
		FModelProtoStringParser::PositiveInt32Or64ToStringAuxiliary(String, TEXT("{0}FModelProto.ModelVersion: {1}\n"), InLineStarted, ModelVersion);
		FModelProtoStringParser::StringToStringIfNotEmptyAuxiliary(String, TEXT("{0}FModelProto.DocString: {1}\n"), InLineStarted, DocString);
		String += FString::Format(TEXT("{0}FModelProto.Graph: {1}"), { InLineStarted, Graph.ToString(FModelProtoStringParser::ToStringSeparator + InLineStarted) });
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FModelProto.MetadataProps[{1}]: {2}"), InLineStarted, MetadataProps, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		FModelProtoStringParser::ArrayTToStringAuxiliary(String, TEXT("{0}FModelProto.TrainingInfo[{1}]: {2}"), InLineStarted, TrainingInfo, FModelProtoStringParser::ToStringSeparator + InLineStarted);
		return String;
	}
	else
	{
		return TEXT("");
	}
}

const FGraphProto& FModelProto::GetGraph() const
{
	return Graph;
}
