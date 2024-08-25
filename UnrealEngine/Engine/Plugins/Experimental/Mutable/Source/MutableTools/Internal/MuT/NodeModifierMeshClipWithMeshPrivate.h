// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeMesh.h"
#include "MuT/AST.h"

#include "MuR/MutableMath.h"


namespace mu
{

    class NodeModifierMeshClipWithMesh::Private : public NodeModifier::Private
	{
	public:

		Private()
		{
		}

		static FNodeType s_type;

		//! 
		Ptr<NodeMesh> ClipMesh;

		//!
		void Serialise( OutputArchive& arch ) const
		{
			NodeModifier::Private::Serialise(arch);

            uint32_t ver = 0;
			arch << ver;

			arch << ClipMesh;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
			NodeModifier::Private::Unserialise( arch );
			
            uint32_t ver;
			arch >> ver;
            check(ver<=0);

			arch >> ClipMesh;
		}

	};


}
