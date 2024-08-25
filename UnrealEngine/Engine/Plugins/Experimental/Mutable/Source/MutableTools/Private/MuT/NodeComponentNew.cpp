// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeComponentNew.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeComponentNewPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurface.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeComponentNew::Private::s_type =
			FNodeType( "NewComponent", NodeComponent::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeComponentNew, EType::New, Node, Node::EType::Component)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const FString& NodeComponentNew::GetName() const
	{
		return m_pD->m_name;
	}


	//---------------------------------------------------------------------------------------------
	void NodeComponentNew::SetName( const FString& strName )
	{
		m_pD->m_name = strName;
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


