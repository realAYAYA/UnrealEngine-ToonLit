// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Table.h"

#include "MuT/ErrorLogPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/SerialisationPrivate.h"

#include <string>

namespace mu
{
	MUTABLE_DEFINE_ENUM_SERIALISABLE(ETableColumnType)


	struct FTableColumn
	{
		FString Name;

		ETableColumnType Type;
	};


	struct FTableValue
	{
		// TODO: Union
		float Scalar;
		FVector4f Color;
		Ptr<ResourceProxy<Image>> ProxyImage;
		Ptr<Mesh> Mesh;
		FString String;

		const void* ErrorContext;
	};


	struct FTableRow
	{
        uint32 Id;
		TArray<FTableValue> Values;
	};


	//!
	class Table::Private
	{
	public:

		FString Name;
		TArray<FTableColumn> Columns;
		TArray<FTableRow> Rows;

		// Transient value for serialization compatibility
		bool bNoneOption_DEPRECATED = false;


		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 4;
			arch << ver;

            arch << Columns.Num();
			for ( int32 c=0; c< Columns.Num(); ++c )
			{
				arch << Columns[c].Name;
				arch << Columns[c].Type;
			}

            arch << Rows.Num();
			for (int32 r=0; r< Rows.Num(); ++r )
			{
				arch << Rows[r].Id;

				for (int32 c=0; c<Columns.Num(); ++c )
				{
					const FTableValue& v = Rows[r].Values[c];

					switch (Columns[c].Type)
					{
					case ETableColumnType::Scalar: 	arch << v.Scalar; break;
					case ETableColumnType::Color: 	arch << v.Color; break;
					case ETableColumnType::Mesh: 	arch << v.Mesh; break;
					case ETableColumnType::Image:
					{
						Ptr<const Image> Value;
						if (v.ProxyImage)
						{
							Value = v.ProxyImage->Get();
						}
						arch << Value;
						break;
					}
					case ETableColumnType::String:	arch << v.String; break;
					default: check( false);
					}
				}
			}
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver<=4);

            uint32 ColumnCount;
			arch >> ColumnCount;
			Columns.SetNum(ColumnCount);
            for (uint32 c=0; c<ColumnCount; ++c )
			{
				if (ver <= 2)
				{
					std::string LegacyString;
					arch >> LegacyString;
					Columns[c].Name = LegacyString.c_str();
				}
				else
				{
					arch >> Columns[c].Name;
				}
				arch >> Columns[c].Type;
			}

            uint32 RowCount;
			arch >> RowCount;
			Rows.SetNum( RowCount );
            for ( uint32 r=0; r<RowCount; ++r )
			{
				arch >> Rows[r].Id;
				Rows[r].Values.SetNum( ColumnCount );

                for (uint32_t c=0; c<ColumnCount; ++c )
				{
					FTableValue& v = Rows[r].Values[c];

					switch (Columns[c].Type)
					{
					case ETableColumnType::Scalar:	
					{
						arch >> v.Scalar;
						break;
					}
					case ETableColumnType::Color:
					{
						if (ver <= 1)
						{
							FVector3f Value;
							arch >> Value;

							v.Color = FVector4f(Value[0], Value[1], Value[2], 1.0f);
						}
						else
						{
							arch >> v.Color;
						}
						break;
					}
					case ETableColumnType::Mesh:		
					{
						arch >> v.Mesh;
						break;
					}
					case ETableColumnType::Image:
					{
						// Are we using proxies?
						v.ProxyImage = arch.NewImageProxy();
						if (!v.ProxyImage)
						{
							// Normal serialisation
							Ptr<Image> Value;
							arch >> Value;
							v.ProxyImage = new ResourceProxyMemory<Image>(Value.get());
						}
						break;
					}
					case ETableColumnType::String:
					{
						if (ver <= 2)
						{
							std::string LegacyString;
							arch >> LegacyString;
							v.String = LegacyString.c_str();
						}
						else
						{
							arch >> v.String;
						}
						break;
					}
					default: check( false);
					}
				}
			}

			if (ver >= 1 && ver <= 3)
			{
				arch >> bNoneOption_DEPRECATED;
			}
		}


		//! Find a row in the table by id. Return -1 if not found.
        int32 FindRow( uint32 id ) const
		{
			int32 res = -1;

			for ( int32 r=0; res<0 && r<Rows.Num(); ++r )
			{
				if ( Rows[r].Id==id )
				{
					res = (int32)r;
				}
			}

			return res;
		}

	};

}
