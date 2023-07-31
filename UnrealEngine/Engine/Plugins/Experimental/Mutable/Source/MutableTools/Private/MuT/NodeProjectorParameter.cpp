// Copyright Epic Games, Inc. All Rights Reserved.


#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeProjectorPrivate.h"
#include "MuT/NodeRange.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeProjectorParameter::Private::s_type =
			NODE_TYPE( "ProjectorParameter", NodeProjector::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeProjectorParameter, EType::Parameter, Node, Node::EType::Projector)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeProjectorParameter::GetInputCount() const
	{
        return m_pD->m_ranges.Num();
    }


	//---------------------------------------------------------------------------------------------
    Node* NodeProjectorParameter::GetInputNode( int i ) const
	{
        check( i<GetInputCount() );
        if (i<GetInputCount())
        {
            return m_pD->m_ranges[i].get();
        }
        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
    void NodeProjectorParameter::SetInputNode( int i, NodePtr n )
	{
        check( i<GetInputCount() );
        if (i<GetInputCount())
        {
            m_pD->m_ranges[i] = dynamic_cast<NodeRange*>(n.get());
        }
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const char* NodeProjectorParameter::GetName() const
	{
		return m_pD->m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeProjectorParameter::SetName( const char* strName )
	{
		if ( strName )
		{
			m_pD->m_name = strName;
		}
		else
		{
			m_pD->m_name = "";
		}
	}


	const char* NodeProjectorParameter::GetUid() const
	{
		return m_pD->m_uid.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeProjectorParameter::SetUid( const char* strUid )
	{
		if ( strUid )
		{
			m_pD->m_uid = strUid;
		}
		else
		{
			m_pD->m_uid = "";
		}
	}


	//---------------------------------------------------------------------------------------------
    void NodeProjectorParameter::GetDefaultValue( PROJECTOR_TYPE* pType,
                                                  float* pPosX, float* pPosY, float* pPosZ,
												  float* pDirX, float* pDirY, float* pDirZ,
												  float* pUpX, float* pUpY, float* pUpZ,
                                                  float* pScaleU, float* pScaleV, float* pScaleW,
                                                  float* pProjectionAngle ) const
	{
        if (pType) *pType = m_pD->m_type;

        if (pPosX) *pPosX = m_pD->m_position[0];
        if (pPosY) *pPosY = m_pD->m_position[1];
        if (pPosZ) *pPosZ = m_pD->m_position[2];

        if (pDirX) *pDirX = m_pD->m_direction[0];
        if (pDirY) *pDirY = m_pD->m_direction[1];
        if (pDirZ) *pDirZ = m_pD->m_direction[2];

        if (pUpX) *pUpX = m_pD->m_up[0];
        if (pUpY) *pUpY = m_pD->m_up[1];
        if (pUpZ) *pUpZ = m_pD->m_up[2];

        if (pScaleU) *pScaleU = m_pD->m_scale[0];
        if (pScaleV) *pScaleV = m_pD->m_scale[1];
        if (pScaleW) *pScaleW = m_pD->m_scale[2];

        if (pProjectionAngle) *pProjectionAngle = m_pD->m_projectionAngle;
    }


	//---------------------------------------------------------------------------------------------
    void NodeProjectorParameter::SetDefaultValue( PROJECTOR_TYPE type,
                                                  float posX, float posY, float posZ,
												  float dirX, float dirY, float dirZ,
												  float upX, float upY, float upZ,
                                                  float scaleU, float scaleV, float scaleW,
                                                  float projectionAngle )
	{
        m_pD->m_type = type;
        m_pD->m_position = vec3<float>( posX, posY, posZ );
		m_pD->m_direction = vec3<float>( dirX, dirY, dirZ );
		m_pD->m_up = vec3<float>( upX, upY, upZ );
        m_pD->m_scale = vec3<float>( scaleU, scaleV, scaleW );
        m_pD->m_projectionAngle = projectionAngle;
    }


    //---------------------------------------------------------------------------------------------
    void NodeProjectorParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.SetNum(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeProjectorParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<m_pD->m_ranges.Num() );
        if ( i>=0 && i<m_pD->m_ranges.Num() )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }

}


