// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarCurve.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ParametersPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarCurvePrivate.h"

#include <memory>
#include <utility>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeScalarCurve::Private::s_type =
            NODE_TYPE( "Curve", NodeScalarCurve::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeScalarCurve, EType::Curve, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeScalarCurve::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeScalarCurve::GetInputNode( int i ) const
	{
		check( i >=0 && i < GetInputCount() );
        (void)i;

		return m_pD->m_input_scalar.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarCurve::SetInputNode( int i, NodePtr pNode )
	{
		check( i >=0 && i < GetInputCount());
        (void)i;

		m_pD->m_input_scalar = dynamic_cast<NodeScalar*>(pNode.get());
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    int NodeScalarCurve::GetKeyFrameCount() const
	{
		return (int)m_pD->m_curve.keyFrames.Num();
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarCurve::SetKeyFrameCount( int num )
	{
		check( num >=0 );
		m_pD->m_curve.keyFrames.SetNum( num );
	}

	//---------------------------------------------------------------------------------------------
    void NodeScalarCurve::SetDefaultValue(float defaultValue)
	{
		m_pD->m_curve.defaultValue = defaultValue;
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarCurve::SetKeyFrame(
            int index, float time, float value,
            float in_tangent, float in_tangent_weight,
            float out_tangent, float out_tangent_weight,
            uint8_t interp_mode, uint8_t tangent_mode, uint8_t tangent_weight_mode )
	{
		check( index >=0 && index < m_pD->m_curve.keyFrames.Num() );

		CurveKeyFrame& key_frame = m_pD->m_curve.keyFrames[index];

		key_frame.time = time;
		key_frame.value = value;
		key_frame.in_tangent = in_tangent;
		key_frame.in_tangent_weight = in_tangent_weight;
		key_frame.out_tangent = out_tangent;
		key_frame.out_tangent_weight = out_tangent_weight;
		
		key_frame.interp_mode = interp_mode;
		key_frame.tangent_mode = tangent_mode;
		key_frame.tangent_weight_mode = tangent_weight_mode;
	}


	//---------------------------------------------------------------------------------------------
    NodeScalarPtr NodeScalarCurve::GetT() const
	{
		return m_pD->m_input_scalar.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarCurve::SetT(NodeScalarPtr pNode)
	{
		m_pD->m_input_scalar = pNode;
	}

}

