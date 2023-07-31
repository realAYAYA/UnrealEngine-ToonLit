// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshTable.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"

#include <memory>
#include <utility>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshTable::Private::s_type =
			NODE_TYPE( "TableMesh", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshTable, EType::Table, Node, Node::EType::Mesh);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshTable::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshTable::GetInputNode( int i ) const
	{
		check( i >=0 && i < 0 );
        (void)i;
        return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshTable::SetInputNode( int i, NodePtr )
	{
		check( i >=0 && i < 0 );
        (void)i;
        //m_pD->m_pObject = dynamic_cast<NodeObject*>( pNode.get() );
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const char* NodeMeshTable::GetColumn() const
	{
		return m_pD->m_columnName.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetColumn( const char* strName )
	{
		if (strName)
		{
			m_pD->m_columnName = strName;
		}
		else
		{
			m_pD->m_columnName = "";
		}
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetParameterName( const char* strName )
	{
		m_pD->m_parameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetTable( TablePtr pTable )
	{
		m_pD->m_pTable = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeMeshTable::GetTable() const
	{
		return m_pD->m_pTable;
	}


	//---------------------------------------------------------------------------------------------
	int NodeMeshTable::GetLayoutCount() const
	{
		return m_pD->m_layouts.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetLayoutCount( int i )
	{
		m_pD->m_layouts.SetNum( i );
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshTable::GetLayout( int i ) const
	{
		check( i>=0 && i<GetLayoutCount() );
		return m_pD->GetLayout( i );
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetLayout( int i, NodeLayoutPtr pLayout )
	{
		check( i>=0 && i<GetLayoutCount() );
		m_pD->m_layouts[i] = pLayout;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshTable::Private::GetLayout( int i ) const
	{
		NodeLayoutPtr pResult;

		if ( i>=0 && i<m_layouts.Num() )
		{
			pResult = m_layouts[i];
		}

		return pResult;
	}

}


