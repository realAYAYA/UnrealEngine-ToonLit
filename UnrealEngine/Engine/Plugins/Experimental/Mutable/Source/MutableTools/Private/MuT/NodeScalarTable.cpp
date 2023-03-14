// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarTable.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarTablePrivate.h"
#include "MuT/Table.h"



namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeScalarTable::Private::s_type =
			NODE_TYPE( "TableScalar", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE(NodeScalarTable, EType::Table, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeScalarTable::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeScalarTable::GetInputNode( int i ) const
	{
		check( i >=0 && i < 0 );
        (void)i;
        return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarTable::SetInputNode( int i, NodePtr )
	{
		check( i >=0 && i < 0 );
        (void)i;
        //m_pD->m_pObject = dynamic_cast<NodeObject*>( pNode.get() );
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const char* NodeScalarTable::GetColumn() const
	{
		return m_pD->m_columnName.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarTable::SetColumn( const char* strName )
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
	void NodeScalarTable::SetParameterName( const char* strName )
	{
		m_pD->m_parameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarTable::SetTable( TablePtr pTable )
	{
		m_pD->m_pTable = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeScalarTable::GetTable() const
	{
		return m_pD->m_pTable;
	}


}


