// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"

namespace mu
{

	// Forward definitions
	class NodeColour;
	typedef Ptr<NodeColour> NodeColourPtr;
	typedef Ptr<const NodeColour> NodeColourPtrConst;

	class NodeImageLayerColour;
	typedef Ptr<NodeImageLayerColour> NodeImageLayerColourPtr;
	typedef Ptr<const NodeImageLayerColour> NodeImageLayerColourPtrConst;

	class InputArchive;
	class OutputArchive;


	//! This node applies a layer blending effect on a base image using a mask and a colour.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageLayerColour : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageLayerColour();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageLayerColour* pNode, OutputArchive& arch );
		static NodeImageLayerColourPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        virtual int GetInputCount() const override;
        virtual Node* GetInputNode( int i ) const override;
        void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the node generating the base image that will have the blending effect applied.
		NodeImagePtr GetBase() const;
		void SetBase( NodeImagePtr );

		//! Get the node generating the mask image controlling the weight of the effect.
		NodeImagePtr GetMask() const;
		void SetMask( NodeImagePtr );

		//! Get the node generating the color to blend on the base.
		NodeColourPtr GetColour() const;
		void SetColour( NodeColourPtr );

		//!
		EBlendType GetBlendType() const;

		//!
		void SetBlendType(EBlendType);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageLayerColour();

	private:

		Private* m_pD;

	};


}
