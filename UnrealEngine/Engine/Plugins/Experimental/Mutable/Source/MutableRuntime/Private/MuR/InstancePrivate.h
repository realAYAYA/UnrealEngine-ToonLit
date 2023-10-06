// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Instance.h"

#include "MuR/ImagePrivate.h"
#include "MuR/MeshPrivate.h"


namespace mu
{

	/** Helper functions to make and read FResourceIDs */
	inline FResourceID MakeResourceID(uint32 RootAddress, uint32 ParameterBlobIndex)
	{
		return (uint64(RootAddress) << 32) | uint64(ParameterBlobIndex);
	}

	inline uint32 GetResourceIDRoot(FResourceID Id)
	{
		return uint32(Id >> 32);
	}

	/** */
    struct INSTANCE_SURFACE
	{
		string m_name;
        uint32 InternalId=0;
        uint32 ExternalId =0;
        uint32 SharedId =0;

		struct IMAGE
		{
            IMAGE(FResourceID p, const char* strName )
			{
                m_imageId = p;
				if (strName)
				{
					m_name = strName;
				}
			}

			FResourceID m_imageId;
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
            MESH(FResourceID p, const char* strName)
			{
                m_meshId = p;
				if (strName)
				{
					m_name = strName;
				}
			}

			FResourceID m_meshId;
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

	struct NamedExtensionData
	{
		ExtensionDataPtrConst Data;
		string Name;
	};

	class Instance::Private : public Base
	{
	public:

        //!
        Instance::ID m_id;

		//!
		TArray<INSTANCE_LOD,TInlineAllocator<4>> m_lods;

		// Every entry must have a valid ExtensionData and name
		TArray<NamedExtensionData> m_extensionData;

        Private()
        {
            m_id = 0;
        }

        int AddLOD();
        int AddComponent( int lod );
        void SetComponentName( int32 lod, int32 comp, const char* strName );
        int AddMesh(int32 lod, int32 comp, FResourceID, const char* strName);
		int AddSurface( int lod, int comp );
        void SetSurfaceName( int32 lod, int32 comp, int32 surf, const char* strName );
        int AddImage( int32 lod, int32 comp, int32 surf, FResourceID, const char* strName );
        int32 AddVector( int32 lod, int32 comp, int32 surf, const FVector4f&, const char* strName );
        int32 AddScalar( int32 lod, int32 comp, int32 surf, float, const char* strName );
        int32 AddString( int32 lod, int32 comp, int32 surf, const char* strValue, const char* strName );
		
		// Data and Name must be non-null
		void AddExtensionData(ExtensionDataPtrConst Data, const char* Name);
    };
}
