// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeLayout.h"
#include "MuT/Table.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{


	class NodeMeshTable::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_parameterName;
		TablePtr m_pTable;
		string m_columnName;

		TArray<NodeLayoutPtr> m_layouts;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 1;
			arch << ver;

			arch << m_parameterName;
			arch << m_pTable;
			arch << m_columnName;
			arch << m_layouts;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver==1);

			arch >> m_parameterName;
			arch >> m_pTable;
			arch >> m_columnName;
			arch >> m_layouts;
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};

}
