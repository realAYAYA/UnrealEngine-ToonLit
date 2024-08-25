// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeSurfaceNew.h"

#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeString.h"
#include "MuT/NodeColour.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceNew::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		FString m_name;
        uint32 ExternalId =0;
        int32 SharedSurfaceId =INDEX_NONE;

		struct MESH
		{
			FString m_name;
			NodeMeshPtr m_pMesh;

			//!
			void Serialise( OutputArchive& arch ) const
			{
				arch << m_name;
				arch << m_pMesh;
			}

			void Unserialise( InputArchive& arch )
			{
				arch >> m_name;
				arch >> m_pMesh;
			}
		};

		TArray<MESH> m_meshes;

		struct IMAGE
		{
			FString m_name;
			FString m_materialName;
			FString m_materialParameterName;
			NodeImagePtr m_pImage;

			// It could be negative, to indicate no layout.
            int8 m_layoutIndex = 0;

            //!
			void Serialise(OutputArchive& arch) const
			{
				arch << m_name;
				arch << m_materialName;
				arch << m_materialParameterName;
				arch << m_pImage;
                arch << m_layoutIndex;
            }

			void Unserialise(InputArchive& arch)
			{
				arch >> m_name;
				arch >> m_materialName;
				arch >> m_materialParameterName;
				arch >> m_pImage;
                arch >> m_layoutIndex;
            }
        };

		TArray<IMAGE> m_images;

		struct VECTOR
		{
			FString m_name;
			NodeColourPtr m_pVector;

			//!
			void Serialise(OutputArchive& arch) const
			{
				arch << m_name;
				arch << m_pVector;
			}

			void Unserialise(InputArchive& arch)
			{
				arch >> m_name;
				arch >> m_pVector;
			}
		};

		TArray<VECTOR> m_vectors;

        struct SCALAR
        {
			FString m_name;
            NodeScalarPtr m_pScalar;

            //!
            void Serialise(OutputArchive& arch) const
            {
                arch << m_name;
                arch << m_pScalar;
            }

            void Unserialise(InputArchive& arch)
            {
                arch >> m_name;
                arch >> m_pScalar;
            }
        };

        TArray<SCALAR> m_scalars;

        struct STRING
        {
			FString m_name;
            NodeStringPtr m_pString;

            //!
            void Serialise( OutputArchive& arch ) const
            {
                arch << m_name;
                arch << m_pString;
            }

            void Unserialise( InputArchive& arch )
            {
                arch >> m_name;
                arch >> m_pString;
            }
        };

		TArray<STRING> m_strings;

        //! Tags in this surface
		TArray<FString> m_tags;

		//! Find an image node index by name or return -1
		int FindImage( const FString& strName ) const;

		//! Find a mesh node index by name or return -1
		int FindMesh(const FString& strName) const;

        //! Find a vector node index by name or return -1
        int FindVector(const FString& strName) const;

        //! Find a scalar node index by name or return -1
        int FindScalar( const FString& strName ) const;

        //! Find a string node index by name or return -1
        int FindString( const FString& strName ) const;

        //!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 9;
			arch << ver;

			arch << m_name;
            arch << ExternalId;
			arch << SharedSurfaceId;

			arch << m_meshes;
			arch << m_images;
			arch << m_tags;
            arch << m_vectors;
            arch << m_scalars;
            arch << m_strings;
        }

        //!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver<=9);

			std::string Temp;
			if (ver <= 8)
			{
				arch >> Temp;
				m_name = Temp.c_str();
			}
			else
			{
				arch >> m_name;
			}
            arch >> ExternalId;

			if (ver >= 8)
			{
				arch >> SharedSurfaceId;
			}

			if (ver <= 8)
			{
				int32 Num = 0;

				arch >> Num;
				m_meshes.SetNum(Num);
				for (int32 i=0; i<Num; ++i)
				{
					arch >> Temp;
					m_meshes[i].m_name = Temp.c_str();
					arch >> m_meshes[i].m_pMesh;
				}

				arch >> Num;
				m_images.SetNum(Num);
				for (int32 i = 0; i < Num; ++i)
				{
					arch >> Temp;
					m_images[i].m_name = Temp.c_str();

					arch >> Temp;
					m_images[i].m_materialName = Temp.c_str();

					arch >> Temp;
					m_images[i].m_materialParameterName = Temp.c_str();

					arch >> m_images[i].m_pImage;
					arch >> m_images[i].m_layoutIndex;
				}

				arch >> Num;
				m_tags.SetNum(Num);
				for (int32 i = 0; i < Num; ++i)
				{
					arch >> Temp;
					m_tags[i] = Temp.c_str();
				}

				arch >> Num;
				m_vectors.SetNum(Num);
				for (int32 i = 0; i < Num; ++i)
				{
					arch >> Temp;
					m_vectors[i].m_name = Temp.c_str();
					arch >> m_vectors[i].m_pVector;
				}

				arch >> Num;
				m_scalars.SetNum(Num);
				for (int32 i = 0; i < Num; ++i)
				{
					arch >> Temp;
					m_scalars[i].m_name = Temp.c_str();
					arch >> m_scalars[i].m_pScalar;
				}

				arch >> Num;
				m_strings.SetNum(Num);
				for (int32 i = 0; i < Num; ++i)
				{
					arch >> Temp;
					m_strings[i].m_name = Temp.c_str();
					arch >> m_strings[i].m_pString;
				}
			}
			else
			{
				arch >> m_meshes;
				arch >> m_images;
				arch >> m_tags;
				arch >> m_vectors;
				arch >> m_scalars;
				arch >> m_strings;
			}
        }
    };




}

