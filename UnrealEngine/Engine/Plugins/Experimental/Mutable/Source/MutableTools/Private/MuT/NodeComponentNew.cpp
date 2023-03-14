// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeComponentNew.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeComponentNewPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurface.h"

#include <memory>
#include <utility>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeComponentNew::Private::s_type =
			NODE_TYPE( "NewComponent", NodeComponent::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeComponentNew, EType::New, Node, Node::EType::Component)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeComponentNew::GetInputCount() const
	{
        return m_pD->m_surfaces.Num();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeComponentNew::GetInputNode( int i ) const
	{
        check( i >=0 && i < GetInputCount() );

		NodePtr pResult;

        if ( i<m_pD->m_surfaces.Num() )
		{
            pResult = m_pD->m_surfaces[i];
		}

		return pResult.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeComponentNew::SetInputNode( int i, NodePtr pNode )
	{
        check( i >=0 && i < GetInputCount() );

        if ( i<m_pD->m_surfaces.Num() )
		{
            m_pD->m_surfaces[ i ] = dynamic_cast<NodeSurface*>(pNode.get());
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const char* NodeComponentNew::GetName() const
	{
		const char* strResult = m_pD->m_name.c_str();

		return strResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeComponentNew::SetName( const char* strName )
	{
		if (strName)
		{
			m_pD->m_name = strName;
		}
		else
		{
			m_pD->m_name = "";
		}
	}


	//---------------------------------------------------------------------------------------------
	uint16 mu::NodeComponentNew::GetId() const
    {
    	return m_pD->m_id;
    }


	//---------------------------------------------------------------------------------------------
	void mu::NodeComponentNew::SetId(uint16 id)
    {
    	m_pD->m_id = id;
    }


	//---------------------------------------------------------------------------------------------
    int NodeComponentNew::GetSurfaceCount() const
	{
        return m_pD->m_surfaces.Num();
	}


	//---------------------------------------------------------------------------------------------
    void NodeComponentNew::SetSurfaceCount( int num )
	{
		check( num >=0 );
        m_pD->m_surfaces.SetNum( num );
	}


	//---------------------------------------------------------------------------------------------
    NodeSurface* NodeComponentNew::GetSurface( int index ) const
	{
        check( index >=0 && index < m_pD->m_surfaces.Num() );

        return m_pD->m_surfaces[ index ].get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeComponentNew::SetSurface( int index, NodeSurface* pNode )
	{
        check( index >=0 && index < m_pD->m_surfaces.Num() );

        m_pD->m_surfaces[ index ] = pNode;
	}

}


