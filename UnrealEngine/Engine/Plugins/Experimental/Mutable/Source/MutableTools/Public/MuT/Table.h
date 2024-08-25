// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"


namespace mu
{
	// Forward declarations
	class Table;
	typedef Ptr<Table> TablePtr;
	typedef Ptr<const Table> TablePtrConst;

	class Mesh;
	class Image;
	class InputArchive;
	class OutputArchive;

	/** Types of the values for the table cells. */
	enum class ETableColumnType : uint32
	{
		None,
		Scalar,
		Color,
		Image,
		Mesh,
		String
	};


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
		static Ptr<Table> StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//!
		void SetName(const FString&);
		const FString& GetName() const;

		//!
		int32 AddColumn(const FString&, ETableColumnType );

		//! Return the column index with the given name. -1 if not found.
		int32 FindColumn(const FString&) const;

		//!
        void AddRow( uint32 id );

		//!
        void SetCell( int32 Column, uint32 RowId, float Value, const void* ErrorContext = nullptr);
        void SetCell( int32 Column, uint32 RowId, const FVector4f& Value, const void* ErrorContext = nullptr);
		void SetCell( int32 Column, uint32 RowId, ResourceProxy<Image>* Value, const void* ErrorContext = nullptr);
		void SetCell( int32 Column, uint32 RowId, Mesh* Value, const void* ErrorContext = nullptr);
        void SetCell( int32 Column, uint32 RowId, const FString& Value, const void* ErrorContext = nullptr);


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
