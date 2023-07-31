// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeRangeFromScalar.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/RefCounted.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRangeFromScalarPrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeRangeFromScalar::Private::s_type =
            NODE_TYPE( "RangeFromScalar", NodeRange::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeRangeFromScalar, EType::FromScalar, Node, Node::EType::Range)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeRangeFromScalar::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeRangeFromScalar::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

        Node* pResult = nullptr;

		switch (i)
		{
        case 0: pResult = m_pD->m_pSize.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
    void NodeRangeFromScalar::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pSize = dynamic_cast<NodeScalar*>(pNode.get()); break;
		}
	}


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
    const char* NodeRangeFromScalar::GetName() const
	{
        return m_pD->m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
    void NodeRangeFromScalar::SetName( const char* strName )
	{
        if (!strName)
        {
            m_pD->m_name.clear();
        }
        else
        {
            m_pD->m_name = strName;
        }
	}

}

