// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeSurfaceVariation.h"

#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuT/NodeModifier.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurfaceVariationPrivate.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeSurfaceVariation::Private::s_type =
            FNodeType( "SurfaceVariation", NodeSurface::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeSurfaceVariation, EType::Variation, Node, Node::EType::Surface)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeSurfaceVariation::AddDefaultSurface( NodeSurface* p )
    {
        m_pD->m_defaultSurfaces.Add(p);
    }


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceVariation::AddDefaultModifier( NodeModifier* p )
    {
        m_pD->m_defaultModifiers.Add(p);
    }


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceVariation::GetVariationCount() const
	{
		return m_pD->m_variations.Num();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceVariation::SetVariationCount( int num )
	{
		check( num >=0 );
		m_pD->m_variations.SetNum( num );
	}


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceVariation::SetVariationType(VariationType type )
    {
        m_pD->m_type = type;
    }


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceVariation::SetVariationTag(int index, const FString& Tag)
	{
		check(index >= 0 && index < m_pD->m_variations.Num());

		m_pD->m_variations[index].m_tag = Tag;
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceVariation::AddVariationSurface(int index, NodeSurface* pNode)
	{
		check(index >= 0 && index < m_pD->m_variations.Num());

		m_pD->m_variations[index].m_surfaces.Add(pNode);
	}


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceVariation::AddVariationModifier(int index, NodeModifier* pModifier)
    {
        check(index >= 0 && index < m_pD->m_variations.Num());

        m_pD->m_variations[index].m_modifiers.Add(pModifier);
    }

}


