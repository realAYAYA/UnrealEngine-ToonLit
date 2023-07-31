// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeMesh.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"

namespace mu
{


    class NodeModifierMeshClipDeform::Private : public NodeModifier::Private
    {
    public:

        MUTABLE_DEFINE_CONST_VISITABLE()

    public:

		Private()
		{
		}

		static NODE_TYPE s_type;

		//! 
		NodeMeshPtr ClipMesh;
		EShapeBindingMethod BindingMethod = EShapeBindingMethod::ClipDeformClosestProject;

		//!
		void Serialise( OutputArchive& arch ) const
		{
			NodeModifier::Private::Serialise(arch);

            uint32_t ver = 1;
			arch << ver;

			arch << ClipMesh;
			arch << BindingMethod;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
			NodeModifier::Private::Unserialise( arch );
			
            uint32_t ver;
			arch >> ver;
            check(ver<=1);

			arch >> ClipMesh;
			if (ver >= 1)
			{
				arch >> BindingMethod;
			}
			else
			{
				BindingMethod = EShapeBindingMethod::ClipDeformClosestProject;
			}
	
		}
    };

}
