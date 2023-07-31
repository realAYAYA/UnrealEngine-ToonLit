// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeSurfaceVariation.h"

#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeModifier.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurfaceVariationPrivate.h"

#include <memory>
#include <utility>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeSurfaceVariation::Private::s_type =
            NODE_TYPE( "SurfaceVariation", NodeSurface::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeSurfaceVariation, EType::Variation, Node, Node::EType::Surface)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeSurfaceVariation::GetInputCount() const
	{
        int32 c = m_pD->m_defaultSurfaces.Num();
        c += m_pD->m_defaultModifiers.Num();
        for (const auto& v : m_pD->m_variations)
		{
            c += v.m_surfaces.Num();
            c += v.m_modifiers.Num();
        }

		return (int)c;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeSurfaceVariation::GetInputNode( int i ) const
	{
		check( i >=0 && i < GetInputCount() );

        if ( i<m_pD->m_defaultSurfaces.Num())
		{
            return m_pD->m_defaultSurfaces[i].get();
		}
        i -= m_pD->m_defaultSurfaces.Num();

        if ( i<m_pD->m_defaultModifiers.Num())
        {
            return m_pD->m_defaultModifiers[i].get();
        }
        i -= m_pD->m_defaultModifiers.Num();

        for (const auto& v : m_pD->m_variations)
        {
            if (i < (int)v.m_surfaces.Num())
            {
                return v.m_surfaces[i].get();
            }
            i -= (int)v.m_surfaces.Num();

            if (i < (int)v.m_modifiers.Num())
            {
                return v.m_modifiers[i].get();
            }
            i -= (int)v.m_modifiers.Num();
        }

		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceVariation::SetInputNode( int i, NodePtr pNode )
	{
		check( i >=0 && i <  GetInputCount());

        if (i<m_pD->m_defaultSurfaces.Num())
		{
            m_pD->m_defaultSurfaces[i] = dynamic_cast<NodeSurface*>(pNode.get());
			return;
		}

        i -= m_pD->m_defaultSurfaces.Num();
        if (i<m_pD->m_defaultModifiers.Num())
        {

            m_pD->m_defaultModifiers[i] = dynamic_cast<NodeModifier*>(pNode.get());
            return;
        }
        i -= m_pD->m_defaultModifiers.Num();

        for (auto& v : m_pD->m_variations)
        {
            if (i < (int)v.m_surfaces.Num())
            {
                v.m_surfaces[i] = dynamic_cast<NodeSurface*>(pNode.get());
                return;
            }
            i -= (int)v.m_surfaces.Num();

            if (i < (int)v.m_modifiers.Num())
            {
                v.m_modifiers[i] = dynamic_cast<NodeModifier*>(pNode.get());
                return;
            }
            i -= (int)v.m_modifiers.Num();
        }
	}


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
	void NodeSurfaceVariation::SetVariationTag(int index, const char* strTag)
	{
		check(index >= 0 && index < m_pD->m_variations.Num());
		check(strTag);

        if (strTag)
        {
            m_pD->m_variations[index].m_tag = strTag;
        }
        else
        {
            m_pD->m_variations[index].m_tag = "";
        }
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


