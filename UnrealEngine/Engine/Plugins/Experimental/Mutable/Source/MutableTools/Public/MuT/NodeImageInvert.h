// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

	// Forward definitions
	class NodeImageInvert;
	typedef Ptr<NodeImageInvert> NodeImageInvertPtr;
	typedef Ptr<const NodeImageInvert> NodeImageInvertPtrConst;

	//! Node that inverts the colors of an image, channel by channel
	//! \ingroup model

	class MUTABLETOOLS_API NodeImageInvert : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageInvert();

		static void Serialise(const NodeImageInvert* pNode, OutputArchive& arch);
		void SerialiseWrapper(OutputArchive& arch) const override;
		static NodeImageInvertPtr StaticUnserialise(InputArchive& arch);


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Base image to invert.
		NodeImagePtr GetBase() const;
		void SetBase(NodeImagePtr);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
		Node::Private*GetBasePrivate()const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template
		~NodeImageInvert();

	private:

		Private* m_pD;
	};
}
