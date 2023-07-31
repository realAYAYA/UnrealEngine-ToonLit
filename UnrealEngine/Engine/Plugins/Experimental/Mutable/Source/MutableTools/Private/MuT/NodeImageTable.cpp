// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageTable.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeImageTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageTable::Private::s_type =
			NODE_TYPE( "TableImage", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageTable, EType::Table, Node, Node::EType::Image);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageTable::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageTable::GetInputNode( int i ) const
	{
		check( i >=0 && i < 0 );
        (void)i;
        return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageTable::SetInputNode( int i, NodePtr )
	{
		check( i >=0 && i < 0 );
        (void)i;
        //m_pD->m_pObject = dynamic_cast<NodeObject*>( pNode.get() );
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const char* NodeImageTable::GetColumn() const
	{
		return m_pD->m_columnName.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetColumn( const char* strName )
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
	void NodeImageTable::SetParameterName( const char* strName )
	{
		m_pD->m_parameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetTable( TablePtr pTable )
	{
		m_pD->m_pTable = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeImageTable::GetTable() const
	{
		return m_pD->m_pTable;
	}


}


