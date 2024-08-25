// Copyright Epic Games, Inc. All Rights Reserved.


#include "Misc/AssertionMacros.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeBoolPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRange.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeBoolParameter::Private::s_type =
			FNodeType( "BoolParameter", NodeBool::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeBoolParameter, EType::Parameter, Node, Node::EType::Bool);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeBoolParameter::SetName( const FString& strName )
	{
		m_pD->m_name = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeBoolParameter::SetDefaultValue( bool v )
	{
		m_pD->m_defaultValue = v;
	}


    //---------------------------------------------------------------------------------------------
    void NodeBoolParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.SetNum(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeBoolParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<int(m_pD->m_ranges.Num()) );
        if ( i>=0 && i<int(m_pD->m_ranges.Num()) )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }

}


