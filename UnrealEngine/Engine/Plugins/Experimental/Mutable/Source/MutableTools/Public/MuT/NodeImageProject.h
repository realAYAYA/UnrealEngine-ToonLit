// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Math/UnrealMathSSE.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"

namespace mu
{

	// Forward definitions
	class NodeScalar;
	typedef Ptr<NodeScalar> NodeScalarPtr;
	typedef Ptr<const NodeScalar> NodeScalarPtrConst;

	class NodeColour;
	typedef Ptr<NodeColour> NodeColourPtr;
	typedef Ptr<const NodeColour> NodeColourPtrConst;

	class NodeMesh;
	typedef Ptr<NodeMesh> NodeMeshPtr;
	typedef Ptr<const NodeMesh> NodeMeshPtrConst;

	class NodeProjector;
	typedef Ptr<NodeProjector> NodeProjectorPtr;
	typedef Ptr<const NodeProjector> NodeProjectorPtrConst;

	class NodeImageProject;
	typedef Ptr<NodeImageProject> NodeImageProjectPtr;
	typedef Ptr<const NodeImageProject> NodeImageProjectPtrConst;

	class InputArchive;
	class OutputArchive;


	//!
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageProject : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageProject();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageProject* pNode, OutputArchive& arch );
		static NodeImageProjectPtr StaticUnserialise( InputArchive& arch );


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

		//! Get the node generating the mesh to be used for the projection.
		NodeMeshPtr GetMesh() const;
		void SetMesh( NodeMeshPtr );

		//! Get the node generating the projector to be used for the projection.
		NodeProjectorPtr GetProjector() const;
		void SetProjector( NodeProjectorPtr );

		//! Get the node generating a mask to filter out the projected images. It is optional.
		NodeImagePtr GetTargetMask() const;
		void SetTargetMask( NodeImagePtr );

        //! Get the node generating the threshold angle of the projection. In degrees.
		NodeScalarPtr GetAngleFadeStart() const;
		void SetAngleFadeStart( NodeScalarPtr );

        //! Get the node generating the threshold angle of the projection. In degrees.
		NodeScalarPtr GetAngleFadeEnd() const;
		void SetAngleFadeEnd( NodeScalarPtr );

        //! Get the node generating the image to project.
        NodeImagePtr GetImage() const;
        void SetImage( NodeImagePtr );

        //! UV layout of the mesh to use for the generated image. Defaults to 0.
        uint8_t GetLayout() const;
        void SetLayout( uint8_t  );

		//! Set the size of the image to generate with the projection. If set to 0 (default) a size
		//! that matches how this node is used will try to be guessed. 
		const FUintVector2& GetImageSize() const;
		void SetImageSize( const FUintVector2& size );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageProject();

	private:

		Private* m_pD;

	};


}
