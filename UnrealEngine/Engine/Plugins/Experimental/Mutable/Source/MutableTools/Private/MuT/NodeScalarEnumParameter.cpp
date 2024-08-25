// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarEnumParameter.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarEnumParameterPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeScalarEnumParameter::Private::s_type =
			FNodeType( "ScalarEnumParameter", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeScalarEnumParameter, EType::EnumParameter, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	const FString& NodeScalarEnumParameter::GetName() const
	{
		return m_pD->m_name;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetName( const FString& strName )
	{
		m_pD->m_name = strName;
	}


	const FString& NodeScalarEnumParameter::GetUid() const
	{
		return m_pD->m_uid;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetUid( const FString& strUid )
	{
		m_pD->m_uid = strUid;
	}


	//---------------------------------------------------------------------------------------------
	int NodeScalarEnumParameter::GetDefaultValueIndex() const
	{
		return m_pD->m_defaultValue;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetDefaultValueIndex( int i )
	{
		m_pD->m_defaultValue = i;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetValueCount( int i )
	{
		m_pD->m_options.SetNum(i);
	}


	//---------------------------------------------------------------------------------------------
	int NodeScalarEnumParameter::GetValueCount() const
	{
		return (int)m_pD->m_options.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetValue( int i, float value, const FString& strName )
	{
		check( i>=0 && i<(int)m_pD->m_options.Num() );
		m_pD->m_options[i].name = strName;
		m_pD->m_options[i].value = value;
	}


    //---------------------------------------------------------------------------------------------
    void NodeScalarEnumParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.SetNum(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeScalarEnumParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<int(m_pD->m_ranges.Num()) );
        if ( i>=0 && i<int(m_pD->m_ranges.Num()) )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }

}


