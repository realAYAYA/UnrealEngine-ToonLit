// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"


namespace mu
{

	// Forward definitions
	class NodeColourTable;
	typedef Ptr<NodeColourTable> NodeColourTablePtr;
	typedef Ptr<const NodeColourTable> NodeColourTablePtrConst;

	class Table;
	typedef Ptr<Table> TablePtr;
	typedef Ptr<const Table> TablePtrConst;


	//! This node provides the meshes stored in the column of a table.
	//! \ingroup transform
	class MUTABLETOOLS_API NodeColourTable : public NodeColour
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeColourTable();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeColourTable* pNode, OutputArchive& arch );
		static NodeColourTablePtr StaticUnserialise( InputArchive& arch );


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

		//!
		void SetNoneOption(bool bAddNoneOption);

		//! Set the row name to be used as default value
		void SetDefaultRowName(const FString RowName);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;


	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeColourTable();

	private:

		Private* m_pD;

	};


}
