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
	FNodeType NodeProjectorConstant::Private::s_type =
			FNodeType( "ProjectorConstant", NodeProjector::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeProjectorConstant, EType::Constant, Node, Node::EType::Projector)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeProjectorConstant::GetValue( PROJECTOR_TYPE* OutType,
		FVector3f* OutPos,
		FVector3f* OutDir,
		FVector3f* OutUp,
		FVector3f* OutScale,
		float* OutProjectionAngle ) const
	{
        if (OutType) *OutType = m_pD->m_type;

        if (OutPos) *OutPos = m_pD->m_position;
        if (OutDir) *OutDir = m_pD->m_direction;
        if (OutUp) *OutUp = m_pD->m_up;
        if (OutScale) *OutScale = m_pD->m_scale;

        if (OutProjectionAngle) *OutProjectionAngle = m_pD->m_projectionAngle;
    }


	//---------------------------------------------------------------------------------------------
    void NodeProjectorConstant::SetValue( PROJECTOR_TYPE type, FVector3f pos, FVector3f dir, FVector3f up, FVector3f scale, float projectionAngle )
	{
        m_pD->m_type = type;
		m_pD->m_position = pos;
		m_pD->m_direction = dir;
		m_pD->m_up = up;
        m_pD->m_scale = scale;
        m_pD->m_projectionAngle = projectionAngle;
	}


}


