// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeObjectNew.h"

#include "HAL/PlatformString.h"
#include "Misc/AssertionMacros.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeLODPrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeObjectNewPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeSurfaceNew.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeObjectNew::Private::s_type = FNodeType( "Object", NodeObject::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeObjectNew, EType::New, Node, Node::EType::Object )


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const FString& NodeObjectNew::GetName() const
	{
		return m_pD->m_name;
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::SetName( const FString& Name )
	{
		m_pD->m_name = Name;
	}


	const FString& NodeObjectNew::GetUid() const
	{
		return m_pD->m_uid;
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::SetUid( const FString& Uid )
	{
		m_pD->m_uid = Uid;
	}


	//---------------------------------------------------------------------------------------------
	int NodeObjectNew::GetLODCount() const
	{
		return m_pD->m_lods.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::SetLODCount( int num )
	{
		check( num >=0 );
		m_pD->m_lods.SetNum( num );
	}


	//---------------------------------------------------------------------------------------------
	NodeLODPtr NodeObjectNew::GetLOD( int index ) const
	{
		check( index >=0 && index < m_pD->m_lods.Num() );

		return m_pD->m_lods[ index ].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::SetLOD( int index, NodeLODPtr pLOD )
	{
		check( index >=0 && index < m_pD->m_lods.Num() );

		m_pD->m_lods[ index ] = pLOD;
	}


	//---------------------------------------------------------------------------------------------
	int NodeObjectNew::GetChildCount() const
	{
		return m_pD->m_children.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::SetChildCount( int num )
	{
		check( num >=0 );
		m_pD->m_children.SetNum( num );
	}


	//---------------------------------------------------------------------------------------------
	NodeObjectPtr NodeObjectNew::GetChild( int index ) const
	{
		check( index >=0 && index < m_pD->m_children.Num() );

		return m_pD->m_children[ index ].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::SetChild( int index, NodeObjectPtr pObject )
	{
		check( index >=0 && index < m_pD->m_children.Num() );

		m_pD->m_children[ index ] = pObject;
	}


	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeObjectNew::Private::GetLayout( int lod,
                                                     int component,
                                                     int surface,
                                                     int //texture
                                                     ) const
	{
		check( lod>=0 && lod<m_lods.Num() );

		NodeLayoutPtr pLayout;

		const NodeComponent* pCompGeneric = m_lods[lod]->GetComponent( component ).get();

        if (pCompGeneric->GetType()==NodeComponentNew::GetStaticType())
        {
			const NodeComponentNew* pComp = static_cast<const NodeComponentNew*> (pCompGeneric);
			const NodeSurface* pSurfaceGeneric = pComp->GetSurface( surface );
            if (pSurfaceGeneric->GetType()==NodeSurfaceNew::GetStaticType())
            {
				const NodeSurfaceNew* pSurface = static_cast<const NodeSurfaceNew*> (pSurfaceGeneric);
				
				// TODO: Look for the layout index of the given texture
                // TODO: Multiple meshes
                if ( pSurface->GetMeshCount()>0 )
                {
                    const NodeMesh* pMesh = pSurface->GetMesh( 0 ).get();
                    if ( pMesh )
                    {
                        NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>( pMesh->GetBasePrivate() );
                        pLayout = pPrivate->GetLayout( 0 );
                    }
                }
            }
        }

		// TODO: layout index
		return pLayout;
	}


    //---------------------------------------------------------------------------------------------
    bool NodeObjectNew::Private::HasComponent( const NodeComponent* pComponent ) const
    {
        bool found = false;
        for ( int32 l=0; !found && l<m_lods.Num(); ++l )
        {
			found = m_lods[l]->GetPrivate()->m_components.FindByPredicate([pComponent](const NodeComponentPtr& e) { return e.get() == pComponent; }) != nullptr;
        }

        return found;
    }


	//---------------------------------------------------------------------------------------------
	int32 NodeObjectNew::GetStateCount() const
	{
		return m_pD->m_states.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::SetStateCount( int32 c )
	{
		m_pD->m_states.SetNum( c );
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::SetStateName( int32 s, const FString& n )
	{
		check( s>=0 && s<GetStateCount() );
		m_pD->m_states[s].m_name = n;
	}


	//---------------------------------------------------------------------------------------------
	bool NodeObjectNew::HasStateParam( int32 s, const FString& param ) const
	{
		check( s>=0 && s<GetStateCount() );
		return m_pD->m_states[s].m_runtimeParams.Contains( param );
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::AddStateParam( int32 s, const FString& param )
	{
		check( s>=0 && s<GetStateCount() );

		if (!HasStateParam(s,param))
		{
			m_pD->m_states[s].m_runtimeParams.Add( param );
		}
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectNew::RemoveStateParam( int32 s, const FString& param )
	{
		check( s>=0 && s<GetStateCount() );

		int32 it = m_pD->m_states[s].m_runtimeParams.Find( param );
		if ( it != INDEX_NONE )
		{
			m_pD->m_states[s].m_runtimeParams.RemoveAt( it );
		}
	}


    //---------------------------------------------------------------------------------------------
    void NodeObjectNew::SetStateProperties( int32 StateIndex, 
		ETextureCompressionStrategy TextureCompressionStrategy, 
		bool bOnlyFirstLOD, 
		uint8 FirstLOD, 
		uint8 NumExtraLODsToBuildAfterFirstLOD )
    {
        check(StateIndex >=0 && StateIndex<GetStateCount() );

		FStateOptimizationOptions& Data = m_pD->m_states[StateIndex].m_optimisation;
        Data.TextureCompressionStrategy = TextureCompressionStrategy;
		Data.bOnlyFirstLOD = bOnlyFirstLOD;
		Data.FirstLOD = FirstLOD;
		Data.NumExtraLODsToBuildAfterFirstLOD = NumExtraLODsToBuildAfterFirstLOD;
    }

    //---------------------------------------------------------------------------------------------
	void NodeObjectNew::AddExtensionDataNode(NodeExtensionDataPtr Node, const FString& Name)
	{
		NodeObjectNew::Private::NamedExtensionDataNode& Entry = m_pD->m_extensionDataNodes.AddDefaulted_GetRef();
		Entry.Node = Node;
		Entry.Name = Name;
	}
}


