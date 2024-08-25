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
		return 16 + sizeof(Private) + m_pD->Lods.GetAllocatedSize() + m_pD->ExtensionData.GetAllocatedSize();
	}


    //---------------------------------------------------------------------------------------------
    Instance::ID Instance::GetId() const
    {
        return m_pD->Id;
    }


    //---------------------------------------------------------------------------------------------
    int32 Instance::GetLODCount() const
    {
		return m_pD->Lods.Num();
    }


	//---------------------------------------------------------------------------------------------
	int32 Instance::GetComponentCount( int32 LODIndex ) const
	{
		check(LODIndex >= 0 && LODIndex < m_pD->Lods.Num());
		if (LODIndex >= 0 && LODIndex < m_pD->Lods.Num())
		{
			return m_pD->Lods[LODIndex].Components.Num();
		}

		return 0;
	}

	
	//---------------------------------------------------------------------------------------------
	uint16 Instance::GetComponentId( int32 LODIndex, int32 ComponentIndex ) const
	{
		if ( LODIndex>=0 && LODIndex<m_pD->Lods.Num() &&
			 ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() )
		{
			return m_pD->Lods[LODIndex].Components[ComponentIndex].Id;
		}
		else
		{
			check(false);
		}

		return 0;
	}


    //---------------------------------------------------------------------------------------------
    int32 Instance::GetSurfaceCount( int32 LODIndex, int32 ComponentIndex ) const
    {
		if (LODIndex >= 0 && LODIndex < m_pD->Lods.Num() &&
			ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num())
		{
			return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num();
		}
		else
		{
			check(false);
		}

		return 0;
	}


    //---------------------------------------------------------------------------------------------
    uint32 Instance::GetSurfaceId( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex ) const
    {
        if ( LODIndex>=0 && LODIndex<m_pD->Lods.Num() &&
             ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() &&
             SurfaceIndex>=0 && SurfaceIndex<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() )
        {
            return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].InternalId;
        }
		else
		{
			check(false);
		}

        return 0;
    }


    //---------------------------------------------------------------------------------------------
    int32 Instance::FindSurfaceById( int32 LODIndex, int32 ComponentIndex, uint32 id ) const
    {
		if (LODIndex >= 0 && LODIndex < m_pD->Lods.Num() &&
			ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num())
		{
			for (int32 i = 0; i < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num(); ++i)
			{
				if (m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[i].InternalId == id)
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
		for (int32 LodIndex = 0; LodIndex < m_pD->Lods.Num(); LodIndex++)
		{
			if (m_pD->Lods[LodIndex].Components.IsValidIndex(CompIndex))
			{
				for (int32 SurfaceIndex = 0; SurfaceIndex < m_pD->Lods[LodIndex].Components[CompIndex].Surfaces.Num(); ++SurfaceIndex)
				{
					if (m_pD->Lods[LodIndex].Components[CompIndex].Surfaces[SurfaceIndex].SharedId == SharedId)
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
		if (LodIndex >= 0 && LodIndex < m_pD->Lods.Num() &&
			CompIndex >= 0 && CompIndex < m_pD->Lods[LodIndex].Components.Num() &&
			SurfaceIndex >= 0 && SurfaceIndex < m_pD->Lods[LodIndex].Components[CompIndex].Surfaces.Num())
		{
			return m_pD->Lods[LodIndex].Components[CompIndex].Surfaces[SurfaceIndex].SharedId;
		}
		else
		{
			check(false);
		}

		return 0;
	}
	

    //---------------------------------------------------------------------------------------------
    uint32 Instance::GetSurfaceCustomId( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex ) const
    {
        if ( LODIndex>=0 && LODIndex<m_pD->Lods.Num() &&
             ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() &&
             SurfaceIndex>=0 && SurfaceIndex<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() )
        {
            return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].ExternalId;
        }
		else
		{
			check(false);
		}

        return 0;
    }


	//---------------------------------------------------------------------------------------------
    int32 Instance::GetMeshCount( int32 LODIndex, int32 ComponentIndex ) const
	{
		check(LODIndex >= 0 && LODIndex < m_pD->Lods.Num());
		check(ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num());

		return m_pD->Lods[LODIndex].Components[ComponentIndex].Meshes.Num();
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::GetImageCount( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex ) const
	{
		check(LODIndex >= 0 && LODIndex < m_pD->Lods.Num());
		check(ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num());
		check(SurfaceIndex >= 0 && SurfaceIndex < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num());

		return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Images.Num();
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::GetVectorCount( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex ) const
	{
		check(LODIndex >= 0 && LODIndex < m_pD->Lods.Num());
		check(ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num());
		check(SurfaceIndex >= 0 && SurfaceIndex < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num());

		return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Vectors.Num();
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::GetScalarCount( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex ) const
	{
		check(LODIndex >= 0 && LODIndex < m_pD->Lods.Num());
		check(ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num());
		check(SurfaceIndex >= 0 && SurfaceIndex < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num());

		return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Scalars.Num();
	}


    //---------------------------------------------------------------------------------------------
    int32 Instance::GetStringCount( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex ) const
    {
		check(LODIndex >= 0 && LODIndex < m_pD->Lods.Num());
		check(ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num());
		check(SurfaceIndex >= 0 && SurfaceIndex < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num());

		return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Strings.Num();
	}


    //---------------------------------------------------------------------------------------------
	FResourceID Instance::GetMeshId( int32 LODIndex, int32 ComponentIndex, int32 mesh ) const
    {
        check( LODIndex>=0 && LODIndex<m_pD->Lods.Num() );
        check( ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() );
        check( mesh>=0 && mesh<m_pD->Lods[LODIndex].Components[ComponentIndex].Meshes.Num() );

		FResourceID result = m_pD->Lods[LODIndex].Components[ComponentIndex].Meshes[mesh].Id;
        return result;
    }


	//---------------------------------------------------------------------------------------------
	FResourceID Instance::GetImageId( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, int32 img ) const
	{
		check( LODIndex>=0 && LODIndex<m_pD->Lods.Num() );
		check( ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() );
        check( SurfaceIndex>=0 && SurfaceIndex<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() );
        check( img>=0 && img<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Images.Num() );

		FResourceID result = m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Images[img].Id;
        return result;
	}


	//---------------------------------------------------------------------------------------------
    FName Instance::GetImageName( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, int32 img ) const
	{
		check( LODIndex>=0 && LODIndex<m_pD->Lods.Num() );
		check( ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() );
        check( SurfaceIndex>=0 && SurfaceIndex<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() );
        check( img>=0 && img<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Images.Num() );

        return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Images[img].Name;
	}


	//---------------------------------------------------------------------------------------------
	FVector4f Instance::GetVector( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, int32 vec ) const
	{
		check( LODIndex>=0 && LODIndex<m_pD->Lods.Num() );
		check( ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() );
        check( SurfaceIndex>=0 && SurfaceIndex<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() );
        check( vec>=0 && vec<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Vectors.Num() );

        return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Vectors[vec].Value;
	}


	//---------------------------------------------------------------------------------------------
	FName Instance::GetVectorName( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, int32 vec ) const
	{
		check( LODIndex>=0 && LODIndex<m_pD->Lods.Num() );
		check( ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() );
        check( SurfaceIndex>=0 && SurfaceIndex<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() );
        check( vec>=0 && vec<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Vectors.Num() );

        return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Vectors[vec].Name;
	}


	//---------------------------------------------------------------------------------------------
    float Instance::GetScalar( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, int32 sca ) const
	{
		check( LODIndex>=0 && LODIndex<m_pD->Lods.Num() );
		check( ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() );
        check( SurfaceIndex>=0 && SurfaceIndex<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() );
        check( sca>=0 && sca<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Scalars.Num() );

        return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Scalars[sca].Value;
	}


	//---------------------------------------------------------------------------------------------
	FName Instance::GetScalarName( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, int32 sca ) const
	{
		check( LODIndex>=0 && LODIndex<m_pD->Lods.Num() );
		check( ComponentIndex>=0 && ComponentIndex<m_pD->Lods[LODIndex].Components.Num() );
        check( SurfaceIndex>=0 && SurfaceIndex<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() );
        check( sca>=0 && sca<m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Scalars.Num() );

        return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Scalars[sca].Name;
	}


    //---------------------------------------------------------------------------------------------
    FString Instance::GetString( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, int32 str ) const
    {
        check( LODIndex >= 0 && LODIndex < m_pD->Lods.Num() );
        check( ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num() );
        check( SurfaceIndex >= 0 &&
                        SurfaceIndex < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() );

        bool valid =
            str >= 0 &&
            str < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Strings.Num();
        check(valid);

        if (valid)
        {
            return m_pD->Lods[LODIndex]
				.Components[ComponentIndex]
				.Surfaces[SurfaceIndex]
				.Strings[str]
				.Value;
        }

        return "";
    }


    //---------------------------------------------------------------------------------------------
	FName Instance::GetStringName( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, int32 str ) const
    {
        check( LODIndex >= 0 && LODIndex < m_pD->Lods.Num() );
        check( ComponentIndex >= 0 && ComponentIndex < m_pD->Lods[LODIndex].Components.Num() );
        check( SurfaceIndex >= 0 &&
                        SurfaceIndex < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() );
        bool valid =
            str >= 0 &&
            str < m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Strings.Num();
        check( valid );

        if (valid)
        {
            return m_pD->Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex].Strings[str].Name;
        }

        return NAME_None;
    }


    //---------------------------------------------------------------------------------------------
	int32 Instance::GetExtensionDataCount() const
	{
		return m_pD->ExtensionData.Num();
	}


    //---------------------------------------------------------------------------------------------
	void Instance::GetExtensionData(int32 Index, Ptr<const class ExtensionData>& OutExtensionData, FName& OutName) const
	{
		check(m_pD->ExtensionData.IsValidIndex(Index));

		OutExtensionData = m_pD->ExtensionData[Index].Data;
		OutName = m_pD->ExtensionData[Index].Name;
	}


    //---------------------------------------------------------------------------------------------
	int32 Instance::Private::AddLOD()
	{
		int32 result = Lods.Num();
		Lods.Add( INSTANCE_LOD() );
		return result;
	}


    //---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddComponent( int32 LODIndex )
    {
        // Automatically create the necessary lods and components
        while (LODIndex >= Lods.Num())
        {
            AddLOD();
        }

        int32 result = Lods[LODIndex].Components.Num();
        Lods[LODIndex].Components.Add( INSTANCE_COMPONENT() );
        return result;
    }


    //---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddSurface( int32 LODIndex, int32 ComponentIndex )
    {
        // Automatically create the necessary lods and components
        while (LODIndex >= Lods.Num())
        {
            AddLOD();
        }
        while (ComponentIndex >= Lods[LODIndex].Components.Num())
        {
            AddComponent(LODIndex);
        }

        int32 result = Lods[LODIndex].Components[ComponentIndex].Surfaces.Num();
        Lods[LODIndex].Components[ComponentIndex].Surfaces.Add( INSTANCE_SURFACE() );
        return result;
    }


    //---------------------------------------------------------------------------------------------
    void Instance::Private::SetComponentName( int32 LODIndex, int32 ComponentIndex, FName Name)
    {
        // Automatically create the necessary lods and components
        while (LODIndex >= Lods.Num())
        {
            AddLOD();
        }
        while (ComponentIndex >= Lods[LODIndex].Components.Num())
        {
            AddComponent( LODIndex );
        }

        INSTANCE_COMPONENT& component = Lods[LODIndex].Components[ComponentIndex];
        component.Name = Name;
    }


    //---------------------------------------------------------------------------------------------
    void Instance::Private::SetSurfaceName( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, FName Name)
    {
        // Automatically create the necessary lods and components
        while ( LODIndex>=Lods.Num() )
        {
            AddLOD();
        }
        while (ComponentIndex >= Lods[LODIndex].Components.Num())
        {
            AddComponent( LODIndex );
        }
        while ( SurfaceIndex>=Lods[LODIndex].Components[ComponentIndex].Surfaces.Num() )
        {
            AddSurface( LODIndex, ComponentIndex );
        }

        INSTANCE_SURFACE& surface = Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex];
        surface.Name = Name;
    }


	//---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddMesh( int32 LODIndex, int32 ComponentIndex, FResourceID meshId, FName Name)
	{
		// Automatically create the necessary lods and components
		while (LODIndex >= Lods.Num())
		{
			AddLOD();
		}
		while (ComponentIndex >= Lods[LODIndex].Components.Num())
		{
			AddComponent( LODIndex );
		}

		INSTANCE_COMPONENT& component = Lods[LODIndex].Components[ComponentIndex];
        int32 result = component.Meshes.Num();
        component.Meshes.Emplace( meshId, Name );

		return result;
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddImage( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, FResourceID imageId, FName Name)
	{
		// Automatically create the necessary lods and components
		while (LODIndex >= Lods.Num())
		{
			AddLOD();
		}
		while (ComponentIndex >= Lods[LODIndex].Components.Num())
		{
			AddComponent(LODIndex);
		}
		while (SurfaceIndex >= Lods[LODIndex].Components[ComponentIndex].Surfaces.Num())
		{
			AddSurface(LODIndex, ComponentIndex);
		}

        INSTANCE_SURFACE& surface = Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex];
        int32 result = surface.Images.Num();
        surface.Images.Emplace( imageId, Name);

        return result;
	}


	//---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddVector( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, const FVector4f& vec, FName Name)
	{
		// Automatically create the necessary lods and components
		while (LODIndex >= Lods.Num())
		{
			AddLOD();
		}
		while (ComponentIndex >= Lods[LODIndex].Components.Num())
		{
			AddComponent(LODIndex);
		}
		while (SurfaceIndex >= Lods[LODIndex].Components[ComponentIndex].Surfaces.Num())
		{
			AddSurface(LODIndex, ComponentIndex);
		}

        INSTANCE_SURFACE& surface = Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex];
        int32 result = surface.Vectors.Num();
        surface.Vectors.Emplace( vec, Name);

        return result;
	}


    //---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddScalar( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, float sca, FName Name)
    {
        // Automatically create the necessary lods and components
		while (LODIndex >= Lods.Num())
		{
			AddLOD();
		}
		while (ComponentIndex >= Lods[LODIndex].Components.Num())
		{
			AddComponent(LODIndex);
		}
		while (SurfaceIndex >= Lods[LODIndex].Components[ComponentIndex].Surfaces.Num())
		{
			AddSurface(LODIndex, ComponentIndex);
		}

        INSTANCE_SURFACE& surface = Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex];
        int32 result = surface.Scalars.Num();
        surface.Scalars.Emplace( sca, Name );

        return result;
    }


    //---------------------------------------------------------------------------------------------
    int32 Instance::Private::AddString( int32 LODIndex, int32 ComponentIndex, int32 SurfaceIndex, const FString& Value, FName Name)
    {
        // Automatically create the necessary lods and components
		while (LODIndex >= Lods.Num())
		{
			AddLOD();
		}
		while (ComponentIndex >= Lods[LODIndex].Components.Num())
		{
			AddComponent(LODIndex);
		}
		while (SurfaceIndex >= Lods[LODIndex].Components[ComponentIndex].Surfaces.Num())
		{
			AddSurface(LODIndex, ComponentIndex);
		}

        INSTANCE_SURFACE& surface = Lods[LODIndex].Components[ComponentIndex].Surfaces[SurfaceIndex];
        int32 result = surface.Strings.Num();
        surface.Strings.Emplace(Value, Name );

        return result;
    }


    //---------------------------------------------------------------------------------------------
	void Instance::Private::AddExtensionData(const Ptr<const class ExtensionData>& Data, FName Name)
	{
		check(Data);

		NamedExtensionData& Entry = ExtensionData.AddDefaulted_GetRef();
		Entry.Data = Data;
		Entry.Name = Name;
	}
}

