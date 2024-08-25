// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeRangeFromScalar.h"

#include "Misc/AssertionMacros.h"
#include "MuR/RefCounted.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRangeFromScalarPrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeRangeFromScalar::Private::s_type =
            FNodeType( "RangeFromScalar", NodeRange::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeRangeFromScalar, EType::FromScalar, Node, Node::EType::Range)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	Ptr<NodeScalar> NodeRangeFromScalar::GetSize() const
	{
        return m_pD->m_pSize.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeRangeFromScalar::SetSize( const Ptr<NodeScalar>& pNode )
	{
        m_pD->m_pSize = pNode;
	}


	//---------------------------------------------------------------------------------------------
    const FString& NodeRangeFromScalar::GetName() const
	{
        return m_pD->m_name;
	}


	//---------------------------------------------------------------------------------------------
    void NodeRangeFromScalar::SetName( const FString& strName )
	{
        m_pD->m_name = strName;
	}

}

