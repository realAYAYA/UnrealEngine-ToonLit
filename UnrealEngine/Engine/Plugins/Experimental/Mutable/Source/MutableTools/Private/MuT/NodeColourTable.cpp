// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourTable.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeColourTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"



namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeColourTable::Private::s_type =
			NODE_TYPE( "TableColour", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourTable, EType::Table, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeColourTable::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeColourTable::GetInputNode( int i ) const
	{
		check( i >=0 && i < 0 );
        (void)i;
        return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeColourTable::SetInputNode( int i, NodePtr )
	{
		check( i >=0 && i < 0 );
        (void)i;
        //m_pD->m_pObject = dynamic_cast<NodeObject*>( pNode.get() );
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const char* NodeColourTable::GetColumn() const
	{
		return m_pD->m_columnName.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetColumn( const char* strName )
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
	void NodeColourTable::SetParameterName( const char* strName )
	{
		m_pD->m_parameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetTable( TablePtr pTable )
	{
		m_pD->m_pTable = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeColourTable::GetTable() const
	{
		return m_pD->m_pTable;
	}


}


