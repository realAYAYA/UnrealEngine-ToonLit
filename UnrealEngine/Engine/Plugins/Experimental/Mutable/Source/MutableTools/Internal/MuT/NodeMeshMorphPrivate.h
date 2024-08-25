// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshMorph.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/AST.h"

namespace mu
{


	class NodeMeshMorph::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		Ptr<NodeScalar> Factor;
		Ptr<NodeMesh> Base;
		Ptr<NodeMesh> Morph;
		
		bool bReshapeSkeleton = false;
		bool bReshapePhysicsVolumes = false;
		
		TArray<uint16> BonesToDeform;
		TArray<uint16> PhysicsToDeform;

        //!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 7;
			arch << ver;

			arch << Factor;
			arch << Base;
			arch << Morph;

			arch << bReshapeSkeleton;
			arch << bReshapePhysicsVolumes;
			arch << BonesToDeform;
			arch << PhysicsToDeform;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver <= 7);

			arch >> Factor;
			arch >> Base;

			if (ver < 6)
			{
				TArray<Ptr<NodeMesh>> Morphs;
				arch >> Morphs;

				// The result will not be the same.
				if (!Morphs.IsEmpty())
				{
					Morph = Morphs.Last();
				}
			}
			else
			{
				arch >> Morph;
			}

			if (ver < 6)
			{
				bool bDeprecated;
				arch >> bDeprecated;
			}
	
			if (ver >= 2)
			{
				arch >> bReshapeSkeleton;
				arch >> bReshapePhysicsVolumes;
	
				// This repetition is needed, there was a bug where m_reshapePhysicsVolumes was serialized twice in ver 2.
				if (ver == 2) 
				{
					arch >> bReshapePhysicsVolumes;
				}

				if (ver >= 7)
				{
					arch >> BonesToDeform;
				}
				else
				{
					TArray<string> BonesToDeform_DEPRECATED;
					arch >> BonesToDeform_DEPRECATED;

					const int32 NumBonesToDeform = BonesToDeform.Num();
					BonesToDeform.SetNumUninitialized(NumBonesToDeform);
					for (int32 Index = 0; Index < NumBonesToDeform; ++Index)
					{
						BonesToDeform[Index] = Index;
					}
				}
			}
			else
			{
				bReshapeSkeleton = false;
				bReshapePhysicsVolumes = false;
				BonesToDeform.Empty();
			}

			if (ver == 3)
			{
				bool bDeformAllBones_DEPRECATED;
				arch >> bDeformAllBones_DEPRECATED;
			}

			if (ver >= 3 && ver < 5)
			{
				bool bDeformAllPhysics_DEPRECATED;
				arch >> bDeformAllPhysics_DEPRECATED;
			}

			if (ver >= 7)
			{
				arch >> PhysicsToDeform;
			}
			else if (ver >= 3)
			{
				TArray<string> PhysicsToDeform_DEPRECATED;
				arch >> PhysicsToDeform_DEPRECATED;

				const int32 NumPhysicsToDeform = PhysicsToDeform.Num();
				PhysicsToDeform.SetNumUninitialized(NumPhysicsToDeform);
				for (int32 Index = 0; Index < NumPhysicsToDeform; ++Index)
				{
					PhysicsToDeform[Index] = Index;
				}
			}
			else
			{
				PhysicsToDeform.Empty();
			}

		}

		// NodeMesh::Private interface
        Ptr<NodeLayout> GetLayout( int index ) const override;

	};


}
