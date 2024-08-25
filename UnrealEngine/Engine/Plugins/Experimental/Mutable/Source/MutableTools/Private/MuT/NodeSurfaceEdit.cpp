// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeSurfaceEdit.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeSurfaceEditPrivate.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeSurfaceEdit::Private::s_type =
            FNodeType( "EditSurface", NodeSurface::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeSurfaceEdit, EType::Edit, Node, Node::EType::Surface)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetParent( NodeSurface* p )
	{
		m_pD->m_pParent = p;
	}


	//---------------------------------------------------------------------------------------------
    NodeSurface* NodeSurfaceEdit::GetParent() const
	{
        return m_pD->m_pParent.get();
	}


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::AddTag(const FString& tagName)
    {
        m_pD->m_tags.Add(tagName);
    }


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetMesh(NodePatchMesh* pNode )
	{
		m_pD->m_pMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
    NodePatchMesh* NodeSurfaceEdit::GetMesh() const
	{
		return m_pD->m_pMesh.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetMorph( NodeMeshPtr pNode )
	{
		m_pD->m_pMorph = pNode;
	}


	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeSurfaceEdit::GetMorph() const
	{
		return m_pD->m_pMorph.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceEdit::SetFactor(NodeScalarPtr pNode)
	{
		m_pD->m_pFactor = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeSurfaceEdit::GetFactor() const
	{
		return m_pD->m_pFactor.get();
	}


	//---------------------------------------------------------------------------------------------
    int NodeSurfaceEdit::GetImageCount() const
	{
		return m_pD->m_textures.Num();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetImageCount( int num )
	{
		check( num >=0 );
		m_pD->m_textures.SetNum( num );
	}


	//---------------------------------------------------------------------------------------------
    NodeImagePtr NodeSurfaceEdit::GetImage( int index ) const
	{
		check( index >=0 && index < m_pD->m_textures.Num() );

		return m_pD->m_textures[ index ].m_pExtend.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetImage( int index, NodeImagePtr pNode )
	{
		check( index >=0 && index < m_pD->m_textures.Num() );

		m_pD->m_textures[ index ].m_pExtend = pNode;
	}


	//---------------------------------------------------------------------------------------------
    NodePatchImagePtr NodeSurfaceEdit::GetPatch( int index ) const
	{
		check( index >=0 && index < m_pD->m_textures.Num() );

		return m_pD->m_textures[ index ].m_pPatch.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetPatch( int index, NodePatchImagePtr pNode )
	{
		check( index >=0 && index < m_pD->m_textures.Num() );

		m_pD->m_textures[ index ].m_pPatch = pNode;
	}

}


