// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeLOD.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeLODPrivate.h"
#include "MuT/NodeModifier.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeLOD::Private::s_type =
			NODE_TYPE( "LOD", NodeLOD::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeLOD, EType::LOD, Node, Node::EType::LOD);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeLOD::GetInputCount() const
	{
		return m_pD->m_components.Num()+m_pD->m_modifiers.Num();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeLOD::GetInputNode( int i ) const
	{
        check(i >= 0 && i < GetInputCount());

        if (i < int(m_pD->m_components.Num()))
        {
            return m_pD->m_components[i].get();
		}

        i -= int(m_pD->m_components.Num());

        if (i < int(m_pD->m_modifiers.Num()))
        {
            return m_pD->m_modifiers[i].get();
        }

        return nullptr;
    }


    //---------------------------------------------------------------------------------------------
	void NodeLOD::SetInputNode( int i, NodePtr pNode )
	{
        check(i >= 0 && i < GetInputCount());

        if (i<int(m_pD->m_components.Num()))
		{
            m_pD->m_components[i] = dynamic_cast<NodeComponent*>(pNode.get());
            return;
        }

        i -= int(m_pD->m_components.Num());
        if (i < int(m_pD->m_modifiers.Num()))
        {
            m_pD->m_modifiers[i] = dynamic_cast<NodeModifier*>(pNode.get());
        }
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	int NodeLOD::GetComponentCount() const
	{
		return (int)m_pD->m_components.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeLOD::SetComponentCount(int num)
	{
		check(num >= 0);
		m_pD->m_components.SetNum(num);
	}


	//---------------------------------------------------------------------------------------------
	NodeComponentPtr NodeLOD::GetComponent(int index) const
	{
		check(index >= 0 && index < m_pD->m_components.Num());

		return m_pD->m_components[index].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeLOD::SetComponent(int index, NodeComponentPtr pComponent)
	{
		check(index >= 0 && index < m_pD->m_components.Num());

		m_pD->m_components[index] = pComponent;
	}


	//---------------------------------------------------------------------------------------------
	int NodeLOD::GetModifierCount() const
	{
		return (int)m_pD->m_modifiers.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeLOD::SetModifierCount(int num)
	{
		check(num >= 0);
		m_pD->m_modifiers.SetNum(num);
	}


	//---------------------------------------------------------------------------------------------
	void NodeLOD::SetModifier(int index, NodeModifierPtr pModifier)
	{
		check(index >= 0 && index < m_pD->m_modifiers.Num());

		m_pD->m_modifiers[index] = pModifier;
	}


}


