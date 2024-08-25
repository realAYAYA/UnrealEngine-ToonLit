// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
	class NodeMeshTable;
	typedef Ptr<NodeMeshTable> NodeMeshTablePtr;
	typedef Ptr<const NodeMeshTable> NodeMeshTablePtrConst;

	class NodeLayout;
	typedef Ptr<NodeLayout> NodeLayoutPtr;
	typedef Ptr<const NodeLayout> NodeLayoutPtrConst;

	class Table;
	typedef Ptr<Table> TablePtr;
	typedef Ptr<const Table> TablePtrConst;


	//! This node provides the meshes stored in the column of a table.
	//! \ingroup transform
	class MUTABLETOOLS_API NodeMeshTable : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshTable();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshTable* pNode, OutputArchive& arch );
		static NodeMeshTablePtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Set the name of the implicit table parameter.
		void SetParameterName( const FString& strName );

		//!
		TablePtr GetTable() const;
		void SetTable( TablePtr );

		//!
		void SetColumn( const FString& strName );

		//! Get the number of layouts defined in the meshes on this column.
		int GetLayoutCount() const;
		void SetLayoutCount( int );

		//! Get the node defining a layout of the meshes on this column.
		NodeLayoutPtr GetLayout( int index ) const;
		void SetLayout( int index, NodeLayoutPtr );

		//! Adds the "None" option to the parameter that represents this table column
		void SetNoneOption(bool bAddOption);

		//! Set the row name to be used as default value
		void SetDefaultRowName(const FString& RowName);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;


	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshTable();

	private:

		Private* m_pD;

	};


}
