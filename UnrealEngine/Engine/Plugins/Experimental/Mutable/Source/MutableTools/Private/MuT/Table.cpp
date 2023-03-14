// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Table.h"

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/MutableMath.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/TablePrivate.h"

#include <memory>
#include <string>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	Table::Table()
	{
		m_pD = new Private();
	}


	//---------------------------------------------------------------------------------------------
	Table::~Table()
	{
        check( m_pD );
		delete m_pD;
		m_pD = 0;
	}


	//---------------------------------------------------------------------------------------------
	void Table::Serialise( const Table* p, OutputArchive& arch )
	{
		arch << *p->m_pD;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr Table::StaticUnserialise( InputArchive& arch )
	{
		TablePtr pResult = new Table();
		arch >> *pResult->m_pD;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	Table::Private* Table::GetPrivate() const
	{
		return m_pD;
	}


	//---------------------------------------------------------------------------------------------
	void Table::SetName( const char* strName )
	{
		m_pD->m_name = strName;
	}


	//---------------------------------------------------------------------------------------------
	const char* Table::GetName() const
	{
		return m_pD->m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
	int Table::AddColumn( const char* strName, TABLE_COLUMN_TYPE type )
	{
		int result = m_pD->m_columns.Num();

		TABLE_COLUMN c;

		if (strName)
		{
			c.m_name = strName;
		}
		c.m_type = type;

		m_pD->m_columns.Add(c);

		// Add it to all rows
		for ( std::size_t r=0; r<m_pD->m_rows.Num(); ++r )
		{
			m_pD->m_rows[r].m_values.Add( TABLE_VALUE() );
		}

		return result;
	}


	//---------------------------------------------------------------------------------------------
	int Table::FindColumn( const char* strName ) const
	{
		int res = -1;

		for ( std::size_t c=0; c<m_pD->m_columns.Num(); ++c )
		{
			if ( m_pD->m_columns[c].m_name == strName )
			{
				res = (int)c;
			}
		}

		return res;
	}


	//---------------------------------------------------------------------------------------------
    void Table::AddRow( uint32_t id )
	{
		check( m_pD->FindRow(id)<0 );

		TABLE_ROW r;
		r.m_id = id;
		r.m_values.SetNum( m_pD->m_columns.Num() );
		m_pD->m_rows.Add( r );
	}


	//---------------------------------------------------------------------------------------------
	void Table::SetNoneOption(bool bAddOption)
	{
		m_pD->m_NoneOption = bAddOption;
	}


	//---------------------------------------------------------------------------------------------
    void Table::SetCell( int column, uint32_t rowId, float value )
	{
		int row = m_pD->FindRow(rowId);
		check( row>=0 );
		check( column < (int)m_pD->m_columns.Num() );
		check( column < (int)m_pD->m_rows[row].m_values.Num() );
		check( m_pD->m_columns[column].m_type == TCT_SCALAR );

		m_pD->m_rows[ row ].m_values[column].m_scalar = value;
	}


	//---------------------------------------------------------------------------------------------
    void Table::SetCell( int column, uint32_t rowId, float r, float g, float b )
	{
		int row = m_pD->FindRow(rowId);
		check( row>=0 );
		check( column < (int)m_pD->m_columns.Num() );
		check( column < (int)m_pD->m_rows[row].m_values.Num() );
		check( m_pD->m_columns[column].m_type == TCT_COLOUR );

		m_pD->m_rows[ row ].m_values[column].m_colour = vec3<float>( r, g, b );
	}


	//---------------------------------------------------------------------------------------------
    void Table::SetCell( int column, uint32_t rowId, Image* pImage )
	{
		int row = m_pD->FindRow(rowId);
		check( row>=0 );
		check( column < (int)m_pD->m_columns.Num() );
		check( column < (int)m_pD->m_rows[row].m_values.Num() );
		check( m_pD->m_columns[column].m_type == TCT_IMAGE );

		m_pD->m_rows[ row ].m_values[column].m_pProxyImage = new ResourceProxyMemory<Image>(pImage);
	}


	//---------------------------------------------------------------------------------------------
    void Table::SetCell( int column, uint32_t rowId, Mesh* pMesh )
	{
		int row = m_pD->FindRow(rowId);
		check( row>=0 );
		check( column < (int)m_pD->m_columns.Num() );
		check( column < (int)m_pD->m_rows[row].m_values.Num() );
		check( m_pD->m_columns[column].m_type == TCT_MESH );

		m_pD->m_rows[ row ].m_values[column].m_pMesh = pMesh;
	}


	//---------------------------------------------------------------------------------------------
    void Table::SetCell( int column, uint32_t rowId, const char* strValue )
	{
		int row = m_pD->FindRow(rowId);
		check( row>=0 );
		check( column < (int)m_pD->m_columns.Num() );
		check( column < (int)m_pD->m_rows[row].m_values.Num() );
		check( m_pD->m_columns[column].m_type == TCT_STRING );

		m_pD->m_rows[ row ].m_values[column].m_string = strValue;
	}


}

