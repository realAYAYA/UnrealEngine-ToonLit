// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeStringParameter.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImage.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeStringParameterPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeStringParameter::Private::s_type =
			FNodeType( "StringParameter", NodeString::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeStringParameter, EType::Parameter, Node, Node::EType::String)


	//---------------------------------------------------------------------------------------------
	void NodeStringParameter::SetName( const FString& Name )
	{
		m_pD->m_name = Name;
	}


	//---------------------------------------------------------------------------------------------
	void NodeStringParameter::SetUid( const FString& Uid )
	{
		m_pD->m_uid = Uid;
	}


	//---------------------------------------------------------------------------------------------
	void NodeStringParameter::SetDefaultValue( const FString& v )
	{
		m_pD->m_defaultValue = v;
	}


    //---------------------------------------------------------------------------------------------
    void NodeStringParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.SetNum(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeStringParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<int(m_pD->m_ranges.Num()) );
        if ( i>=0 && i<int(m_pD->m_ranges.Num()) )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }


}


