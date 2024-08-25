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
		FName Name;
        uint32 InternalId=0;
        uint32 ExternalId =0;
        uint32 SharedId =0;

		struct IMAGE
		{
            IMAGE(FResourceID InId, FName InName)
			{
				Id = InId;
				Name = InName;
			}

			FResourceID Id;
			FName Name;
		};

		TArray<IMAGE, TInlineAllocator<4>> Images;

		struct VECTOR
		{
			VECTOR( const FVector4f& v, FName InName)
			{
				Value = v;
				Name = InName;
			}

			FVector4f Value;
			FName Name;
		};

		TArray<VECTOR> Vectors;

        struct SCALAR
        {
            SCALAR( float v, FName InName )
            {
                Value = v;
                Name = InName;
            }

            float Value;
			FName Name;
		};

		TArray<SCALAR> Scalars;

        struct STRING
        {
            STRING(const FString& InValue, FName InName)
            {
				Value = InValue;
				Name = InName;
			}

			FString Value;
			FName Name;
		};

		TArray<STRING> Strings;
    };


    struct INSTANCE_COMPONENT
    {
        FName Name;

    	uint16 Id;

		struct MESH
		{
            MESH(FResourceID InId, FName InName)
			{
				Id = InId;
				Name = InName;
			}

			FResourceID Id;
			FName Name;
		};
		TArray<MESH, TInlineAllocator<2>> Meshes;

		// The order must match the meshes surfaces
		TArray<INSTANCE_SURFACE, TInlineAllocator<4>> Surfaces;
	};


    struct INSTANCE_LOD
    {
		TArray<INSTANCE_COMPONENT, TInlineAllocator<4>> Components;
    };

	struct NamedExtensionData
	{
		Ptr<const ExtensionData> Data;
		FName Name;
	};

	class Instance::Private
	{
	public:

        //!
        Instance::ID Id = 0;

		//!
		TArray<INSTANCE_LOD,TInlineAllocator<4>> Lods;

		// Every entry must have a valid ExtensionData and name
		TArray<NamedExtensionData> ExtensionData;

		int32 AddLOD();
		int32 AddComponent(int32 lod );
        void SetComponentName( int32 lod, int32 comp, FName Name );
		int32 AddMesh(int32 lod, int32 comp, FResourceID, FName Name);
		int32 AddSurface(int32 lod, int32 comp );
        void SetSurfaceName( int32 lod, int32 comp, int32 surf, FName Name);
		int32 AddImage( int32 lod, int32 comp, int32 surf, FResourceID, FName Name);
        int32 AddVector( int32 lod, int32 comp, int32 surf, const FVector4f&, FName Name);
        int32 AddScalar( int32 lod, int32 comp, int32 surf, float, FName Name);
        int32 AddString( int32 lod, int32 comp, int32 surf, const FString& Value, FName Name);
		
		// Data must be non-null
		void AddExtensionData(const Ptr<const class ExtensionData>& Data, FName Name);
    };
}
