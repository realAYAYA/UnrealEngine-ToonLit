// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once


#include "MuT/NodeComponentPrivate.h"
#include "MuT/NodeComponentNew.h"
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
	class NodeComponentNew::Private : public NodeComponent::Private
	{
	public:

		static FNodeType s_type;

		FString m_name;

		uint16 m_id = 0;

        TArray<NodeSurfacePtr> m_surfaces;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 4;
			
			arch << ver;

			arch << m_name;
			arch << m_id;
            arch << m_surfaces;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver>=2 && ver<=4);

			if (ver <= 3)
			{
				std::string Temp;
				arch>>Temp;
				m_name = Temp.c_str();
			}
			else
			{
				arch >> m_name;
			}

			if (ver >= 3)
			{
				arch >> m_id;
			}

            arch >> m_surfaces;
		}

		// NodeComponent::Private interface
        const NodeComponentNew::Private* GetParentComponentNew() const override
		{
			return this;
		}

	};

}

