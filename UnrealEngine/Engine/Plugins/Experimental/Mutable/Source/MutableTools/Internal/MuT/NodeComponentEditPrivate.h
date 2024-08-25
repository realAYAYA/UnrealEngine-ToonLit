// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodeComponentPrivate.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodePatchImagePrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeSurface.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class NodeComponentEdit::Private : public NodeComponent::Private
	{
	public:

		static FNodeType s_type;

        NodeComponentPtr m_pParent;

        TArray<NodeSurfacePtr> m_surfaces;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 2;
			arch << ver;

			arch << m_pParent;
            arch << m_surfaces;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver == 2);

            arch >> m_pParent;
            arch >> m_surfaces;
		}


		// NodeComponent::Private interface
		const NodeComponentNew::Private* GetParentComponentNew() const override;
	};

}

