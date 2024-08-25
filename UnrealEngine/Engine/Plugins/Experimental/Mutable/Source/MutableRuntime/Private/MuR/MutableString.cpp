// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/MutableString.h"


namespace mu {

	//---------------------------------------------------------------------------------------------
	String::String( const FString& InValue)
	{
        Value = InValue;
	}


	////---------------------------------------------------------------------------------------------
	//void String::Serialise( const String* p, OutputArchive& arch )
	//{
	//	arch << *p;
	//}


	////---------------------------------------------------------------------------------------------
	//Ptr<String> String::StaticUnserialise( InputArchive& arch )
	//{
	//	Ptr<String> pResult = new String();
	//	arch >> *pResult;
	//	return pResult;
	//}


	//---------------------------------------------------------------------------------------------
	Ptr<String> String::Clone() const
	{
		Ptr<String> pResult = new String(Value);
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	int32 String::GetDataSize() const
	{
		return sizeof(String) + Value.GetAllocatedSize();
	}


	//---------------------------------------------------------------------------------------------
	const FString& String::GetValue() const
	{
        return Value;
	}

}

