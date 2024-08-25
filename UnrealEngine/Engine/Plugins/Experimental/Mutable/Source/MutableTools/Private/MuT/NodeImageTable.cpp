// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageTable.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageTable::Private::s_type =
			FNodeType( "TableImage", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageTable, EType::Table, Node, Node::EType::Image);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetColumn( const FString& strName )
	{
		m_pD->ColumnName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetParameterName( const FString& strName )
	{
		m_pD->ParameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetTable( TablePtr pTable )
	{
		m_pD->Table = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeImageTable::GetTable() const
	{
		return m_pD->Table;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetMaxTextureSize(uint16 MaxTextureSize)
	{
		m_pD->MaxTextureSize = MaxTextureSize;
	}


	//---------------------------------------------------------------------------------------------
	uint16 NodeImageTable::GetMaxTextureSize()
	{
		return m_pD->MaxTextureSize;
	}

	
	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetNoneOption(bool bAddNoneOption)
	{
		m_pD->bNoneOption = bAddNoneOption;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetReferenceImageDescriptor(const FImageDesc& ImageDesc)
	{
		m_pD->ReferenceImageDesc = ImageDesc;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetDefaultRowName(const FString& RowName)
	{
		m_pD->DefaultRowName = RowName;
	}

}


