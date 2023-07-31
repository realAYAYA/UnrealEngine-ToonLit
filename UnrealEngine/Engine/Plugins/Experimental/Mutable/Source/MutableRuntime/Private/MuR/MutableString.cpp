// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/MutableString.h"


namespace mu {

	//---------------------------------------------------------------------------------------------
	String::String( const char* v )
	{
		if (v)
		{
            m_value = v;
        }
	}


	//---------------------------------------------------------------------------------------------
	void String::Serialise( const String* p, OutputArchive& arch )
	{
		arch << *p;
	}


	//---------------------------------------------------------------------------------------------
	Ptr<String> String::StaticUnserialise( InputArchive& arch )
	{
		Ptr<String> pResult = new String();
		arch >> *pResult;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	Ptr<String> String::Clone() const
	{
		Ptr<String> pResult = new String();
		pResult->m_value = m_value;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	const char* String::GetValue() const
	{
        return m_value.c_str();
	}

}

