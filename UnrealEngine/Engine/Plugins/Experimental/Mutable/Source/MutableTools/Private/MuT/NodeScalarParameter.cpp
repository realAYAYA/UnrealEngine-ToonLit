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
	NODE_TYPE NodeScalarParameter::Private::s_type =
			NODE_TYPE( "ScalarParameter", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeScalarParameter, EType::Parameter, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	int NodeScalarParameter::GetInputCount() const
	{
        return m_pD->m_ranges.Num();
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeScalarParameter::GetInputNode( int i ) const
	{
        check( i<GetInputCount() );

        int rangeCount = int(m_pD->m_ranges.Num());
		if ( i < rangeCount )
        {
            return m_pD->m_ranges[i].get();
        }
        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarParameter::SetInputNode( int i, NodePtr n )
	{
        check( i<GetInputCount() );
        int rangeCount = int(m_pD->m_ranges.Num());
        if ( i < rangeCount )
        {
            m_pD->m_ranges[i] = dynamic_cast<NodeRange*>(n.get());
        }
    }


	//---------------------------------------------------------------------------------------------
	const char* NodeScalarParameter::GetName() const
	{
		return m_pD->m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarParameter::SetName( const char* strName )
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


	const char* NodeScalarParameter::GetUid() const
	{
		return m_pD->m_uid.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarParameter::SetUid( const char* strUid )
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
	float NodeScalarParameter::GetDefaultValue() const
	{
		return m_pD->m_defaultValue;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarParameter::SetDefaultValue( float v )
	{
		m_pD->m_defaultValue = v;
	}


	//---------------------------------------------------------------------------------------------
	PARAMETER_DETAILED_TYPE NodeScalarParameter::GetDetailedType() const
	{
		return m_pD->m_detailedType;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarParameter::SetDetailedType( PARAMETER_DETAILED_TYPE t )
	{
		m_pD->m_detailedType = t;
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


