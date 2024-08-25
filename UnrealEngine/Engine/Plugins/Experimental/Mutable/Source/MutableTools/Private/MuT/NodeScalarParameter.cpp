// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarParameter.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImage.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalarParameterPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeScalarParameter::Private::s_type =
			FNodeType( "ScalarParameter", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeScalarParameter, EType::Parameter, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	void NodeScalarParameter::SetName( const FString& Name )
	{
		m_pD->m_name = Name;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarParameter::SetUid( const FString& Uid )
	{
		m_pD->m_uid = Uid;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarParameter::SetDefaultValue( float v )
	{
		m_pD->m_defaultValue = v;
	}


    //---------------------------------------------------------------------------------------------
    void NodeScalarParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.SetNum(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeScalarParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<int(m_pD->m_ranges.Num()) );
        if ( i>=0 && i<int(m_pD->m_ranges.Num()) )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }


}


