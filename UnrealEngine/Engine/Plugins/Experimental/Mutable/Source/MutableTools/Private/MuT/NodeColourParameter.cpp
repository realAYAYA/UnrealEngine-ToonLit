// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourParameter.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeColourParameterPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRange.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeColourParameter::Private::s_type =
			FNodeType( "ColourParameter", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourParameter, EType::Parameter, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetName( const FString& strName )
	{
		m_pD->m_name = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetUid( const FString& strUid )
	{
		m_pD->m_uid = strUid;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetDefaultValue(FVector4f Value)
	{
		m_pD->m_defaultValue = Value;
	}


    //---------------------------------------------------------------------------------------------
    void NodeColourParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.SetNum(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeColourParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<int(m_pD->m_ranges.Num()) );
        if ( i>=0 && i<int(m_pD->m_ranges.Num()) )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }

}


