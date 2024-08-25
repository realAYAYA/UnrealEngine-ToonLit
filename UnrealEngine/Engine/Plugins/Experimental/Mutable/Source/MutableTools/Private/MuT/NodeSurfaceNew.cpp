// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeSurfaceNew.h"

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeString.h"
#include "MuT/NodeSurfaceNewPrivate.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeSurfaceNew::Private::s_type =
            FNodeType( "NewSurface", NodeSurface::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeSurfaceNew, EType::New, Node, Node::EType::Surface)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetName( const FString& strName )
    {
        m_pD->m_name = strName;
    }


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceNew::SetSharedSurfaceId(int32 SharedSurfaceId)
    {
		m_pD->SharedSurfaceId = SharedSurfaceId;
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetCustomID( uint32 id )
    {
        m_pD->ExternalId = id;
    }


	//---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::GetMeshCount() const
	{
		return (int)m_pD->m_meshes.Num();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetMeshCount( int num )
	{
		check( num >=0 );
		m_pD->m_meshes.SetNum( num );
	}


	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeSurfaceNew::GetMesh( int index ) const
	{
		check( index >=0 && index < (int)m_pD->m_meshes.Num() );

		return m_pD->m_meshes[ index ].m_pMesh.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetMesh( int index, NodeMeshPtr pNode )
	{
		check( index >=0 && index < (int)m_pD->m_meshes.Num() );

		m_pD->m_meshes[ index ].m_pMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
    const FString& NodeSurfaceNew::GetMeshName( int index ) const
	{
		check( index >=0 && index < (int)m_pD->m_meshes.Num() );

		return m_pD->m_meshes[ index ].m_name;
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetMeshName( int index, const FString& strName )
	{
		check( index >=0 && index < (int)m_pD->m_meshes.Num() );

		m_pD->m_meshes[ index ].m_name = strName;
	}


	//---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::GetImageCount() const
	{
		return (int)m_pD->m_images.Num();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetImageCount( int num )
	{
		check( num >=0 );
		m_pD->m_images.SetNum( num );
	}


	//---------------------------------------------------------------------------------------------
    NodeImagePtr NodeSurfaceNew::GetImage( int index ) const
	{
		check( index >=0 && index < (int)m_pD->m_images.Num() );

		return m_pD->m_images[ index ].m_pImage.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetImage( int index, NodeImagePtr pNode )
	{
		check( index >=0 && index < (int)m_pD->m_images.Num() );

		m_pD->m_images[ index ].m_pImage = pNode;
	}


	//---------------------------------------------------------------------------------------------
    const FString& NodeSurfaceNew::GetImageName( int index ) const
	{
		check( index >=0 && index < (int)m_pD->m_images.Num() );

		return m_pD->m_images[ index ].m_name;
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetImageName( int index, const FString& strName )
	{
		check( index >=0 && index < (int)m_pD->m_images.Num() );

		m_pD->m_images[ index ].m_name = strName;
	}


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::GetImageLayoutIndex( int index ) const
    {
        check( index >= 0 && index < (int)m_pD->m_images.Num() );

        return int(m_pD->m_images[index].m_layoutIndex);
    }


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceNew::SetImageLayoutIndex(int index, int layoutIndex)
	{
		check(index >= 0 && index < (int)m_pD->m_images.Num());

		m_pD->m_images[index].m_layoutIndex = int8_t(layoutIndex);
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceNew::SetImageAdditionalNames(int index, const FString& strMaterialName, const FString& strMaterialParameterName )
	{
		if (index >= 0 && index < (int)m_pD->m_images.Num())
		{
			m_pD->m_images[index].m_materialName = strMaterialName;
			m_pD->m_images[index].m_materialParameterName = strMaterialParameterName;
		}
		else
		{
			check(false);
		}
	}


    //---------------------------------------------------------------------------------------------
	int NodeSurfaceNew::GetVectorCount() const
	{
		return (int)m_pD->m_vectors.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceNew::SetVectorCount(int num)
	{
		check(num >= 0);
		m_pD->m_vectors.SetNum(num);
	}


	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeSurfaceNew::GetVector(int index) const
	{
		check(index >= 0 && index < (int)m_pD->m_vectors.Num());

		return m_pD->m_vectors[index].m_pVector.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceNew::SetVector(int index, NodeColourPtr pNode)
	{
		check(index >= 0 && index < (int)m_pD->m_vectors.Num());

		m_pD->m_vectors[index].m_pVector = pNode;
	}


	//---------------------------------------------------------------------------------------------
	const FString& NodeSurfaceNew::GetVectorName(int index) const
	{
        check(index >= 0 && index < (int)m_pD->m_vectors.Num());

		return m_pD->m_vectors[index].m_name;
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceNew::SetVectorName(int index, const FString& strName)
	{
        check(index >= 0 && index < (int)m_pD->m_vectors.Num());

		m_pD->m_vectors[index].m_name = strName;
	}


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::GetScalarCount() const
    {
        return (int)m_pD->m_scalars.Num();
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetScalarCount(int num)
    {
        check(num >= 0);
        m_pD->m_scalars.SetNum(num);
    }


    //---------------------------------------------------------------------------------------------
    NodeScalarPtr NodeSurfaceNew::GetScalar(int index) const
    {
        check(index >= 0 && index < (int)m_pD->m_scalars.Num());

        return m_pD->m_scalars[index].m_pScalar.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetScalar(int index, NodeScalarPtr pNode)
    {
        check(index >= 0 && index < (int)m_pD->m_scalars.Num());

        m_pD->m_scalars[index].m_pScalar = pNode;
    }


    //---------------------------------------------------------------------------------------------
    const FString& NodeSurfaceNew::GetScalarName(int index) const
    {
        check(index >= 0 && index < (int)m_pD->m_scalars.Num());

        return m_pD->m_scalars[index].m_name;
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetScalarName(int index, const FString& strName)
    {
        check(index >= 0 && index < (int)m_pD->m_scalars.Num());

        m_pD->m_scalars[index].m_name = strName;
    }


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::GetStringCount() const 
	{ 
		return (int)m_pD->m_strings.Num(); 
	}


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetStringCount( int num )
    {
        check( num >= 0 );
        m_pD->m_strings.SetNum( num );
    }


    //---------------------------------------------------------------------------------------------
    NodeStringPtr NodeSurfaceNew::GetString( int index ) const
    {
        check( index >= 0 && index < (int)m_pD->m_strings.Num() );

        return m_pD->m_strings[index].m_pString.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetString( int index, NodeStringPtr pNode )
    {
        check( index >= 0 && index < (int)m_pD->m_strings.Num() );

        m_pD->m_strings[index].m_pString = pNode;
    }


    //---------------------------------------------------------------------------------------------
    const FString& NodeSurfaceNew::GetStringName( int index ) const
    {
        check( index >= 0 && index < (int)m_pD->m_strings.Num() );

        return m_pD->m_strings[index].m_name;
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetStringName( int index, const FString& strName )
    {
        check( index >= 0 && index < (int)m_pD->m_strings.Num() );

        m_pD->m_strings[index].m_name = strName;
    }


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::Private::FindImage( const FString& strName ) const
	{		
		for ( int32 i = 0; i<m_images.Num(); ++i )
		{
			if ( m_images[i].m_name == strName)
			{
				return i;
			}
		}

		return -1;
	}


	//---------------------------------------------------------------------------------------------
	int NodeSurfaceNew::Private::FindMesh(const FString& strName) const
	{
		for ( int32 i = 0; i < m_meshes.Num(); ++i)
		{
			if (m_meshes[i].m_name == strName)
			{
				return i;
			}
		}

		return -1;
	}


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::Private::FindVector(const FString& strName) const
    {
        for ( int32 i = 0; i < m_vectors.Num(); ++i)
        {
            if (m_vectors[i].m_name == strName)
            {
                return i;
            }
        }

        return -1;
    }


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::Private::FindScalar( const FString& strName ) const
    {
        for (int32 i = 0; i < m_scalars.Num(); ++i)
        {
            if ( m_scalars[i].m_name == strName )
            {
                return i;
            }
        }

        return -1;
    }


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::Private::FindString( const FString& strName ) const
    {
        for (int32 i = 0; i < m_strings.Num(); ++i)
        {
            if ( m_strings[i].m_name == strName )
            {
                return i;
            }
        }

        return -1;
    }


    //---------------------------------------------------------------------------------------------
	void NodeSurfaceNew::AddTag(const FString& tagName)
	{
		m_pD->m_tags.Add(tagName);
	}

}


