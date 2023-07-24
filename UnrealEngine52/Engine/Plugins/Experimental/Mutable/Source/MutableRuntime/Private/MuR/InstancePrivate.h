// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Instance.h"

#include "MuR/ImagePrivate.h"
#include "MuR/MeshPrivate.h"


namespace mu
{

    struct INSTANCE_SURFACE
	{
		string m_name;
        uint32_t m_internalID=0;
        uint32_t m_customID=0;

		struct IMAGE
		{
            IMAGE( RESOURCE_ID p, const char* strName )
			{
                m_imageId = p;
				if (strName)
				{
					m_name = strName;
				}
			}

            RESOURCE_ID m_imageId;
			string m_name;
		};

		TArray<IMAGE, TInlineAllocator<4>> m_images;

		struct VECTOR
		{
			VECTOR( const FVector4f& v, const char* strName )
			{
				m_vec = v;
				if (strName)
				{
					m_name = strName;
				}
			}

			FVector4f m_vec;
			string m_name;
		};

		TArray<VECTOR> m_vectors;

        struct SCALAR
        {
            SCALAR( float v, const char* strName )
            {
                m_scalar = v;
                if ( strName )
                {
                    m_name = strName;
                }
            }

            float m_scalar;
            string m_name;
        };

		TArray<SCALAR> m_scalars;

        struct STRING
        {
            STRING( const char* strValue, const char* strName )
            {
                m_string = strValue ? strValue : "";
                if ( strName )
                {
                    m_name = strName;
                }
            }

            string m_string;
            string m_name;
        };

		TArray<STRING> m_strings;
    };


    struct INSTANCE_COMPONENT
    {
        string m_name;

    	uint16 m_id;

		struct MESH
		{
            MESH(RESOURCE_ID p, const char* strName)
			{
                m_meshId = p;
				if (strName)
				{
					m_name = strName;
				}
			}

            RESOURCE_ID m_meshId;
			string m_name;
		};
		TArray<MESH, TInlineAllocator<2>> m_meshes;

		// The order must match the meshes surfaces
		TArray<INSTANCE_SURFACE, TInlineAllocator<4>> m_surfaces;
	};


    struct INSTANCE_LOD
    {
		TArray<INSTANCE_COMPONENT, TInlineAllocator<4>> m_components;
    };


	class Instance::Private : public Base
	{
	public:

        //!
        Instance::ID m_id;

		//!
		TArray<INSTANCE_LOD,TInlineAllocator<4>> m_lods;

        Private()
        {
            m_id = 0;
        }

		int GetLODCount() const;
        int GetComponentCount( int lod ) const;
        int GetSurfaceCount( int lod, int comp ) const;
        int GetMeshCount( int lod, int comp ) const;
        int GetImageCount( int lod, int comp, int surf ) const;
        int GetVectorCount( int lod, int comp, int surf ) const;
        int GetScalarCount( int lod, int comp, int surf ) const;
        int GetStringCount( int lod, int comp, int surf ) const;
        int AddLOD();
        int AddComponent( int lod );
        void SetComponentName( int lod, int comp, const char* strName );
        int AddMesh(int lod, int comp, RESOURCE_ID, const char* strName);
		int AddSurface( int lod, int comp );
        void SetSurfaceName( int lod, int comp, int surf, const char* strName );
        int AddImage( int lod, int comp, int surf, RESOURCE_ID, const char* strName );
        int AddVector( int lod, int comp, int surf, const FVector4f&, const char* strName );
        int AddScalar( int lod, int comp, int surf, float, const char* strName );
        int AddString( int lod, int comp, int surf, const char* strValue, const char* strName );
    };
}
