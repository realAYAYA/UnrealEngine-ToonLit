// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"

namespace mu
{

	// Forward definitions
	class NodeImageLayer;
	typedef Ptr<NodeImageLayer> NodeImageLayerPtr;
	typedef Ptr<const NodeImageLayer> NodeImageLayerPtrConst;


	//! This node applies a layer blending effect on a base image using a mask and a blended image.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageLayer : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageLayer();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageLayer* pNode, OutputArchive& arch );
		static NodeImageLayerPtr StaticUnserialise( InputArchive& arch );


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

		//! Get the node generating the base image that will have the blendeing effect applied.
		NodeImagePtr GetBase() const;
		void SetBase( NodeImagePtr );

		//! Get the node generating the mask image controlling the weight of the effect.
		NodeImagePtr GetMask() const;
		void SetMask( NodeImagePtr );

		//! Get the image blended on top of the base.
		NodeImagePtr GetBlended() const;
		void SetBlended( NodeImagePtr );

		static const char* s_blendTypeName[int32(EBlendType::_BT_COUNT)];

		//!
		EBlendType GetBlendType() const;
		void SetBlendType(EBlendType);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageLayer();

	private:

		Private* m_pD;

	};


}
