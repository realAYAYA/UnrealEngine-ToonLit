// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeMeshSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshSwitchPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeMeshSwitch::Private::s_type =
			FNodeType( "MeshSwitch", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshSwitch, EType::Switch, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeMeshSwitch::GetParameter() const
	{
		return m_pD->m_pParameter.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSwitch::SetParameter( NodeScalarPtr pNode )
	{
		m_pD->m_pParameter = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSwitch::SetOptionCount( int t )
	{
		m_pD->m_options.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMeshSwitch::GetOption( int t ) const
	{
		check( t>=0 && t<m_pD->m_options.Num() );
		return m_pD->m_options[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSwitch::SetOption( int t, NodeMeshPtr pNode )
	{
		check( t>=0 && t<m_pD->m_options.Num() );
		m_pD->m_options[t] = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshSwitch::Private::GetLayout( int index ) const
	{
		for (int32 i=0; i<m_options.Num(); ++i)
		{
			if (m_options[i])
			{
				NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>(m_options[i]->GetBasePrivate());
				NodeLayoutPtr pResult = pPrivate->GetLayout(index);
				return pResult;
			}
		}

		return nullptr;
	}



}


