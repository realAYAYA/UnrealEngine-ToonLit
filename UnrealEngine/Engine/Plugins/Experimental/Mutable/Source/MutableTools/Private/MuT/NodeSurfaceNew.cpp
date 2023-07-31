// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeSurfaceNew.h"

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeString.h"
#include "MuT/NodeSurfaceNewPrivate.h"

#include <memory>
#include <utility>

namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeSurfaceNew::Private::s_type =
            NODE_TYPE( "NewSurface", NodeSurface::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeSurfaceNew, EType::New, Node, Node::EType::Surface)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::GetInputCount() const
	{
        return m_pD->m_meshes.Num() + m_pD->m_images.Num() + m_pD->m_vectors.Num() +
                      m_pD->m_scalars.Num() + m_pD->m_strings.Num();
    }


	//---------------------------------------------------------------------------------------------
    mu::Node* NodeSurfaceNew::GetInputNode( int i ) const
	{
		check( i >=0 && i < GetInputCount() );

        int32 index = i;

        if ( index < m_pD->m_meshes.Num() )
        {
            return m_pD->m_meshes[index].m_pMesh.get();
        }
		else
		{
            index -= m_pD->m_meshes.Num();
        }

        if ( index < m_pD->m_images.Num() )
        {
            return m_pD->m_images[index].m_pImage.get();
        }
        else 
		{
            index -= m_pD->m_images.Num();
        }

        if ( index < m_pD->m_vectors.Num() )
        {
            return m_pD->m_vectors[index].m_pVector.get();
        }
        else
        {
            index -= m_pD->m_vectors.Num();
        }

        if ( index < m_pD->m_scalars.Num() )
        {
            return m_pD->m_scalars[index].m_pScalar.get();
        }
        else
        {
            index -= m_pD->m_scalars.Num();
        }

        if ( index < m_pD->m_strings.Num() )
        {
            return m_pD->m_strings[index].m_pString.get();
        }
        else
        {
            index -= m_pD->m_strings.Num();
        }

        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetInputNode( int i, NodePtr pNode )
	{
		check( i >=0 && i < GetInputCount());

        std::size_t index = size_t( i );

        if ( index<m_pD->m_meshes.Num() )
		{
			m_pD->m_meshes[ index ].m_pMesh = dynamic_cast<NodeMesh*>(pNode.get());
            return;
        }
		else
		{
            index -= m_pD->m_meshes.Num();
        }


        if ( index < m_pD->m_images.Num())
        {
			m_pD->m_images[index].m_pImage = dynamic_cast<NodeImage*>(pNode.get());
            return;
        }
        else
        {
            index -= m_pD->m_images.Num();
        }
        
		if ( index < m_pD->m_vectors.Num() )
        {
            m_pD->m_vectors[index].m_pVector = dynamic_cast<NodeColour*>(pNode.get());
            return;
        }
        else
        {
            index -= m_pD->m_vectors.Num();
        }

        if ( index < m_pD->m_scalars.Num() )
        {
            m_pD->m_scalars[index].m_pScalar = dynamic_cast<NodeScalar*>( pNode.get() );
            return;
        }
        else
        {
            index -= m_pD->m_scalars.Num();
        }

        if ( index < m_pD->m_strings.Num() )
        {
            m_pD->m_strings[index].m_pString = dynamic_cast<NodeString*>( pNode.get() );
            return;
        }
        else
        {
            index -= m_pD->m_strings.Num();
        }
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    const char* NodeSurfaceNew::GetName() const
	{
		const char* strResult = m_pD->m_name.c_str();

		return strResult;
	}


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetName( const char* strName )
    {
        if (strName)
        {
            m_pD->m_name = strName;
        }
        else
        {
            m_pD->m_name = "";
        }
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetCustomID( uint32_t id )
    {
        m_pD->m_customID = id;
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
    const char* NodeSurfaceNew::GetMeshName( int index ) const
	{
		check( index >=0 && index < (int)m_pD->m_meshes.Num() );

		const char* strResult = m_pD->m_meshes[ index ].m_name.c_str();

		return strResult;
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetMeshName( int index, const char* strName )
	{
		check( index >=0 && index < (int)m_pD->m_meshes.Num() );

		if (strName)
		{
			m_pD->m_meshes[ index ].m_name = strName;
		}
		else
		{
			m_pD->m_meshes[ index ].m_name = "";
		}
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
    const char* NodeSurfaceNew::GetImageName( int index ) const
	{
		check( index >=0 && index < (int)m_pD->m_images.Num() );

		const char* strResult = m_pD->m_images[ index ].m_name.c_str();

		return strResult;
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetImageName( int index, const char* strName )
	{
		check( index >=0 && index < (int)m_pD->m_images.Num() );

		if (strName)
		{
			m_pD->m_images[ index ].m_name = strName;
		}
		else
		{
			m_pD->m_images[ index ].m_name = "";
		}
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
	void NodeSurfaceNew::SetImageAdditionalNames(int index, const char* strMaterialName, const char* strMaterialParameterName )
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
	const char* NodeSurfaceNew::GetVectorName(int index) const
	{
        check(index >= 0 && index < (int)m_pD->m_vectors.Num());

		const char* strResult = m_pD->m_vectors[index].m_name.c_str();

		return strResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceNew::SetVectorName(int index, const char* strName)
	{
        check(index >= 0 && index < (int)m_pD->m_vectors.Num());

		if (strName)
		{
			m_pD->m_vectors[index].m_name = strName;
		}
		else
		{
			m_pD->m_vectors[index].m_name = "";
		}
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
    const char* NodeSurfaceNew::GetScalarName(int index) const
    {
        check(index >= 0 && index < (int)m_pD->m_scalars.Num());

        const char* strResult = m_pD->m_scalars[index].m_name.c_str();

        return strResult;
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetScalarName(int index, const char* strName)
    {
        check(index >= 0 && index < (int)m_pD->m_scalars.Num());

        if (strName)
        {
            m_pD->m_scalars[index].m_name = strName;
        }
        else
        {
            m_pD->m_scalars[index].m_name = "";
        }
    }


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::GetStringCount() const { return (int)m_pD->m_strings.Num(); }


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
    const char* NodeSurfaceNew::GetStringName( int index ) const
    {
        check( index >= 0 && index < (int)m_pD->m_strings.Num() );

        const char* strResult = m_pD->m_strings[index].m_name.c_str();

        return strResult;
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceNew::SetStringName( int index, const char* strName )
    {
        check( index >= 0 && index < (int)m_pD->m_strings.Num() );

        if ( strName )
        {
            m_pD->m_strings[index].m_name = strName;
        }
        else
        {
            m_pD->m_strings[index].m_name = "";
        }
    }


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::Private::FindImage( const char* strName ) const
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
	int NodeSurfaceNew::Private::FindMesh(const char* strName) const
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
    int NodeSurfaceNew::Private::FindVector(const char* strName) const
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
    int NodeSurfaceNew::Private::FindScalar( const char* strName ) const
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
    int NodeSurfaceNew::Private::FindString( const char* strName ) const
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
	void NodeSurfaceNew::AddTag(const char* tagName)
	{
		m_pD->m_tags.Add(tagName);
	}


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceNew::GetTagCount() const
    {
        return m_pD->m_tags.Num();
    }


    //---------------------------------------------------------------------------------------------
    const char* NodeSurfaceNew::GetTag( int i ) const
    {
        if (i>=0 && i<GetTagCount())
        {
            return m_pD->m_tags[i].c_str();
        }
        return nullptr;
    }

}


