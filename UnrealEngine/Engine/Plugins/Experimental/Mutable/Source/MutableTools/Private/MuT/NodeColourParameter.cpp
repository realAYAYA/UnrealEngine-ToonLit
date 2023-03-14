// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourParameter.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeColourParameterPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRange.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeColourParameter::Private::s_type =
			NODE_TYPE( "ColourParameter", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourParameter, EType::Parameter, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeColourParameter::GetInputCount() const
	{
        return  m_pD->m_ranges.Num();
    }


	//---------------------------------------------------------------------------------------------
    Node* NodeColourParameter::GetInputNode( int i ) const
	{
        check( i<GetInputCount() );
        if (i<GetInputCount())
        {
            return m_pD->m_ranges[i].get();
        }
        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
    void NodeColourParameter::SetInputNode( int i, NodePtr n )
	{
        check( i<GetInputCount() );
        if (i<GetInputCount())
        {
            m_pD->m_ranges[i] = dynamic_cast<NodeRange*>(n.get());
        }
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const char* NodeColourParameter::GetName() const
	{
		return m_pD->m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetName( const char* strName )
	{
		if ( strName )
		{
			m_pD->m_name = strName;
		}
		else
		{
			m_pD->m_name = "";
		}
	}


	const char* NodeColourParameter::GetUid() const
	{
		return m_pD->m_uid.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetUid( const char* strUid )
	{
		if ( strUid )
		{
			m_pD->m_uid = strUid;
		}
		else
		{
			m_pD->m_uid = "";
		}
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::GetDefaultValue( float* pR, float* pG, float* pB ) const
	{
		if (*pR) *pR = m_pD->m_defaultValue[0];
		if (*pG) *pG = m_pD->m_defaultValue[1];
		if (*pB) *pB = m_pD->m_defaultValue[2];
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetDefaultValue( float r, float g, float b )
	{
		m_pD->m_defaultValue = vec3<float>( r, g, b );
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


