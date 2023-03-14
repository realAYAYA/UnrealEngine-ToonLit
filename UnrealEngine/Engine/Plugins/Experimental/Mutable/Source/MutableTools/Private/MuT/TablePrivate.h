// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Table.h"

#include "MuT/Visitor.h"
#include "MuT/ErrorLogPrivate.h"

#include "MuR/MutableMath.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/SerialisationPrivate.h"


namespace mu
{

	MUTABLE_DEFINE_ENUM_SERIALISABLE( TABLE_COLUMN_TYPE )


	struct TABLE_COLUMN
	{
		string m_name;

		TABLE_COLUMN_TYPE m_type;
	};


	struct TABLE_VALUE
	{
		// TODO: Union
		float m_scalar;
		vec3<float> m_colour;
		Ptr<ResourceProxy<Image>> m_pProxyImage;
		MeshPtr m_pMesh;
		string m_string;
	};


	struct TABLE_ROW
	{
        uint32_t m_id;
		TArray<TABLE_VALUE> m_values;
	};


	//!
	class Table::Private : public Base
	{
	public:

		string m_name;
		TArray<TABLE_COLUMN> m_columns;
		TArray<TABLE_ROW> m_rows;
		bool m_NoneOption = false;


		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

            arch << (uint32_t)m_columns.Num();
			for ( int32 c=0; c<m_columns.Num(); ++c )
			{
				arch << m_columns[c].m_name;
				arch << m_columns[c].m_type;
			}

            arch << (uint32_t)m_rows.Num();
			for (int32 r=0; r<m_rows.Num(); ++r )
			{
				arch << m_rows[r].m_id;

				for (int32 c=0; c<m_columns.Num(); ++c )
				{
					const TABLE_VALUE& v = m_rows[r].m_values[c];

					switch (m_columns[c].m_type)
					{
					case TCT_SCALAR: 	arch << v.m_scalar; break;
					case TCT_COLOUR: 	arch << v.m_colour; break;
					case TCT_MESH: 		arch << v.m_pMesh; break;
					case TCT_IMAGE:		
					{
						Ptr<const Image> image;
						if (v.m_pProxyImage)
						{
							image = v.m_pProxyImage->Get();
						}
						arch << image;
						break;
					}
					case TCT_STRING:	arch << v.m_string; break;
					default: check( false);
					}
				}
			}

			arch << m_NoneOption;

		}


		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver<=1);

            uint32_t columnCount;
			arch >> columnCount;
			m_columns.SetNum( columnCount );
            for (uint32_t c=0; c<columnCount; ++c )
			{
				arch >> m_columns[c].m_name;
				arch >> m_columns[c].m_type;
			}

            uint32_t rowCount;
			arch >> rowCount;
			m_rows.SetNum( rowCount );
            for ( uint32_t r=0; r<rowCount; ++r )
			{
				arch >> m_rows[r].m_id;
				m_rows[r].m_values.SetNum( columnCount );

                for (uint32_t c=0; c<columnCount; ++c )
				{
					TABLE_VALUE& v = m_rows[r].m_values[c];

					switch (m_columns[c].m_type)
					{
					case TCT_SCALAR:	arch >> v.m_scalar; break;
					case TCT_COLOUR:	arch >> v.m_colour; break;
					case TCT_MESH:		arch >> v.m_pMesh; break;
					case TCT_IMAGE:		
					{
						// Are we using proxies?
						v.m_pProxyImage = arch.NewImageProxy();
						if (!v.m_pProxyImage)
						{
							// Normal serialisation
							ImagePtr image;
							arch >> image;
							v.m_pProxyImage = new ResourceProxyMemory<Image>(image.get());
						}
						break;
					}
					case TCT_STRING:	arch >> v.m_string; break;
					default: check( false);
					}
				}
			}

			if (ver >= 1)
			{
				arch >> m_NoneOption;
			}
		}


		//! Find a row in the table by id. Return -1 if not found.
        int FindRow( uint32_t id ) const
		{
			int res = -1;

			for ( std::size_t r=0; res<0 && r<m_rows.Num(); ++r )
			{
				if ( m_rows[r].m_id==id )
				{
					res = (int)r;
				}
			}

			return res;
		}

	};

}
