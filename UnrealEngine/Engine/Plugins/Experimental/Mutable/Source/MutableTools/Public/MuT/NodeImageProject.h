// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Math/UnrealMathSSE.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"
#include "MuR/Image.h"

namespace mu
{

	// Forward definitions
	class NodeScalar;
	class NodeColour;
	class NodeMesh;
	class NodeProjector;
	class NodeImageProject;
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
		static Ptr<NodeImageProject> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the node generating the mesh to be used for the projection.
		void SetMesh( Ptr<NodeMesh> );

		//! Get the node generating the projector to be used for the projection.
		void SetProjector( Ptr<NodeProjector> );

		//! Get the node generating a mask to filter out the projected images. It is optional.
		void SetTargetMask( Ptr<NodeImage> );

		/** Eanble or disable the additional operations to correct texture UV seam artifact correction. Default is enabled.*/
		void SetEnableSeamCorrection(bool bEnabled);

		//! Set the angle-based fading behaviour for this projector. It must be set separately for 
		//! RGB and Alpha channels of the images to project. By default it is enabled for all channels.
		//! Disabling the angle fade will still use the AngleFadeEnd below to clamp the projection,
		//! but it will ignore the start angle, so it will not apply a fading gradient.
		void SetAngleFadeChannels( bool bFadeRGB, bool bFadeA );

        //! Set the node generating the fading start angle. Only relevant if fading is enabled with 
		void SetAngleFadeStart( Ptr<NodeScalar> );

		//! Set the node generating the fading end angle
		void SetAngleFadeEnd( Ptr<NodeScalar> );

		//! Set sampling method.
		void SetSamplingMethod(ESamplingMethod SamplingMethod);
		
		//! Set min filter method
		void SetMinFilterMethod(EMinFilterMethod MinFilterMethod);

        //! Get the node generating the image to project.
        void SetImage( Ptr<NodeImage> );

        //! UV layout of the mesh to use for the generated image. Defaults to 0.
        void SetLayout( uint8  );

		//! Set the size of the image to generate with the projection. If set to 0 (default) a size
		//! that matches how this node is used will try to be guessed. 
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
