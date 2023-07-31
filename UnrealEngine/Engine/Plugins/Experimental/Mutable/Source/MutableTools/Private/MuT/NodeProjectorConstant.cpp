// Copyright Epic Games, Inc. All Rights Reserved.


#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuR/Parameters.h"
#include "MuT/Node.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeProjectorPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeProjectorConstant::Private::s_type =
			NODE_TYPE( "ProjectorConstant", NodeProjector::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeProjectorConstant, EType::Constant, Node, Node::EType::Projector)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeProjectorConstant::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeProjectorConstant::GetInputNode( int ) const
	{
		check( false );
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeProjectorConstant::SetInputNode( int, NodePtr )
	{
		check( false );
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeProjectorConstant::GetValue( PROJECTOR_TYPE* pType,
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
    void NodeProjectorConstant::SetValue( PROJECTOR_TYPE type,
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


}


