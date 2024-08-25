// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshClipWithMesh.h"
#include "MuT/AST.h"

#include "MuR/MutableMath.h"


namespace mu
{


    class NodeMeshClipWithMesh::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		NodeMeshPtr m_pSource;
		NodeMeshPtr m_pClipMesh;

		TArray<FString> m_tags;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 2;
			arch << ver;

			arch << m_pSource;
			arch << m_pClipMesh;
			arch << m_tags;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver<=2)

			arch >> m_pSource;
			arch >> m_pClipMesh;

			if (ver <= 1)
			{
				TArray<std::string> Temp;
				arch >> Temp;
				m_tags.SetNum(Temp.Num());
				for (int32 i = 0; i < Temp.Num(); ++i)
				{
					m_tags[i] = Temp[i].c_str();
				}
			}
			else
			{
				arch >> m_tags;
			}
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};


}
