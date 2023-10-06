// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Instance.h"

#include "HAL/LowLevelMemTracker.h"
#include "Misc/AssertionMacros.h"
#include "MuR/InstancePrivate.h"
#include "MuR/MutableMath.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	Instance::Instance()
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		m_pD = new Private();
	}


	//---------------------------------------------------------------------------------------------
	Instance::~Instance()
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
        check( m_pD );
		delete m_pD;
		m_pD = 0;
	}


	//---------------------------------------------------------------------------------------------
	Instance::Private* Instance::GetPrivate() const
	{
		return m_pD;
	}


    //---------------------------------------------------------------------------------------------
    InstancePtr Instance::Clone() const
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
        InstancePtr pResult = new Instance();

        *pResult->GetPrivate() = *m_pD;

        return pResult;
    }


	//---------------------------------------------------------------------------------------------
	int32 Instance::GetDataSize() const
	{
		return 16 + sizeof(Private) + m_pD->m_lods.GetAllocatedSize() + m_pD->m_extensionData.GetAllocatedSize();
	}


    //---------------------------------------------------------------------------------------------
    Instance::ID Instance::GetId() const
    {
        return m_pD->m_id;
    }


    //---------------------------------------------------------------------------------------------
    int32 Instance::GetLODCount() const
    {
		return m_pD->m_lods.Num();
    }


	//---------------------------------------------------------------------------------------------
	int32 Instance::GetComponentCount( int32 lod ) const
	{
		check(lod >= 0 && lod < m_pD->m_lods.Num());
		if (lod >= 0 && lod < m_pD->m_lods.Num())
		{
			return m_pD->m_lods[lod].m_components.Num();
		}

		return 0;
	}


    //---------------------------------------------------------------------------------------------
    const char* Instance::GetComponentName( int32 lod, int32 comp ) const
    {
        if ( lod>=0 && lod<m_pD->m_lods.Num() &&
             comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() )
        {
            return m_pD->m_lods[lod].m_components[comp].m_name.c_str();
        }
		else
		{
			check(false);
		}

        return "";
    }

	
	//---------------------------------------------------------------------------------------------
	uint16 Instance::GetComponentId( int32 lod, int32 comp ) const
	{
		if ( lod>=0 && lod<m_pD->m_lods.Num() &&
			 comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() )
		{
			return m_pD->m_lods[lod].m_components[comp].m_id;
		}
		else
		{
			check(false);
		}

		return 0;
	}


    //---------------------------------------------------------------------------------------------
    int32 Instance::GetSurfaceCount( int32 lod, int32 comp ) const
    {
		if (lod >= 0 && lod < m_pD->m_lods.Num() &&
			comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num())
		{
			return m_pD->m_lods[lod].m_components[comp].m_surfaces.Num();
		}
		else
		{
			check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
    const char* Instance::GetSurfaceName( int32 lod, int32 comp, int32 surf ) const
    {
        if ( lod>=0 && lod<m_pD->m_lods.Num() &&
             comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() &&
             surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() )
        {
            return m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_name.c_str();
        }
		else
		{
			check(false);
		}

        return "";
    }


    //---------------------------------------------------------------------------------------------
    uint32 Instance::GetSurfaceId( int32 lod, int32 comp, int32 surf ) const
    {
        if ( lod>=0 && lod<m_pD->m_lods.Num() &&
             comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() &&
             surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() )
        {
            return m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].InternalId;
        }
		else
		{
			check(false);
		}

        return 0;
    }


    //---------------------------------------------------------------------------------------------
    int32 Instance::FindSurfaceById( int32 lod, int32 comp, uint32 id ) const
    {
		if (lod >= 0 && lod < m_pD->m_lods.Num() &&
			comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num())
		{
			for (int32 i = 0; i < m_pD->m_lods[lod].m_components[comp].m_surfaces.Num(); ++i)
			{
				if (m_pD->m_lods[lod].m_components[comp].m_surfaces[i].InternalId == id)
				{
					return i;
				}
			}
		}
		else
		{
			check(false);
		}

        return -1;
    }

	
	//---------------------------------------------------------------------------------------------
	void Instance::FindBaseSurfaceBySharedId(int32 CompIndex, int32 SharedId, int32& OutSurfaceIndex, int32& OutLODIndex) const
	{
		for (int32 LodIndex = 0; LodIndex < m_pD->m_lods.Num(); LodIndex++)
		{
			if (m_pD->m_lods[LodIndex].m_components.IsValidIndex(CompIndex))
			{
				for (int32 SurfaceIndex = 0; SurfaceIndex < m_pD->m_lods[LodIndex].m_components[CompIndex].m_surfaces.Num(); ++SurfaceIndex)
				{
					if (m_pD->m_lods[LodIndex].m_components[CompIndex].m_surfaces[SurfaceIndex].SharedId == SharedId)
					{
						OutSurfaceIndex = SurfaceIndex;
						OutLODIndex = LodIndex;
						return;
					}
				}
			}

		}

		OutSurfaceIndex = INDEX_NONE;
		OutLODIndex = INDEX_NONE;
	}


	//---------------------------------------------------------------------------------------------
	int32 Instance::GetSharedSurfaceId(int32 LodIndex, int32 CompIndex, int32 SurfaceIndex) const
	{
		if (LodIndex >= 0 && LodIndex < m_pD->m_lods.Num() &&
			CompIndex >= 0 && CompIndex < m_pD->m_lods[LodIndex].m_components.Num() &&
			SurfaceIndex >= 0 && SurfaceIndex < m_pD->m_lods[LodIndex].m_components[CompIndex].m_surfaces.Num())
		{
			return m_pD->m_lods[LodIndex].m_components[CompIndex].m_surfaces[SurfaceIndex].SharedId;
		}
		else
		{
			check(false);
		}

		return 0;
	}
	

    //---------------------------------------------------------------------------------------------
    uint32 Instance::GetSurfaceCustomId( int32 lod, int32 comp, int32 surf ) const
    {
        if ( lod>=0 && lod<m_pD->m_lods.Num() &&
             comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() &&
             surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() )
        {
            return m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].ExternalId;
        }
		else
		{
			check(false);
		}

        return 0;
    }


	//---------------------------------------------------------------------------------------------
    int32 Instance::GetMeshCount( int32 lod, int32 comp ) const
	{
		check(lod >= 0 && lod < m_pD->m_lods.Num());
		check(comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num());

		return m_pD->m_lods[lod].m_components[comp].m_meshes.Num();
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::GetImageCount( int32 lod, int32 comp, int32 surf ) const
	{
		check(lod >= 0 && lod < m_pD->m_lods.Num());
		check(comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num());
		check(surf >= 0 && surf < m_pD->m_lods[lod].m_components[comp].m_surfaces.Num());

		return m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_images.Num();
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::GetVectorCount( int32 lod, int32 comp, int32 surf ) const
	{
		check(lod >= 0 && lod < m_pD->m_lods.Num());
		check(comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num());
		check(surf >= 0 && surf < m_pD->m_lods[lod].m_components[comp].m_surfaces.Num());

		return m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_vectors.Num();
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::GetScalarCount( int32 lod, int32 comp, int32 surf ) const
	{
		check(lod >= 0 && lod < m_pD->m_lods.Num());
		check(comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num());
		check(surf >= 0 && surf < m_pD->m_lods[lod].m_components[comp].m_surfaces.Num());

		return m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_scalars.Num();
	}


    //---------------------------------------------------------------------------------------------
    int32 Instance::GetStringCount( int32 lod, int32 comp, int32 surf ) const
    {
		check(lod >= 0 && lod < m_pD->m_lods.Num());
		check(comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num());
		check(surf >= 0 && surf < m_pD->m_lods[lod].m_components[comp].m_surfaces.Num());

		return m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_strings.Num();
	}



    //---------------------------------------------------------------------------------------------
	FResourceID Instance::GetMeshId( int32 lod, int32 comp, int32 mesh ) const
    {
        check( lod>=0 && lod<m_pD->m_lods.Num() );
        check( comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() );
        check( mesh>=0 && mesh<m_pD->m_lods[lod].m_components[comp].m_meshes.Num() );

		FResourceID result = m_pD->m_lods[lod].m_components[comp].m_meshes[mesh].m_meshId;
        return result;
    }


	//---------------------------------------------------------------------------------------------
	FResourceID Instance::GetImageId( int32 lod, int32 comp, int32 surf, int32 img ) const
	{
		check( lod>=0 && lod<m_pD->m_lods.Num() );
		check( comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() );
        check( surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() );
        check( img>=0 && img<m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_images.Num() );

		FResourceID result = m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_images[img].m_imageId;
        return result;
	}


	//---------------------------------------------------------------------------------------------
    const char* Instance::GetImageName( int32 lod, int32 comp, int32 surf, int32 img ) const
	{
		check( lod>=0 && lod<m_pD->m_lods.Num() );
		check( comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() );
        check( surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() );
        check( img>=0 && img<m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_images.Num() );

        const char* strResult = m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_images[img].m_name.c_str();
		return strResult;
	}


	//---------------------------------------------------------------------------------------------
	FVector4f Instance::GetVector( int32 lod, int32 comp, int32 surf, int32 vec ) const
	{
		check( lod>=0 && lod<m_pD->m_lods.Num() );
		check( comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() );
        check( surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() );
        check( vec>=0 && vec<m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_vectors.Num() );

        return m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_vectors[vec].m_vec;
	}


	//---------------------------------------------------------------------------------------------
    const char* Instance::GetVectorName( int32 lod, int32 comp, int32 surf, int32 vec ) const
	{
		check( lod>=0 && lod<m_pD->m_lods.Num() );
		check( comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() );
        check( surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() );
        check( vec>=0 && vec<m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_vectors.Num() );

        const char* strResult = m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_vectors[vec].m_name.c_str();
		return strResult;
	}


	//---------------------------------------------------------------------------------------------
    float Instance::GetScalar( int32 lod, int32 comp, int32 surf, int32 sca ) const
	{
		check( lod>=0 && lod<m_pD->m_lods.Num() );
		check( comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() );
        check( surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() );
        check( sca>=0 && sca<m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_scalars.Num() );

        float result = m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_scalars[sca].m_scalar;
		return result;
	}


	//---------------------------------------------------------------------------------------------
    const char* Instance::GetScalarName( int32 lod, int32 comp, int32 surf, int32 sca ) const
	{
		check( lod>=0 && lod<m_pD->m_lods.Num() );
		check( comp>=0 && comp<m_pD->m_lods[lod].m_components.Num() );
        check( surf>=0 && surf<m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() );
        check( sca>=0 && sca<m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_scalars.Num() );

        const char* strResult = m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_scalars[sca].m_name.c_str();
		return strResult;
	}


    //---------------------------------------------------------------------------------------------
    const char* Instance::GetString( int32 lod, int32 comp, int32 surf, int32 str ) const
    {
        check( lod >= 0 && lod < m_pD->m_lods.Num() );
        check( comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num() );
        check( surf >= 0 &&
                        surf < m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() );

        bool valid =
            str >= 0 &&
            str < m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_strings.Num();
        check(valid);

        if (valid)
        {
            const char* result = m_pD->m_lods[lod]
                                     .m_components[comp]
                                     .m_surfaces[surf]
                                     .m_strings[str]
                                     .m_string.c_str();
            return result;
        }

        return "";
    }


    //---------------------------------------------------------------------------------------------
    const char* Instance::GetStringName( int32 lod, int32 comp, int32 surf, int32 str ) const
    {
        check( lod >= 0 && lod < m_pD->m_lods.Num() );
        check( comp >= 0 && comp < m_pD->m_lods[lod].m_components.Num() );
        check( surf >= 0 &&
                        surf < m_pD->m_lods[lod].m_components[comp].m_surfaces.Num() );
        bool valid =
            str >= 0 &&
            str < m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_strings.Num();
        check( valid );

        if (valid)
        {
            const char* strResult =
                m_pD->m_lods[lod].m_components[comp].m_surfaces[surf].m_strings[str].m_name.c_str();
            return strResult;
        }

        return "";
    }


    //---------------------------------------------------------------------------------------------
	int32 Instance::GetExtensionDataCount() const
	{
		return m_pD->m_extensionData.Num();
	}


    //---------------------------------------------------------------------------------------------
	void Instance::GetExtensionData(int32 Index, ExtensionDataPtrConst& OutExtensionData, const char*& OutName) const
	{
		check(m_pD->m_extensionData.IsValidIndex(Index));

		OutExtensionData = m_pD->m_extensionData[Index].Data;
		OutName = m_pD->m_extensionData[Index].Name.c_str();
	}


    //---------------------------------------------------------------------------------------------
	int32 Instance::Private::AddLOD()
	{
		int32 result = m_lods.Num();
		m_lods.Add( INSTANCE_LOD() );
		return result;
	}


    //---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddComponent( int32 lod )
    {
        // Automatically create the necessary lods and components
        while (lod >= m_lods.Num())
        {
            AddLOD();
        }

        int32 result = m_lods[lod].m_components.Num();
        m_lods[lod].m_components.Add( INSTANCE_COMPONENT() );
        return result;
    }


    //---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddSurface( int32 lod, int32 comp )
    {
        // Automatically create the necessary lods and components
        while (lod >= m_lods.Num())
        {
            AddLOD();
        }
        while (comp >= m_lods[lod].m_components.Num())
        {
            AddComponent(lod);
        }

        int32 result = m_lods[lod].m_components[comp].m_surfaces.Num();
        m_lods[lod].m_components[comp].m_surfaces.Add( INSTANCE_SURFACE() );
        return result;
    }


    //---------------------------------------------------------------------------------------------
    void Instance::Private::SetComponentName( int32 lod, int32 comp, const char* strName )
    {
        // Automatically create the necessary lods and components
        while (lod >= m_lods.Num())
        {
            AddLOD();
        }
        while (comp >= m_lods[lod].m_components.Num())
        {
            AddComponent( lod );
        }

        INSTANCE_COMPONENT& component = m_lods[lod].m_components[comp];
        if (strName)
        {
            component.m_name = strName;
        }
        else
        {
            component.m_name = "";
        }
    }


    //---------------------------------------------------------------------------------------------
    void Instance::Private::SetSurfaceName( int32 lod, int32 comp, int32 surf, const char* strName )
    {
        // Automatically create the necessary lods and components
        while ( lod>=m_lods.Num() )
        {
            AddLOD();
        }
        while (comp >= m_lods[lod].m_components.Num())
        {
            AddComponent( lod );
        }
        while ( surf>=m_lods[lod].m_components[comp].m_surfaces.Num() )
        {
            AddSurface( lod, comp );
        }

        INSTANCE_SURFACE& surface = m_lods[lod].m_components[comp].m_surfaces[surf];
        if (strName)
        {
            surface.m_name = strName;
        }
        else
        {
            surface.m_name = "";
        }
    }


	//---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddMesh( int32 lod, int32 comp, FResourceID meshId, const char* strName )
	{
		// Automatically create the necessary lods and components
		while (lod >= m_lods.Num())
		{
			AddLOD();
		}
		while (comp >= m_lods[lod].m_components.Num())
		{
			AddComponent( lod );
		}

		INSTANCE_COMPONENT& component = m_lods[lod].m_components[comp];
        int32 result = component.m_meshes.Num();
        component.m_meshes.Emplace( meshId, strName );

		return result;
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddImage( int32 lod, int32 comp, int32 surf, FResourceID imageId, const char* strName )
	{
		// Automatically create the necessary lods and components
		while (lod >= m_lods.Num())
		{
			AddLOD();
		}
		while (comp >= m_lods[lod].m_components.Num())
		{
			AddComponent(lod);
		}
		while (surf >= m_lods[lod].m_components[comp].m_surfaces.Num())
		{
			AddSurface(lod, comp);
		}

        INSTANCE_SURFACE& surface = m_lods[lod].m_components[comp].m_surfaces[surf];
        int32 result = surface.m_images.Num();
        surface.m_images.Emplace( imageId, strName );

        return result;
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddVector( int32 lod, int32 comp, int32 surf, const FVector4f& vec, const char* strName )
	{
		// Automatically create the necessary lods and components
		while (lod >= m_lods.Num())
		{
			AddLOD();
		}
		while (comp >= m_lods[lod].m_components.Num())
		{
			AddComponent(lod);
		}
		while (surf >= m_lods[lod].m_components[comp].m_surfaces.Num())
		{
			AddSurface(lod, comp);
		}

        INSTANCE_SURFACE& surface = m_lods[lod].m_components[comp].m_surfaces[surf];
        int32 result = surface.m_vectors.Num();
        surface.m_vectors.Emplace( vec, strName );

        return result;
	}


    //---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddScalar( int32 lod, int32 comp, int32 surf, float sca, const char* strName )
    {
        // Automatically create the necessary lods and components
		while (lod >= m_lods.Num())
		{
			AddLOD();
		}
		while (comp >= m_lods[lod].m_components.Num())
		{
			AddComponent(lod);
		}
		while (surf >= m_lods[lod].m_components[comp].m_surfaces.Num())
		{
			AddSurface(lod, comp);
		}

        INSTANCE_SURFACE& surface = m_lods[lod].m_components[comp].m_surfaces[surf];
        int32 result = surface.m_scalars.Num();
        surface.m_scalars.Emplace( sca, strName );

        return result;
    }


    //---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddString( int32 lod, int32 comp, int32 surf, const char* str, const char* strName )
    {
        // Automatically create the necessary lods and components
		while (lod >= m_lods.Num())
		{
			AddLOD();
		}
		while (comp >= m_lods[lod].m_components.Num())
		{
			AddComponent(lod);
		}
		while (surf >= m_lods[lod].m_components[comp].m_surfaces.Num())
		{
			AddSurface(lod, comp);
		}

        INSTANCE_SURFACE& surface = m_lods[lod].m_components[comp].m_surfaces[surf];
        int32 result = surface.m_strings.Num();
        surface.m_strings.Emplace( str, strName );

        return result;
    }


    //---------------------------------------------------------------------------------------------
	void Instance::Private::AddExtensionData(ExtensionDataPtrConst Data, const char* Name)
	{
		check(Data);
		check(Name);

		NamedExtensionData& Entry = m_extensionData.AddDefaulted_GetRef();
		Entry.Data = Data;
		Entry.Name = Name;

		check(Entry.Name.length() > 0);
	}
}

