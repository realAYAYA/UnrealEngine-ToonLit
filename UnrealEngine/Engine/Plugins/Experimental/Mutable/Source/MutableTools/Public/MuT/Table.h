// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"


namespace mu
{
	// Forward declarations
	class Table;
	typedef Ptr<Table> TablePtr;
	typedef Ptr<const Table> TablePtrConst;

	class Mesh;
	typedef Ptr<Mesh> MeshPtr;
	typedef Ptr<const Mesh> MeshPtrConst;

	class Image;
	typedef Ptr<Image> ImagePtr;
	typedef Ptr<const Image> ImagePtrConst;

	class InputArchive;
	class OutputArchive;

	//! Types of the values for the table cells.
	//! \ingroup runtime
	typedef enum
	{
		TCT_NONE,
		TCT_SCALAR,
		TCT_COLOUR,
		TCT_IMAGE,
		TCT_MESH,
		TCT_STRING,
		TCT_COUNT,

		_TCT_FORCE32BITS = 0xFFFFFFFF
	} TABLE_COLUMN_TYPE;


	//! A table that contains many rows and defines attributes like meshes, images,
	//! colours, etc. for every column. It is useful to define a big number of similarly structured
	//! objects, by using the NodeDatabase in a model expression.
	//! \ingroup model
	class MUTABLETOOLS_API Table : public RefCounted
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle.
		//-----------------------------------------------------------------------------------------
		Table();

		static void Serialise( const Table* p, OutputArchive& arch );
		static TablePtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//!
		void SetName( const char* strName );
		const char* GetName() const;

		//!
		int AddColumn( const char* strName, TABLE_COLUMN_TYPE type );

		//! Return the column index with the given name. -1 if not found.
		int FindColumn( const char* strName ) const;

		//!
        void AddRow( uint32 id );

		//! Adds a "NONE" option
		void SetNoneOption(bool bAddOption);

		//!
        void SetCell( int column, uint32 rowId, float value );
        void SetCell( int column, uint32 rowId, float r, float g, float b );
        void SetCell( int column, uint32 rowId, Image* pImage );
        void SetCell( int column, uint32 rowId, Mesh* pMesh );
        void SetCell( int column, uint32 rowId, const char* strValue );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~Table();

	private:

		Private* m_pD;

	};

}
