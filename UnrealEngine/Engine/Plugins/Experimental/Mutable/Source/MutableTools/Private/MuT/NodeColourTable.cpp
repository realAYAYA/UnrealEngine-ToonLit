// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourTable.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColourTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"



namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeColourTable::Private::s_type =
			FNodeType( "TableColour", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourTable, EType::Table, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetColumn( const FString& strName )
	{
		m_pD->ColumnName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetParameterName( const FString& strName )
	{
		m_pD->ParameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetTable( TablePtr pTable )
	{
		m_pD->Table = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeColourTable::GetTable() const
	{
		return m_pD->Table;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetNoneOption(bool bAddNoneOption)
	{
		m_pD->bNoneOption = bAddNoneOption;
	}

	
	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetDefaultRowName(const FString RowName)
	{
		m_pD->DefaultRowName = RowName;
	}

}


