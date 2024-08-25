// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshTable.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeMeshTable::Private::s_type =
			FNodeType( "TableMesh", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshTable, EType::Table, Node, Node::EType::Mesh);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetColumn( const FString& strName )
	{
		m_pD->ColumnName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetParameterName( const FString& strName )
	{
		m_pD->ParameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetTable( TablePtr pTable )
	{
		m_pD->Table = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeMeshTable::GetTable() const
	{
		return m_pD->Table;
	}


	//---------------------------------------------------------------------------------------------
	int NodeMeshTable::GetLayoutCount() const
	{
		return m_pD->Layouts.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetLayoutCount( int i )
	{
		m_pD->Layouts.SetNum( i );
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
		m_pD->Layouts[i] = pLayout;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshTable::Private::GetLayout( int i ) const
	{
		NodeLayoutPtr pResult;

		if ( i>=0 && i< Layouts.Num() )
		{
			pResult = Layouts[i];
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetNoneOption(bool bAddNoneOption)
	{
		m_pD->bNoneOption = bAddNoneOption;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetDefaultRowName(const FString& RowName)
	{
		m_pD->DefaultRowName = RowName;
	}

}


