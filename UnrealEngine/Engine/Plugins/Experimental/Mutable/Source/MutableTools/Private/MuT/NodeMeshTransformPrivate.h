// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshTransform.h"
#include "MuT/AST.h"

#include "MuR/MutableMath.h"
#include "MuR/Serialisation.h"


namespace mu
{


    class NodeMeshTransform::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		NodeMeshPtr Source;
		FMatrix44f Transform;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 0;
			arch << ver;

			arch << Source;
            arch << Transform;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver==0);

			arch >> Source;
            arch >> Transform;
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};


}
