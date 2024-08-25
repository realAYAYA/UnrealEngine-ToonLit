// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"

namespace mu
{

	// Forward definitions
    class NodeScalarCurve;
    typedef Ptr<NodeScalarCurve> NodeScalarCurvePtr;
    typedef Ptr<const NodeScalarCurve> NodeScalarCurvePtrConst;


    //! This node makes a new scalar value transforming another scalar value with a curve.
	//! \ingroup model
    class MUTABLETOOLS_API NodeScalarCurve : public NodeScalar
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeScalarCurve();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeScalarCurve* pNode, OutputArchive& arch );
        static NodeScalarCurvePtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

        //! Get the number of key frames in the Curve.
		int GetKeyFrameCount() const;

        //! Set the number of key frames in the Curve.
		void SetKeyFrameCount( int );

		//! Set the default value of the curve
		void SetDefaultValue(float);

		//! \param index index of the curve, from 0 to GetKeyFrameCount()-1
        void SetKeyFrame(int index, float time, float value,
                         float in_tangent, float in_tangent_weight,
                         float out_tangent, float out_tangent_weight,
                         uint8_t interp_mode,
                         uint8_t tangent_mode, uint8_t tangent_weight_mode);

        //! Get the curve time parameter
		NodeScalarPtr GetT() const;
		void SetT(NodeScalarPtr);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeScalarCurve();

	private:

		Private* m_pD;

	};


}
